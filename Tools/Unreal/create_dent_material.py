"""
Cria o material-pai do protótipo de amassado (M_DNDentParent) por script, sem abrir o editor.

Rodar SEM janela, pelo commandlet de Python do Unreal (testado no UE 5.8):

    UnrealEditor-Cmd.exe <projeto>.uproject -run=PythonScript ^
        -Script="<caminho>/create_dent_material.py --dest /Game/Sandbox_Teste" ^
        -unattended -nullrhi -nosplash -nosound

Existe um invólucro em PowerShell: Tools/Unreal/create_dent_material.ps1.

O QUE O MATERIAL FAZ (protótipo, lote 10; não é código final)
    Desloca vértices (World Position Offset) para dentro, ao longo da normal do vértice, perto de até 8 amassados.
    Cada amassado é descrito por TRÊS parâmetros, no espaço local do componente (cm):
        D<i>P = (X, Y, Z)             posição (vetor)
        D<i>R = raio                  escalar; raio <= 0 desliga o amassado
        D<i>D = profundidade          escalar
    (O raio não vai no canal alfa do vetor: o nó Custom recebe só RGB de um parâmetro vetorial.)
    O deslocamento de um vértice é  -normal x profundidade x f,  com  f = suavização de (1 - distância / raio).
    Também escurece um pouco a cor dentro do amassado (marca de batida). A normal NÃO é recalculada: a iluminação do
    amassado usa a normal original da malha (risco anotado em docs/prototipo-amassado.md).

    PINTURA (lote 16): o mesmo material é a pintura automotiva da carroceria. Modelo de sombreamento Clear Coat: verniz liso por cima
    (ClearCoat, ClearCoatRoughness) e base metálica com flocos por baixo (normal de flocos T_DNFlocos_N, gerada por este script, em
    FlakeTiling x UV, força FlakeStrength). Cor = Base Texture x Paint Tint. Os parâmetros Base Texture, MRA (oclusão no B), Normal,
    Paint Tint e MetalPaintMetallic têm os nomes do material do carro do template: o ADNVehicle copia os valores originais de cada
    slot (chassi e janelas continuam com as texturas deles) e só a pintura fica com o tom deste material (vermelho escuro metálico).
    Roughness é a aspereza da base. Tudo sai pelos atributos de material, o único caminho do Python 5.8 até o verniz.
    Rodar de novo refaz o material no mesmo pacote (sem apagar).

LIMITE DE SEGURANÇA: só grava pacotes em /Game/ do projeto que o abriu (qualquer outro destino é recusado antes de
qualquer escrita) e o relatório só pode ficar dentro da pasta do projeto.
"""

import sys

# O Python embutido da engine gravaria .pyc dentro da instalação do Unreal. Desliga ANTES de qualquer outro import.
sys.dont_write_bytecode = True

import argparse
import math
import os
import random
import re
import struct
import zlib

import unreal

DEST_PATTERN = re.compile(r"^/Game(/[A-Za-z0-9_]+)+$")
ASSET_NAME = "M_DNDentParent"
MAX_DENTS = 8

# Núcleo do deslocamento. Entradas: P (posição local, cm), N (normal do vértice no mundo) e os 24 parâmetros dos amassados.
# Saída: float4 (deslocamento xyz no mundo, máscara do amassado em w).
HLSL = """
float3 offset = float3(0, 0, 0);
float mask = 0;
float3 pos[8] = { D0P, D1P, D2P, D3P, D4P, D5P, D6P, D7P };
float rad[8] = { D0R, D1R, D2R, D3R, D4R, D5R, D6R, D7R };
float dep[8] = { D0D, D1D, D2D, D3D, D4D, D5D, D6D, D7D };
[unroll]
for (int i = 0; i < 8; i++)
{
    float r = rad[i];
    if (r > 0.001)
    {
        float d = distance(P, pos[i]);
        float f = saturate(1.0 - d / r);
        f = f * f * (3.0 - 2.0 * f);
        offset += -N * (dep[i] * f);
        mask = max(mask, f * saturate(dep[i] / 5.0));
    }
}
return float4(offset, mask);
"""


class DentError(RuntimeError):
    """Erro esperado, com mensagem para o usuário."""


def log(message):
    unreal.log("[create_dent_material] " + message)


def project_dir():
    return os.path.abspath(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))


def parse_args(argv):
    parser = argparse.ArgumentParser(prog="create_dent_material.py", add_help=False)
    parser.add_argument("--dest", required=True)
    parser.add_argument("--report", default=None)
    # atalho (16b): só refaz as instâncias e veste o carro, sem tocar no material-pai. Refazer o M_DNDentParent quebrou no 16b
    # (Assertion !IsRooted em delete_all_material_expressions) depois que a carroceria com alvos (lote 17) passou a ser carregada
    # pelo construtor do carro; enquanto a causa não é achada, mudança só de valores vai por aqui (docs/estado-atual.md)
    parser.add_argument("--instances-only", action="store_true")
    try:
        return parser.parse_args(argv)
    except SystemExit:
        raise DentError("Argumentos inválidos. Uso: --dest /Game/<pasta> [--report <arquivo>] [--instances-only]")


def resolve_report(path_arg):
    project = project_dir()
    path = os.path.abspath(path_arg) if path_arg else os.path.join(project, "Saved", "DentMaterialReport.txt")
    if not os.path.normcase(path).startswith(os.path.normcase(project) + os.sep):
        raise DentError("Relatório recusado: '%s' fica fora da pasta do projeto (%s)." % (path, project))
    return path


def node(mel, material, cls, x, y):
    return mel.create_material_expression(material, cls, x, y)


def scalar_param(mel, material, name, value, x, y):
    n = node(mel, material, unreal.MaterialExpressionScalarParameter, x, y)
    n.set_editor_property("parameter_name", name)
    n.set_editor_property("default_value", value)
    return n


def vector_param(mel, material, name, value, x, y):
    n = node(mel, material, unreal.MaterialExpressionVectorParameter, x, y)
    n.set_editor_property("parameter_name", name)
    n.set_editor_property("default_value", unreal.LinearColor(*value))
    return n


def write_flake_png(path, size=256, cell=2, tilt=0.35, seed=7):
    """Normal map de flocos metálicos (conteúdo próprio, gerado aqui): células de cell x cell px, cada uma com uma normal inclinada
    ao acaso até 'tilt' (~20 graus). PNG RGB 8 bits escrito só com a biblioteca padrão."""
    rng = random.Random(seed)
    cells = size // cell
    grid = []
    for _ in range(cells):
        row = []
        for _ in range(cells):
            nx, ny = rng.uniform(-tilt, tilt), rng.uniform(-tilt, tilt)
            nz = math.sqrt(max(0.0, 1.0 - nx * nx - ny * ny))
            row.append(bytes(int(round((c * 0.5 + 0.5) * 255.0)) for c in (nx, ny, nz)))
        grid.append(row)
    rows = [b"".join(grid[y // cell][x // cell] for x in range(size)) for y in range(size)]
    raw = b"".join(b"\x00" + r for r in rows)

    def chunk(kind, data):
        return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data) & 0xFFFFFFFF)

    png = b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", size, size, 8, 2, 0, 0, 0)) + chunk(b"IDAT", zlib.compress(raw, 9)) + chunk(b"IEND", b"")
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "wb") as handle:
        handle.write(png)


def import_flakes(dest, report):
    """Gera e importa T_DNFlocos_N em <dest> (normal map, sem sRGB)."""
    png = os.path.join(project_dir(), "Saved", "DentMaterial", "T_DNFlocos_N.png")
    write_flake_png(png)
    task = unreal.AssetImportTask()
    task.filename = png
    task.destination_path = dest
    task.destination_name = "T_DNFlocos_N"
    task.automated = True
    task.replace_existing = True
    task.save = False
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    tex = unreal.load_asset(dest + "/T_DNFlocos_N")
    if not isinstance(tex, unreal.Texture2D):
        raise DentError("Não consegui importar a textura de flocos em " + dest)
    tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP)
    tex.set_editor_property("srgb", False)
    unreal.EditorAssetLibrary.save_asset(dest + "/T_DNFlocos_N")
    report.append("textura de flocos gerada: " + dest + "/T_DNFlocos_N")
    return tex


def find_texture(name, fallback):
    """A textura do carro do template pelo nome (o caminho muda entre o projeto e o Sandbox); senão, a da engine."""
    for a in unreal.AssetRegistryHelpers.get_asset_registry().get_assets_by_path("/Game", recursive=True):
        if str(a.asset_name) == name:
            tex = unreal.load_asset(str(a.package_name))
            if isinstance(tex, unreal.Texture):
                return tex
    return unreal.load_asset(fallback)


def build(dest, report):
    if not DEST_PATTERN.match(dest):
        raise DentError("Destino recusado: '%s'. Só /Game/<pasta> do projeto é aceito." % dest)

    eal = unreal.EditorAssetLibrary
    path = dest + "/" + ASSET_NAME
    mel = unreal.MaterialEditingLibrary
    flakes = import_flakes(dest, report)
    if eal.does_asset_exist(path):
        # refaz no mesmo pacote: apagar no commandlet deixa o arquivo no disco e a criação seguinte falha
        material = unreal.load_asset(path)
        # o delete_all do 5.8 apaga só metade dos nós por chamada (medido no lote 17b: 85 -> 42). Os restos se acumulavam a cada
        # execução até haver duas saídas da normal de baixo do verniz, e aí o material não compila e o jogo mostra o padrão cinza
        # ("The material can contain only one ClearCoatBottomNormal node"). Chama até zerar.
        left = mel.get_num_material_expressions(material)
        for _ in range(16):
            if left == 0:
                break
            mel.delete_all_material_expressions(material)
            left = mel.get_num_material_expressions(material)
        if left:
            raise DentError("Sobraram %d nós no material depois de apagar tudo: %s" % (left, path))
        report.append("material existente refeito: " + path)
    else:
        material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(ASSET_NAME, dest, unreal.Material, unreal.MaterialFactoryNew())
    if material is None:
        raise DentError("Não consegui criar o material em " + dest)

    # Usado em malha estática e esquelética (os pedaços do carro são estáticos presos ao esqueleto).
    material.set_editor_property("used_with_skeletal_mesh", True)
    # Lote 17: a carroceria virou malha com esqueleto e alvos de deformação (SK_DNSportsCarBody); sem esta flag o jogo mostra o
    # material padrão (xadrez cinza) no lugar da pintura.
    material.set_editor_property("used_with_morph_targets", True)
    # A malha da carroceria é Nanite: sem esta flag a engine troca o material pelo padrão (aviso "missing usage flag Nanite").
    material.set_editor_property("used_with_nanite", True)
    # Nanite precisa saber o quanto o material pode deslocar um vértice (para não cortar o objeto por engano).
    try:
        material.set_editor_property("max_world_position_offset_displacement", 40.0)
    except Exception as exc:  # o nome da propriedade pode variar entre versões da engine: registra e segue
        unreal.log_warning("[create_dent_material] não consegui definir o deslocamento máximo do WPO: %r" % (exc,))

    # Pintura automotiva (lote 16): verniz (clear coat) liso por cima e base metálica com flocos por baixo. O verniz só entra pelos
    # atributos de material (o Python da 5.8 não expõe o pino de verniz direto no material).
    material.set_editor_property("use_material_attributes", True)
    material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_CLEAR_COAT)

    # --- parâmetros com os MESMOS nomes do material do carro do template (o C++ copia o visual de cada slot) ---
    def tex_param(name, texture, sampler, x, y):
        n = node(mel, material, unreal.MaterialExpressionTextureSampleParameter2D, x, y)
        n.set_editor_property("parameter_name", name)
        n.set_editor_property("texture", texture)
        # o tipo de amostragem tem que casar com a compressão da textura, senão o material não compila
        compression = texture.get_editor_property("compression_settings")
        if compression == unreal.TextureCompressionSettings.TC_NORMALMAP:
            sampler = unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL
        elif compression == unreal.TextureCompressionSettings.TC_MASKS:
            sampler = unreal.MaterialSamplerType.SAMPLERTYPE_MASKS
        elif sampler != unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL:
            sampler = unreal.MaterialSamplerType.SAMPLERTYPE_COLOR if texture.get_editor_property("srgb") else unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR
        n.set_editor_property("sampler_type", sampler)
        return n

    base_tex = tex_param("Base Texture", find_texture("T_SportsCar_Body_D", "/Engine/EngineResources/WhiteSquareTexture"),
                         unreal.MaterialSamplerType.SAMPLERTYPE_COLOR, -1700, -700)
    mra_tex = tex_param("MRA", find_texture("T_SportsCar_Body_MRA", "/Engine/EngineResources/WhiteSquareTexture"),
                        unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR, -1700, -450)
    body_n = tex_param("Normal", find_texture("T_SportsCar_Body_BotN", "/Engine/EngineMaterials/DefaultNormal"),
                       unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL, -1700, -200)
    tint = vector_param(mel, material, "Paint Tint", (0.16, 0.012, 0.012, 1.0), -1700, -850)  # vermelho escuro metálico
    metallic = scalar_param(mel, material, "MetalPaintMetallic", 0.7, -1400, -560)
    roughness = scalar_param(mel, material, "Roughness", 0.4, -1400, -500)
    coat = scalar_param(mel, material, "ClearCoat", 1.0, -1400, -440)
    coat_rough = scalar_param(mel, material, "ClearCoatRoughness", 0.04, -1400, -380)  # 16b: 0,03 a 0,05 (revisão do par)

    color = node(mel, material, unreal.MaterialExpressionMultiply, -1400, -750)
    mel.connect_material_expressions(base_tex, "RGB", color, "A")
    mel.connect_material_expressions(tint, "", color, "B")

    # flocos: normal procedural repetida pelo UV, misturada à normal da carroceria na camada de baixo do verniz
    uv = node(mel, material, unreal.MaterialExpressionTextureCoordinate, -1900, 50)
    tiling = scalar_param(mel, material, "FlakeTiling", 60.0, -1900, 150)  # 16b: flocos menores (30 granulava de perto)
    flake_uv = node(mel, material, unreal.MaterialExpressionMultiply, -1700, 80)
    mel.connect_material_expressions(uv, "", flake_uv, "A")
    mel.connect_material_expressions(tiling, "", flake_uv, "B")
    flake_n = node(mel, material, unreal.MaterialExpressionTextureSample, -1500, 80)
    flake_n.set_editor_property("texture", flakes)
    flake_n.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
    mel.connect_material_expressions(flake_uv, "", flake_n, "UVs")
    strength = scalar_param(mel, material, "FlakeStrength", 0.25, -1500, 250)  # 16b: 0,6 era forte demais (pintura parecia lixa)
    blend = node(mel, material, unreal.MaterialExpressionCustom, -1200, 0)
    blend.set_editor_property("code", "return normalize(float3(A.xy + B.xy * S, A.z));")
    blend.set_editor_property("description", "Flocos na normal")
    blend.set_editor_property("output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    blend_inputs = []
    for name in ("A", "B", "S"):
        ci = unreal.CustomInput()
        ci.set_editor_property("input_name", name)
        blend_inputs.append(ci)
    blend.set_editor_property("inputs", blend_inputs)
    mel.connect_material_expressions(body_n, "RGB", blend, "A")
    mel.connect_material_expressions(flake_n, "RGB", blend, "B")
    mel.connect_material_expressions(strength, "", blend, "S")
    bottom = node(mel, material, unreal.MaterialExpressionClearCoatNormalCustomOutput, -900, 0)
    mel.connect_material_expressions(blend, "", bottom, "")

    # --- posição local sem deslocamentos e normal do vértice ---
    world = node(mel, material, unreal.MaterialExpressionWorldPosition, -1400, 100)
    world.set_editor_property("world_position_shader_offset", unreal.WorldPositionIncludedOffsets.WPT_EXCLUDE_ALL_SHADER_OFFSETS)
    local = node(mel, material, unreal.MaterialExpressionTransformPosition, -1150, 100)
    local.set_editor_property("transform_source_type", unreal.MaterialPositionTransformSource.TRANSFORMPOSSOURCE_WORLD)
    local.set_editor_property("transform_type", unreal.MaterialPositionTransformSource.TRANSFORMPOSSOURCE_LOCAL)
    mel.connect_material_expressions(world, "", local, "")
    normal = node(mel, material, unreal.MaterialExpressionVertexNormalWS, -1150, 250)

    # --- nó Custom com o laço dos 8 amassados ---
    custom = node(mel, material, unreal.MaterialExpressionCustom, -700, 300)
    custom.set_editor_property("code", HLSL)
    custom.set_editor_property("description", "Amassados (WPO)")
    custom.set_editor_property("output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT4)
    input_names = ["P", "N"] + ["D%dP" % i for i in range(MAX_DENTS)] + ["D%dR" % i for i in range(MAX_DENTS)] + ["D%dD" % i for i in range(MAX_DENTS)]
    inputs = []
    for name in input_names:
        ci = unreal.CustomInput()
        ci.set_editor_property("input_name", name)
        inputs.append(ci)
    custom.set_editor_property("inputs", inputs)

    mel.connect_material_expressions(local, "", custom, "P")
    mel.connect_material_expressions(normal, "", custom, "N")
    for i in range(MAX_DENTS):
        pos = vector_param(mel, material, "D%dP" % i, (0.0, 0.0, 0.0, 0.0), -1500, 500 + i * 130)
        rad = scalar_param(mel, material, "D%dR" % i, 0.0, -1250, 500 + i * 130)
        dep = scalar_param(mel, material, "D%dD" % i, 0.0, -1000, 500 + i * 130)
        mel.connect_material_expressions(pos, "", custom, "D%dP" % i)
        mel.connect_material_expressions(rad, "", custom, "D%dR" % i)
        mel.connect_material_expressions(dep, "", custom, "D%dD" % i)

    # --- saídas: deslocamento -> WPO; máscara -> escurece a cor ---
    offset = node(mel, material, unreal.MaterialExpressionComponentMask, -400, 300)
    offset.set_editor_property("r", True)
    offset.set_editor_property("g", True)
    offset.set_editor_property("b", True)
    offset.set_editor_property("a", False)
    mel.connect_material_expressions(custom, "", offset, "")

    mask = node(mel, material, unreal.MaterialExpressionComponentMask, -400, 450)
    mask.set_editor_property("r", False)
    mask.set_editor_property("g", False)
    mask.set_editor_property("b", False)
    mask.set_editor_property("a", True)
    mel.connect_material_expressions(custom, "", mask, "")

    darkening = scalar_param(mel, material, "DentDarkening", 0.6, -400, 600)
    scaled = node(mel, material, unreal.MaterialExpressionMultiply, -150, 500)
    mel.connect_material_expressions(mask, "", scaled, "A")
    mel.connect_material_expressions(darkening, "", scaled, "B")
    keep = node(mel, material, unreal.MaterialExpressionOneMinus, 50, 500)
    mel.connect_material_expressions(scaled, "", keep, "")
    painted = node(mel, material, unreal.MaterialExpressionMultiply, 250, 0)
    mel.connect_material_expressions(color, "", painted, "A")
    mel.connect_material_expressions(keep, "", painted, "B")

    # --- tudo pelos atributos de material (é o único caminho do Python para o verniz) ---
    attrs = node(mel, material, unreal.MaterialExpressionMakeMaterialAttributes, 550, 0)
    for src, out, pin in ((painted, "", "BaseColor"), (metallic, "", "Metallic"), (roughness, "", "Roughness"), (body_n, "RGB", "Normal"),
                          (offset, "", "WorldPositionOffset"), (coat, "", "ClearCoat"), (coat_rough, "", "ClearCoatRoughness"),
                          (mra_tex, "B", "AmbientOcclusion")):
        if not mel.connect_material_expressions(src, out, attrs, pin):
            raise DentError("Ligação de material falhou: %s -> %s" % (src.get_name(), pin))
    mel.connect_material_property(attrs, "", unreal.MaterialProperty.MP_MATERIAL_ATTRIBUTES)

    mel.recompile_material(material)
    if not eal.save_asset(path):
        raise DentError("Não consegui salvar " + path)
    report.append("material criado: " + path)
    report.append("parâmetros: Paint Tint, Base Texture, MRA, Normal, MetalPaintMetallic, Roughness, ClearCoat, ClearCoatRoughness, FlakeTiling, "
                  "FlakeStrength, DentDarkening, D0P..D7P, D0R..D7R, D0D..D7D")
    dress_car(dest, material, report)


def find_asset(name, cls):
    for a in unreal.AssetRegistryHelpers.get_asset_registry().get_assets_by_path("/Game", recursive=True):
        if str(a.asset_name) == name:
            asset = unreal.load_asset(str(a.package_name))
            if isinstance(asset, cls):
                return asset
    return None


def make_instance(dest, name, parent, scalars=None, vectors=None, textures=None):
    """Instância de material em <dest>, refeita no mesmo pacote se já existe."""
    mel = unreal.MaterialEditingLibrary
    path = dest + "/" + name
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        mi = unreal.load_asset(path)
    else:
        mi = unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, dest, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    mel.set_material_instance_parent(mi, parent)
    mel.clear_all_material_instance_parameters(mi)
    for k, v in (scalars or {}).items():
        mel.set_material_instance_scalar_parameter_value(mi, k, v)
    for k, v in (vectors or {}).items():
        mel.set_material_instance_vector_parameter_value(mi, k, unreal.LinearColor(v[0], v[1], v[2], 1.0))
    for k, v in (textures or {}).items():
        if v is not None:
            mel.set_material_instance_texture_parameter_value(mi, k, v)
    mel.update_material_instance(mi)
    unreal.EditorAssetLibrary.save_asset(path)
    return mi


def dress_car(dest, paint, report):
    """Veste o carro do template: pintura e chassi/janelas na carroceria, roda e pneu na roda (lote 16). Só mexe nas malhas que achar."""
    body = find_asset("SM_SportsCar", unreal.StaticMesh)
    if body is not None:
        # 16b: os valores da pintura ficam explícitos na instância (não dependem do padrão do material-pai): flocos mais finos e
        # mais fracos, verniz na faixa 0,03 a 0,05 pedida na revisão
        pintura = make_instance(dest, "MI_DNPintura", paint, {"FlakeStrength": 0.25, "FlakeTiling": 60.0, "ClearCoatRoughness": 0.04})
        # chassi, grade e janelas: as texturas do chassi do template, verniz bem liso e sem flocos (vidro fumê opaco: o carro não tem interior)
        chassi = make_instance(dest, "MI_DNChassi", paint, {"MetalPaintMetallic": 1.0, "FlakeStrength": 0.0, "ClearCoatRoughness": 0.03, "Roughness": 0.3},
                               {"Paint Tint": (1.0, 0.992, 0.99)},
                               {"Base Texture": find_texture("T_SportsCar_Chasis_D", "/Engine/EngineResources/WhiteSquareTexture"),
                                "MRA": find_texture("T_SportsCar_Chasis_MRA", "/Engine/EngineResources/WhiteSquareTexture"),
                                "Normal": find_texture("T_SportsCar_Chasis_N", "/Engine/EngineMaterials/DefaultNormal")})
        slots = [str(s.material_slot_name) for s in body.get_editor_property("static_materials")]
        for i, slot in enumerate(slots):
            body.set_material(i, pintura if "Body" in slot else chassi)
        unreal.EditorAssetLibrary.save_asset(body.get_path_name().split(".")[0])
        report.append("carroceria %s: slots %s -> pintura no 'Body', chassi nos outros" % (body.get_path_name(), slots))
    wheel = find_asset("SM_SportsCar_Wheel", unreal.StaticMesh)
    wheel_parent = find_asset("M_SportsCar_Wheel", unreal.Material)
    if wheel is not None and wheel_parent is not None:
        roda = make_instance(dest, "MI_DNRoda", wheel_parent, {"Metallic1": 1.0, "Roughness1": 0.22, "Metallic2": 1.0, "Roughness2": 0.12},
                             {"BaseColor1": (0.55, 0.56, 0.58), "BaseColor2": (0.9, 0.9, 0.9)})
        pneu = make_instance(dest, "MI_DNPneu", wheel_parent, {"Metallic1": 0.0, "Roughness1": 0.88, "Metallic2": 0.0, "Roughness2": 0.75},
                             {"BaseColor1": (0.022, 0.022, 0.022), "BaseColor2": (0.09, 0.09, 0.09)},
                             {"Normal": find_texture("T_SportsCar_Tire_N", "/Engine/EngineMaterials/DefaultNormal"),
                              "Masks": find_texture("T_SportsCar_Tire_M_TGA", "/Engine/EngineResources/WhiteSquareTexture")})
        slots = [str(s.material_slot_name) for s in wheel.get_editor_property("static_materials")]
        for i, slot in enumerate(slots):
            wheel.set_material(i, pneu if "Tire" in slot else roda)
        unreal.EditorAssetLibrary.save_asset(wheel.get_path_name().split(".")[0])
        report.append("roda %s: slots %s -> pneu no 'Tire', aro nos outros" % (wheel.get_path_name(), slots))


def main():
    report = []
    error = None
    report_path = None
    try:
        args = parse_args(sys.argv[1:])
        report_path = resolve_report(args.report)
        if args.instances_only:
            if not DEST_PATTERN.match(args.dest):
                raise DentError("Destino recusado: '%s'. Só /Game/<pasta> do projeto é aceito." % args.dest)
            parent = unreal.load_asset(args.dest + "/" + ASSET_NAME)
            if not isinstance(parent, unreal.Material):
                raise DentError("--instances-only precisa do material-pai já criado em " + args.dest)
            dress_car(args.dest, parent, report)
        else:
            build(args.dest, report)
    except DentError as exc:
        error = str(exc)
    except Exception as exc:  # qualquer outro erro também derruba o processo, com mensagem
        error = "Erro inesperado: %r" % (exc,)

    if error:
        report.append("ERRO: " + error)
        unreal.log_error("[create_dent_material] " + error)

    if report_path:
        os.makedirs(os.path.dirname(report_path), exist_ok=True)
        with open(report_path, "w", encoding="utf-8") as handle:
            handle.write("\n".join(report) + "\n")
        log("relatório: " + report_path)
    for line in report:
        log(line)

    if error:
        raise RuntimeError(error)


main()
