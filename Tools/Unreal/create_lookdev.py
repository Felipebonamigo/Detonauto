"""
Cria a cena de aparência (lookdev) do Detonauto por script: o mapa /Game/Detonauto/Maps/L_Lookdev, os materiais da rua e dos
prédios e as texturas CC0 importadas. Lote 15, docs/plano-visual.md ("Cena de aparência").

Rodar SEM janela, pelo commandlet de Python do Unreal (invólucro: Tools/Unreal/create_lookdev.ps1):

    UnrealEditor-Cmd.exe Detonauto.uproject -run=PythonScript ^
        -Script="<caminho>/create_lookdev.py --textures <pasta com as texturas>" -unattended -nullrhi -nosplash -nosound

O QUE A CENA TEM
    Luz: Sky Atmosphere, sol de fim de tarde (15 graus acima do horizonte, quase na direção da rua), nuvens volumétricas, Sky Light em
    tempo real e neblina de altura leve. Pós-processo com exposição de câmera física (abertura, obturador e ISO).
    Rua de 1,2 km ao longo de X: duas faixas de 3,5 m (asfalto), faixa central amarela tracejada, linhas de bordo brancas, meio-fio de
    15 cm e calçadas de 3 m; prédios em blocos simples dos dois lados, com faixas de vidro por andar. Os prédios do City Sample não
    estão no PC: os blocos ficam no lugar deles até a migração (docs/estado-atual.md).
    PlayerStart com a tag DNMapStart: o ADNVehicleTestGameMode usa esse ponto e não cria a arena de teste.

TEXTURAS (CC0, Poly Haven; uma linha por conjunto em docs/licencas.csv)
    A pasta --textures tem uma subpasta por conjunto com <id>_diff_2k.jpg, <id>_nor_dx_2k.jpg e <id>_arm_2k.jpg (ARM: oclusão no R,
    aspereza no G, metal no B). Os .jpg ficam fora do Git (Sandbox/); entram no projeto só os .uasset importados.

Projeção das texturas: pelas coordenadas do mundo (o eixo dominante da normal escolhe o plano), com o tamanho real de cada textura
em cm. Assim os blocos esticados não esticam a textura. Atalho deliberado: as normais das faces verticais ficam com a base do
tangente da malha, não da projeção; serve para a cena de aparência, não para arte final.

LIMITE DE SEGURANÇA: só grava pacotes em /Game/ do projeto que o abriu e o relatório só pode ficar dentro da pasta do projeto.
Sempre REFAZ o conteúdo: tira todos os atores do mapa e todos os nós dos materiais-pai e monta de novo, nos mesmos pacotes (nada é
apagado do disco). O resultado tem que sair do script, não de uma edição à mão.
"""

import sys

# O Python embutido da engine gravaria .pyc dentro da instalação do Unreal. Desliga ANTES de qualquer outro import.
sys.dont_write_bytecode = True

import argparse
import os
import random
import re
import struct
import zlib

import unreal

GAME_PATH = re.compile(r"^/Game(/[A-Za-z0-9_]+)+$")
MAP_PATH = "/Game/Detonauto/Maps/L_Lookdev"
ROOT = "/Game/Detonauto/Lookdev"
TEX_DIR = ROOT + "/Textures"
MAT_DIR = ROOT + "/Materials"
CUBE = "/Engine/BasicShapes/Cube.Cube"
CLOUD_MATERIAL = "/Engine/EngineSky/VolumetricClouds/m_SimpleVolumetricCloud_Inst.m_SimpleVolumetricCloud_Inst"

# (nome no projeto, id do Poly Haven, lado real da textura em cm)
SETS = [
    ("Asfalto", "asphalt_02", 300.0),
    ("Calcada", "concrete_pavement", 180.0),
    ("MeioFio", "brushed_concrete", 250.0),
    ("Fachada", "clay_plaster", 200.0),
]
# tons das fachadas de reboco (multiplicam a cor da textura): gelo, areia, terracota clara, cinza azulado; a quinta é de concreto
FACADE_TINTS = [(1.0, 1.0, 1.0), (0.95, 0.85, 0.68), (0.92, 0.66, 0.52), (0.72, 0.78, 0.84)]
FLOOR_HEIGHTS = (300, 350)   # pé-direito dos andares [cm]
GROUND_FLOOR_CM = 450.0      # térreo com vitrine
# trechos de prédios baixos e recuados (x inicial, x final, lado), para o sol de fim de tarde entrar na calçada
LOW_ZONES = [(-9000.0, -3000.0, -1), (6000.0, 12000.0, 1), (20000.0, 26000.0, -1), (-30000.0, -24000.0, 1)]

# geometria da rua [cm]
ROAD_X0, ROAD_X1 = -56000.0, 64000.0
LANE_CM = 350.0
CURB_W, CURB_H = 20.0, 15.0
SIDEWALK_W = 300.0
ROAD_TOP = 2.0
SIDEWALK_TOP = ROAD_TOP + CURB_H
START = (-12000.0, LANE_CM * 0.5, 60.0)  # faixa da direita, de frente para +X

# luz e câmera
# 15 graus acima do horizonte, quase no eixo da rua (a luz anda para -X; o sol fica no fim da rua, 2 graus à direita). Mais que uns 3 graus
# de lado e os prédios fazem sombra na rua inteira: com o sol tão baixo, a luz só chega ao asfalto correndo ao longo da rua.
SUN_PITCH, SUN_YAW = -15.0, 182.0
SUN_LUX = 100000.0                      # iluminância antes da atmosfera; a Sky Atmosphere atenua no fim de tarde
CAMERA_FSTOP, CAMERA_SHUTTER, CAMERA_ISO = 8.0, 180.0, 100.0  # EV100 = log2(8² x 180) = 13,5 (13 estourava a frente do carro; 14 fechava as sombras)


class LookdevError(RuntimeError):
    """Erro esperado, com mensagem para o usuário."""


def log(message):
    unreal.log("[create_lookdev] " + message)


def project_dir():
    return os.path.abspath(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))


def parse_args(argv):
    parser = argparse.ArgumentParser(prog="create_lookdev.py", add_help=False)
    parser.add_argument("--textures", required=True)
    parser.add_argument("--models", default=None)
    parser.add_argument("--report", default=None)
    try:
        return parser.parse_args(argv)
    except SystemExit:
        raise LookdevError("Argumentos inválidos. Uso: --textures <pasta> [--report <arquivo>]")


def resolve_report(path_arg):
    project = project_dir()
    path = os.path.abspath(path_arg) if path_arg else os.path.join(project, "Saved", "LookdevReport.txt")
    if not os.path.normcase(path).startswith(os.path.normcase(project) + os.sep):
        raise LookdevError("Relatório recusado: '%s' fica fora da pasta do projeto (%s)." % (path, project))
    return path


def check_game_path(path):
    if not GAME_PATH.match(path):
        raise LookdevError("Destino recusado: '%s'. Só /Game/<pasta> do projeto é aceito." % path)


# --- texturas ---

def import_textures(source_root, report):
    check_game_path(TEX_DIR)
    tasks = []
    wanted = []
    for name, ph_id, _ in SETS:
        for suffix, kind in (("diff", "D"), ("nor_dx", "N"), ("arm", "ARM")):
            path = os.path.join(source_root, ph_id, "%s_%s_2k.jpg" % (ph_id, suffix))
            if not os.path.isfile(path):
                raise LookdevError("Textura não encontrada: " + path)
            task = unreal.AssetImportTask()
            task.filename = path
            task.destination_path = TEX_DIR
            task.destination_name = "T_%s_%s" % (name, kind)
            task.automated = True
            task.replace_existing = True
            task.save = False
            tasks.append(task)
            wanted.append((TEX_DIR + "/T_%s_%s" % (name, kind), kind))
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)
    textures = {}
    for path, kind in wanted:
        tex = unreal.load_asset(path)
        if not isinstance(tex, unreal.Texture2D):
            raise LookdevError("A importação não gerou a textura " + path)
        if kind == "N":
            tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP)
            tex.set_editor_property("srgb", False)
        elif kind == "ARM":
            tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_MASKS)
            tex.set_editor_property("srgb", False)
        unreal.EditorAssetLibrary.save_asset(path)
        textures[path] = tex
    report.append("texturas importadas: %d em %s" % (len(textures), TEX_DIR))
    return textures


# --- materiais ---

def recreate(path, asset_class, factory):
    """Cria o asset ou, se já existe, reaproveita o mesmo pacote: material-pai perde todos os nós e é refeito; instância é regravada.
    Nada é apagado do disco (apagar no commandlet deixava o arquivo e quebrava a criação seguinte)."""
    check_game_path(path.rsplit("/", 1)[0])
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        asset = unreal.load_asset(path)
        if not isinstance(asset, asset_class):
            raise LookdevError("Já existe um asset de outro tipo em " + path)
        if isinstance(asset, unreal.Material):
            unreal.MaterialEditingLibrary.delete_all_material_expressions(asset)
        return asset
    folder, name = path.rsplit("/", 1)
    asset = unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, folder, asset_class, factory)
    if asset is None:
        raise LookdevError("Não consegui criar " + path)
    return asset


def expr(material, cls, x, y):
    return unreal.MaterialEditingLibrary.create_material_expression(material, cls, x, y)


def connect(a, a_out, b, b_in):
    if not unreal.MaterialEditingLibrary.connect_material_expressions(a, a_out, b, b_in):
        raise LookdevError("Ligação de material falhou: %s.%s -> %s.%s" % (a.get_name(), a_out, b.get_name(), b_in))


def mask(material, source, channels, x, y):
    m = expr(material, unreal.MaterialExpressionComponentMask, x, y)
    for c in "rgba":
        m.set_editor_property(c, c in channels)
    connect(source, "", m, "")
    return m


def build_surface_master(textures, report):
    """Material-pai da rua e dos prédios: cor, normal e ARM projetados pelo mundo, com o tamanho real (Tile, cm) e um tom (Tint)."""
    path = MAT_DIR + "/M_DNLookdevSuperficie"
    mel = unreal.MaterialEditingLibrary
    m = recreate(path, unreal.Material, unreal.MaterialFactoryNew())

    world = expr(m, unreal.MaterialExpressionWorldPosition, -1800, 0)
    normal = expr(m, unreal.MaterialExpressionVertexNormalWS, -1800, 300)
    absn = expr(m, unreal.MaterialExpressionAbs, -1600, 300)
    connect(normal, "", absn, "")
    nx, ny, nz = (mask(m, absn, c, -1400, 250 + i * 80) for i, c in enumerate("rgb"))
    xy, yz, xz = (mask(m, world, c, -1400, -150 + i * 80) for i, c in enumerate(("rg", "gb", "rb")))

    # parede: normal mais em X usa o plano YZ; mais em Y usa XZ
    wall = expr(m, unreal.MaterialExpressionIf, -1150, 100)
    connect(nx, "", wall, "A")
    connect(ny, "", wall, "B")
    connect(yz, "", wall, "A > B")
    connect(yz, "", wall, "A == B")
    connect(xz, "", wall, "A < B")
    # chão: normal mais em Z que 0,5 usa o plano XY
    half = expr(m, unreal.MaterialExpressionConstant, -1150, 350)
    half.set_editor_property("r", 0.5)
    uv = expr(m, unreal.MaterialExpressionIf, -950, 0)
    connect(nz, "", uv, "A")
    connect(half, "", uv, "B")
    connect(xy, "", uv, "A > B")
    connect(xy, "", uv, "A == B")
    connect(wall, "", uv, "A < B")

    tile = expr(m, unreal.MaterialExpressionScalarParameter, -950, 250)
    tile.set_editor_property("parameter_name", "Tile")
    tile.set_editor_property("default_value", 300.0)
    scaled = expr(m, unreal.MaterialExpressionDivide, -750, 50)
    connect(uv, "", scaled, "A")
    connect(tile, "", scaled, "B")

    def sampler(name, tex_path, sampler_type, y):
        s = expr(m, unreal.MaterialExpressionTextureSampleParameter2D, -500, y)
        s.set_editor_property("parameter_name", name)
        s.set_editor_property("texture", textures[tex_path])
        s.set_editor_property("sampler_type", sampler_type)
        connect(scaled, "", s, "UVs")
        return s

    color = sampler("Cor", TEX_DIR + "/T_Asfalto_D", unreal.MaterialSamplerType.SAMPLERTYPE_COLOR, -300)
    nmap = sampler("Normal", TEX_DIR + "/T_Asfalto_N", unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL, 0)
    arm = sampler("ARM", TEX_DIR + "/T_Asfalto_ARM", unreal.MaterialSamplerType.SAMPLERTYPE_MASKS, 300)

    tint = expr(m, unreal.MaterialExpressionVectorParameter, -500, -500)
    tint.set_editor_property("parameter_name", "Tint")
    tint.set_editor_property("default_value", unreal.LinearColor(1.0, 1.0, 1.0, 1.0))
    tinted = expr(m, unreal.MaterialExpressionMultiply, -200, -350)
    connect(color, "RGB", tinted, "A")
    connect(tint, "", tinted, "B")

    mel.connect_material_property(tinted, "", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(nmap, "RGB", unreal.MaterialProperty.MP_NORMAL)
    mel.connect_material_property(arm, "R", unreal.MaterialProperty.MP_AMBIENT_OCCLUSION)
    mel.connect_material_property(arm, "G", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.connect_material_property(arm, "B", unreal.MaterialProperty.MP_METALLIC)
    mel.recompile_material(m)
    unreal.EditorAssetLibrary.save_asset(path)
    report.append("material criado: " + path)
    return m


def build_flat_master(report):
    """Material-pai liso (tinta da faixa e vidro): cor, aspereza e metal."""
    path = MAT_DIR + "/M_DNLookdevLiso"
    mel = unreal.MaterialEditingLibrary
    m = recreate(path, unreal.Material, unreal.MaterialFactoryNew())
    color = expr(m, unreal.MaterialExpressionVectorParameter, -500, -200)
    color.set_editor_property("parameter_name", "Cor")
    color.set_editor_property("default_value", unreal.LinearColor(0.8, 0.8, 0.8, 1.0))
    rough = expr(m, unreal.MaterialExpressionScalarParameter, -500, 0)
    rough.set_editor_property("parameter_name", "Aspereza")
    rough.set_editor_property("default_value", 0.5)
    metal = expr(m, unreal.MaterialExpressionScalarParameter, -500, 150)
    metal.set_editor_property("parameter_name", "Metal")
    metal.set_editor_property("default_value", 0.0)
    mel.connect_material_property(color, "", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.connect_material_property(metal, "", unreal.MaterialProperty.MP_METALLIC)
    mel.recompile_material(m)
    unreal.EditorAssetLibrary.save_asset(path)
    report.append("material criado: " + path)
    return m


# Interior mapping (salas falsas por paralaxe), procedural, sem textura. Entradas: WP posição no mundo [cm], V vetor para a câmera,
# N normal da fachada, Room tamanho da sala [cm], Off origem das salas [cm], Pane largura do painel de vidro [cm].
# Saída: rgb = cor da sala vista pelo vidro, a = máscara do caixilho (1 = montante).
INTERIOR_HLSL = """
float3 rs = max(Room, 1.0);
float3 n = normalize(N);
float3 p = (WP - n * 2.0 - Off) / rs;
float3 cell = floor(p);
float3 l = p - cell;
float3 d = -V / rs;
d = sign(d) * max(abs(d), 1e-5);
float3 t = (step(0.0, d) - l) / d;
float tm = min(min(t.x, t.y), t.z);
float r = frac(sin(dot(cell, float3(12.9898, 78.233, 37.719))) * 43758.5453);
float lit = r > 0.5 ? 1.0 : 0.12;
float3 wall = lerp(float3(0.42, 0.36, 0.28), float3(0.3, 0.33, 0.38), frac(r * 7.13));
float3 col;
if (tm == t.z)
{
    col = d.z < 0.0 ? float3(0.28, 0.19, 0.12) : float3(0.8, 0.79, 0.75);
}
else if ((tm == t.x && abs(n.x) > 0.5) || (tm == t.y && abs(n.y) > 0.5))
{
    col = wall * 0.75;
}
else
{
    col = wall;
}
float depth = saturate(tm * length(d * rs) / 700.0);
col *= lerp(1.0, 0.45, depth) * lit;
float u = abs(n.x) > 0.5 ? WP.y : WP.x;
float m = frac(u / max(Pane, 1.0));
float frame = (m < 0.025 || m > 0.975) ? 1.0 : 0.0;
return float4(col, frame);
"""


def build_window_master(report):
    """Vidro dos prédios: reflexo (vidro liso), salas falsas por interior mapping e caixilho de alumínio."""
    path = MAT_DIR + "/M_DNLookdevJanela"
    mel = unreal.MaterialEditingLibrary
    m = recreate(path, unreal.Material, unreal.MaterialFactoryNew())

    def param(cls, name, value, x, y):
        p = expr(m, cls, x, y)
        p.set_editor_property("parameter_name", name)
        p.set_editor_property("default_value", value)
        return p

    world = expr(m, unreal.MaterialExpressionWorldPosition, -1400, -200)
    cam = expr(m, unreal.MaterialExpressionCameraVectorWS, -1400, -100)
    normal = expr(m, unreal.MaterialExpressionVertexNormalWS, -1400, 0)
    room = param(unreal.MaterialExpressionVectorParameter, "RoomSizeCm", unreal.LinearColor(450.0, 450.0, 300.0, 0.0), -1400, 100)
    off = param(unreal.MaterialExpressionVectorParameter, "RoomOffsetCm", unreal.LinearColor(0.0, 0.0, 0.0, 0.0), -1400, 250)
    pane = param(unreal.MaterialExpressionScalarParameter, "PaneWidthCm", 150.0, -1400, 400)

    custom = expr(m, unreal.MaterialExpressionCustom, -1000, 0)
    custom.set_editor_property("code", INTERIOR_HLSL)
    custom.set_editor_property("description", "Salas falsas (interior mapping)")
    custom.set_editor_property("output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT4)
    inputs = []
    for name in ("WP", "V", "N", "Room", "Off", "Pane"):
        ci = unreal.CustomInput()
        ci.set_editor_property("input_name", name)
        inputs.append(ci)
    custom.set_editor_property("inputs", inputs)
    for src, pin in ((world, "WP"), (cam, "V"), (normal, "N"), (room, "Room"), (off, "Off"), (pane, "Pane")):
        connect(src, "", custom, pin)

    interior = mask(m, custom, "rgb", -750, -100)
    frame = mask(m, custom, "a", -750, 100)

    glass = param(unreal.MaterialExpressionVectorParameter, "CorVidro", unreal.LinearColor(0.01, 0.012, 0.015, 1.0), -750, -400)
    frame_col = param(unreal.MaterialExpressionVectorParameter, "CorCaixilho", unreal.LinearColor(0.09, 0.09, 0.095, 1.0), -750, -300)
    base = expr(m, unreal.MaterialExpressionLinearInterpolate, -450, -350)
    connect(glass, "", base, "A")
    connect(frame_col, "", base, "B")
    connect(frame, "", base, "Alpha")
    rough = expr(m, unreal.MaterialExpressionLinearInterpolate, -450, -200)
    rough.set_editor_property("const_a", 0.04)
    rough.set_editor_property("const_b", 0.45)
    connect(frame, "", rough, "Alpha")
    metal = expr(m, unreal.MaterialExpressionLinearInterpolate, -450, -50)
    metal.set_editor_property("const_a", 0.0)
    metal.set_editor_property("const_b", 0.8)
    connect(frame, "", metal, "Alpha")

    # sala acesa: emissivo atrás do vidro, mais fraco no ângulo rasante (onde o vidro só reflete)
    fresnel = expr(m, unreal.MaterialExpressionFresnel, -750, 300)
    see = expr(m, unreal.MaterialExpressionOneMinus, -600, 300)
    connect(fresnel, "", see, "")
    no_frame = expr(m, unreal.MaterialExpressionOneMinus, -600, 150)
    connect(frame, "", no_frame, "")
    # nits: com a exposição de dia (EV 13,5) uns 2500 nits viram cinza médio; a sala acesa fica bem abaixo da rua ensolarada
    bright = param(unreal.MaterialExpressionScalarParameter, "BrilhoInterior", 1500.0, -600, 450)  # 4000 deixava as salas brancas
    e1 = expr(m, unreal.MaterialExpressionMultiply, -400, 100)
    connect(interior, "", e1, "A")
    connect(no_frame, "", e1, "B")
    e2 = expr(m, unreal.MaterialExpressionMultiply, -250, 200)
    connect(e1, "", e2, "A")
    connect(see, "", e2, "B")
    e3 = expr(m, unreal.MaterialExpressionMultiply, -100, 250)
    connect(e2, "", e3, "A")
    connect(bright, "", e3, "B")

    mel.connect_material_property(base, "", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.connect_material_property(metal, "", unreal.MaterialProperty.MP_METALLIC)
    mel.connect_material_property(e3, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mel.recompile_material(m)
    unreal.EditorAssetLibrary.save_asset(path)
    report.append("material criado: " + path)
    return m


def instance(parent, name, scalars=None, vectors=None, textures=None):
    path = MAT_DIR + "/" + name
    mel = unreal.MaterialEditingLibrary
    mi = recreate(path, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    mel.set_material_instance_parent(mi, parent)
    for k, v in (scalars or {}).items():
        mel.set_material_instance_scalar_parameter_value(mi, k, v)
    for k, v in (vectors or {}).items():
        mel.set_material_instance_vector_parameter_value(mi, k, unreal.LinearColor(v[0], v[1], v[2], 1.0))
    for k, v in (textures or {}).items():
        mel.set_material_instance_texture_parameter_value(mi, k, v)
    mel.update_material_instance(mi)
    unreal.EditorAssetLibrary.save_asset(path)
    return mi


def build_materials(textures, report):
    surface = build_surface_master(textures, report)
    flat = build_flat_master(report)
    mats = {}
    for name, _, tile_cm in SETS:
        tex = {"Cor": textures[TEX_DIR + "/T_%s_D" % name], "Normal": textures[TEX_DIR + "/T_%s_N" % name],
               "ARM": textures[TEX_DIR + "/T_%s_ARM" % name]}
        if name == "Fachada":
            for i, tint in enumerate(FACADE_TINTS):
                mats["Fachada%d" % i] = instance(surface, "MI_Fachada%d" % i, {"Tile": tile_cm}, {"Tint": tint}, tex)
        else:
            mats[name] = instance(surface, "MI_" + name, {"Tile": tile_cm}, None, tex)
    # quinta fachada: concreto aparente (a textura do meio-fio, clara)
    concrete = {"Cor": textures[TEX_DIR + "/T_MeioFio_D"], "Normal": textures[TEX_DIR + "/T_MeioFio_N"], "ARM": textures[TEX_DIR + "/T_MeioFio_ARM"]}
    mats["Fachada%d" % len(FACADE_TINTS)] = instance(surface, "MI_Fachada%d" % len(FACADE_TINTS), {"Tile": 250.0}, {"Tint": (1.15, 1.12, 1.08)}, concrete)
    mats["FaixaBranca"] = instance(flat, "MI_FaixaBranca", {"Aspereza": 0.6}, {"Cor": (0.75, 0.75, 0.72)})
    mats["FaixaAmarela"] = instance(flat, "MI_FaixaAmarela", {"Aspereza": 0.6}, {"Cor": (0.80, 0.52, 0.04)})
    # vidro com salas falsas: uma instância por pé-direito (as salas precisam casar com os andares) e uma para as vitrines do térreo
    window = build_window_master(report)
    for floor_cm in FLOOR_HEIGHTS:
        mats["Janela%d" % floor_cm] = instance(window, "MI_Janela%d" % floor_cm, {"PaneWidthCm": 150.0},
                                               {"RoomSizeCm": (450.0, 450.0, floor_cm), "RoomOffsetCm": (0.0, 0.0, SIDEWALK_TOP + GROUND_FLOOR_CM)})
    mats["Vitrine"] = instance(window, "MI_Vitrine", {"PaneWidthCm": 250.0, "BrilhoInterior": 2000.0},
                               {"RoomSizeCm": (700.0, 700.0, GROUND_FLOOR_CM), "RoomOffsetCm": (0.0, 0.0, SIDEWALK_TOP)})
    report.append("instâncias de material: %d" % len(mats))
    return mats


# --- mapa ---

class Builder:
    def __init__(self, mats):
        self.mats = mats
        self.cube = unreal.load_asset(CUBE)
        self.actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
        self.count = 0

    def spawn(self, cls, location, rotation=None, label=None):
        actor = self.actors.spawn_actor_from_class(cls, unreal.Vector(*location), rotation or unreal.Rotator(0.0, 0.0, 0.0))
        if actor is None:
            raise LookdevError("Não consegui criar o ator " + cls.__name__)
        if label:
            actor.set_actor_label(label)
        self.count += 1
        return actor

    def box(self, label, x0, x1, y0, y1, z0, z1, mat):
        """Bloco alinhado aos eixos entre os limites dados [cm], com o cubo básico de 100 cm."""
        actor = self.spawn(unreal.StaticMeshActor, ((x0 + x1) * 0.5, (y0 + y1) * 0.5, (z0 + z1) * 0.5), label=label)
        comp = actor.static_mesh_component
        comp.set_static_mesh(self.cube)
        comp.set_material(0, self.mats[mat])
        actor.set_actor_scale3d(unreal.Vector((x1 - x0) / 100.0, (y1 - y0) / 100.0, (z1 - z0) / 100.0))
        return actor


def build_street(b):
    half_road = LANE_CM
    b.box("Chao", -200000, 200000, -200000, 200000, -40, 0, "MeioFio")
    b.box("Rua_Asfalto", ROAD_X0, ROAD_X1, -half_road, half_road, -20, ROAD_TOP, "Asfalto")
    for side in (-1, 1):
        y_curb0, y_curb1 = sorted((side * half_road, side * (half_road + CURB_W)))
        b.box("MeioFio_%+d" % side, ROAD_X0, ROAD_X1, y_curb0, y_curb1, -20, SIDEWALK_TOP, "MeioFio")
        y_walk0, y_walk1 = sorted((side * (half_road + CURB_W), side * (half_road + CURB_W + SIDEWALK_W)))
        b.box("Calcada_%+d" % side, ROAD_X0, ROAD_X1, y_walk0, y_walk1, -20, SIDEWALK_TOP, "Calcada")
        # linha de bordo branca contínua, 10 cm, a 20 cm do meio-fio
        y_edge = side * (half_road - 30.0)
        b.box("LinhaBordo_%+d" % side, ROAD_X0, ROAD_X1, y_edge - 5.0, y_edge + 5.0, ROAD_TOP - 0.2, ROAD_TOP + 0.4, "FaixaBranca")
    # faixa central amarela tracejada: 3 m pintados, 6 m de intervalo
    x = ROAD_X0 + 200.0
    n = 0
    while x + 300.0 < ROAD_X1:
        b.box("FaixaCentral_%03d" % n, x, x + 300.0, -6.0, 6.0, ROAD_TOP - 0.2, ROAD_TOP + 0.4, "FaixaAmarela")
        x += 900.0
        n += 1


FACADES = []  # (x inicial, x final, lado, y da fachada) de cada prédio, para prender as lanternas


def build_buildings(b, report):
    FACADES.clear()
    rng = random.Random(15)  # semente fixa: a cena é igual em toda execução
    front = LANE_CM + CURB_W + SIDEWALK_W
    total = 0
    low = 0
    for side in (-1, 1):
        x = ROAD_X0
        while x < ROAD_X1:
            width = rng.uniform(1400.0, 2600.0)
            in_low_zone = any(z0 <= x < z1 and s == side for z0, z1, s in LOW_ZONES)
            if in_low_zone:
                # baixo e recuado: 1 ou 2 andares, 6 a 12 m para dentro (deixa o sol entrar na calçada)
                floors = rng.randint(1, 2)
                setback = rng.uniform(600.0, 1200.0)
                low += 1
            else:
                floors = rng.randint(4, 12)
                setback = rng.choice((0.0, 0.0, 150.0, 300.0))
            floor_cm = rng.choice(FLOOR_HEIGHTS + (FLOOR_HEIGHTS[0],))
            height = GROUND_FLOOR_CM + floors * floor_cm
            depth = rng.uniform(1500.0, 2200.0)
            mat = "Fachada%d" % rng.randrange(len(FACADE_TINTS) + 1)
            y_face = side * (front + setback)
            y0, y1 = sorted((y_face, y_face + side * depth))
            b.box("Predio_%+d_%02d" % (side, total), x, x + width, y0, y1, SIDEWALK_TOP - 5.0, SIDEWALK_TOP + height, mat)
            FACADES.append((x, x + width, side, y_face))
            # vidro na fachada da rua: vitrine no térreo e uma faixa por andar, 2 cm para fora da parede
            gy0, gy1 = sorted((y_face, y_face - side * 2.0))
            b.box("Vitrine_%+d_%02d" % (side, total), x + 150.0, x + width - 150.0, gy0, gy1, SIDEWALK_TOP + 40.0, SIDEWALK_TOP + GROUND_FLOOR_CM - 90.0, "Vitrine")
            window_h = floor_cm * 0.5
            for f in range(floors):
                z0 = SIDEWALK_TOP + GROUND_FLOOR_CM + f * floor_cm + (floor_cm - window_h) * 0.45
                b.box("Janelas_%+d_%02d_%02d" % (side, total, f), x + 120.0, x + width - 120.0, gy0, gy1, z0, z0 + window_h, "Janela%d" % floor_cm)
            total += 1
            x += width + rng.choice((0.0, 0.0, 200.0, 400.0))
    report.append("prédios: %d, dos quais %d baixos e recuados (blocos simples no lugar dos prédios do City Sample, que não estão no PC)" % (total, low))


def build_light(b):
    sun = b.spawn(unreal.DirectionalLight, (0.0, 0.0, 2000.0), unreal.Rotator(roll=0.0, pitch=SUN_PITCH, yaw=SUN_YAW), "Sol")
    sun.root_component.set_mobility(unreal.ComponentMobility.MOVABLE)
    light = sun.light_component
    light.set_editor_property("intensity", SUN_LUX)
    light.set_editor_property("atmosphere_sun_light", True)
    light.set_editor_property("cast_shadows", True)

    b.spawn(unreal.SkyAtmosphere, (0.0, 0.0, 0.0), label="Atmosfera")

    sky = b.spawn(unreal.SkyLight, (0.0, 0.0, 1000.0), label="LuzDoCeu")
    sky.root_component.set_mobility(unreal.ComponentMobility.MOVABLE)
    sky.light_component.set_editor_property("real_time_capture", True)

    clouds = b.spawn(unreal.VolumetricCloud, (0.0, 0.0, 0.0), label="Nuvens")
    cloud_material = unreal.load_asset(CLOUD_MATERIAL)
    if cloud_material is None:
        raise LookdevError("Material de nuvem não encontrado: " + CLOUD_MATERIAL)
    clouds.get_component_by_class(unreal.VolumetricCloudComponent).set_editor_property("material", cloud_material)

    fog = b.spawn(unreal.ExponentialHeightFog, (0.0, 0.0, 0.0), label="Neblina")
    fog_comp = fog.get_component_by_class(unreal.ExponentialHeightFogComponent)
    fog_comp.set_editor_property("fog_density", 0.02)
    fog_comp.set_editor_property("fog_height_falloff", 0.2)

    post = b.spawn(unreal.PostProcessVolume, (0.0, 0.0, 0.0), label="PosProcesso")
    post.set_editor_property("unbound", True)
    s = post.get_editor_property("settings")
    for prop, value in (
        ("auto_exposure_method", unreal.AutoExposureMethod.AEM_MANUAL),
        ("auto_exposure_apply_physical_camera_exposure", True),
        ("depth_of_field_fstop", CAMERA_FSTOP),
        ("camera_shutter_speed", CAMERA_SHUTTER),
        ("camera_iso", CAMERA_ISO),
        # desfoque de movimento desligado aqui: a câmera do carro já desliga (rodas nítidas); o "só de câmera" fica para o passo 6
        ("motion_blur_amount", 0.0),
    ):
        s.set_editor_property("override_" + prop, True)
        s.set_editor_property(prop, value)
    post.set_editor_property("settings", s)

    start = b.spawn(unreal.PlayerStart, START, unreal.Rotator(roll=0.0, pitch=0.0, yaw=0.0), "Largada")
    start.set_editor_property("tags", [unreal.Name("DNMapStart")])


def open_empty_map(report):
    """Abre o mapa (criando se não existe) e tira todos os atores: o conteúdo sai inteiro do script, nunca de uma edição à mão."""
    check_game_path(MAP_PATH.rsplit("/", 1)[0])
    levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    if not unreal.EditorAssetLibrary.does_asset_exist(MAP_PATH):
        if not levels.new_level(MAP_PATH):
            raise LookdevError("Não consegui criar o mapa " + MAP_PATH)
        return levels
    if not levels.load_level(MAP_PATH):
        raise LookdevError("Não consegui abrir o mapa " + MAP_PATH)
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    removed = 0
    for actor in actors.get_all_level_actors():
        if isinstance(actor, unreal.WorldSettings):
            continue
        if actors.destroy_actor(actor):
            removed += 1
    report.append("mapa existente esvaziado: %d atores removidos" % removed)
    return levels


# --- mobiliário de rua (modelos CC0 do Poly Haven; uma linha por modelo em docs/licencas.csv) ---

PROP_DIR = ROOT + "/Props"
PROPS = ("street_lamp_01", "street_lamp_02", "metal_trash_can", "modular_street_seating", "fire_hydrant", "jacaranda_tree")
SKIP_SETS = ("_aged", "_rust")  # variações gastas ficam de fora: as limpas bastam e pesam menos


def prop_texture_sets(folder):
    """{conjunto: {tipo: arquivo}} a partir de textures/<conjunto>_<tipo>_<res>.<ext>."""
    sets = {}
    tdir = os.path.join(folder, "textures")
    for name in sorted(os.listdir(tdir)):
        m = re.match(r"^(.*)_(diff|nor_gl|rough|metal|opacity|alpha)_\dk\.(jpg|png|exr)$", name)
        if not m or any(m.group(1).endswith(s) for s in SKIP_SETS):
            continue
        sets.setdefault(m.group(1), {})[m.group(2)] = os.path.join(tdir, name)
    return sets


def import_prop_texture(path, name, kind):
    task = unreal.AssetImportTask()
    task.filename = path
    task.destination_path = PROP_DIR + "/Textures"
    task.destination_name = name
    task.automated = True
    task.replace_existing = True
    task.save = False
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    asset_path = PROP_DIR + "/Textures/" + name
    tex = unreal.load_asset(asset_path)
    if not isinstance(tex, unreal.Texture2D):
        raise LookdevError("A importação não gerou a textura " + asset_path)
    if kind == "nor_gl":
        tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP)
        tex.set_editor_property("srgb", False)
        tex.set_editor_property("flip_green_channel", True)  # normal no padrão OpenGL; o Unreal usa o DirectX
    elif kind != "diff":
        tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_MASKS)
        tex.set_editor_property("srgb", False)
    unreal.EditorAssetLibrary.save_asset(asset_path)
    return tex


def plain_texture(name, value, kind):
    """Textura 4x4 de um valor só (conteúdo próprio, gerada aqui), com a compressão do tipo: o padrão dos parâmetros do material-pai
    precisa casar com o tipo de amostragem, e o "sem metal" das instâncias precisa de uma máscara preta de verdade."""
    png = os.path.join(project_dir(), "Saved", "Lookdev", name + ".png")
    os.makedirs(os.path.dirname(png), exist_ok=True)
    row = b"\x00" + bytes([value, value, value]) * 4

    def chunk(kind_, data):
        return struct.pack(">I", len(data)) + kind_ + data + struct.pack(">I", zlib.crc32(kind_ + data) & 0xFFFFFFFF)

    with open(png, "wb") as handle:
        handle.write(b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", 4, 4, 8, 2, 0, 0, 0)) + chunk(b"IDAT", zlib.compress(row * 4)) + chunk(b"IEND", b""))
    return import_prop_texture(png, name, kind)


def build_prop_master(name, masked, defaults, report):
    """Material-pai dos modelos: cor, normal, aspereza (R) e metal (R); o mascarado tem opacidade (R) e duas faces (folhas, vidro)."""
    path = PROP_DIR + "/" + name
    mel = unreal.MaterialEditingLibrary
    m = recreate(path, unreal.Material, unreal.MaterialFactoryNew())

    def sampler(param, texture, kind, y):
        s = expr(m, unreal.MaterialExpressionTextureSampleParameter2D, -600, y)
        s.set_editor_property("parameter_name", param)
        s.set_editor_property("texture", texture)
        s.set_editor_property("sampler_type", kind)
        return s

    color = sampler("Cor", defaults["cor"], unreal.MaterialSamplerType.SAMPLERTYPE_COLOR, -300)
    normal = sampler("Normal", unreal.load_asset("/Engine/EngineMaterials/DefaultNormal"), unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL, 0)
    rough = sampler("Aspereza", defaults["branco"], unreal.MaterialSamplerType.SAMPLERTYPE_MASKS, 300)
    metal = sampler("Metal", defaults["preto"], unreal.MaterialSamplerType.SAMPLERTYPE_MASKS, 600)
    mel.connect_material_property(color, "RGB", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(normal, "RGB", unreal.MaterialProperty.MP_NORMAL)
    mel.connect_material_property(rough, "R", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.connect_material_property(metal, "R", unreal.MaterialProperty.MP_METALLIC)
    if masked:
        m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_MASKED)
        m.set_editor_property("two_sided", True)
        opacity = sampler("Opacidade", defaults["branco"], unreal.MaterialSamplerType.SAMPLERTYPE_MASKS, 900)
        mel.connect_material_property(opacity, "R", unreal.MaterialProperty.MP_OPACITY_MASK)
    m.set_editor_property("used_with_nanite", True)
    mel.recompile_material(m)
    unreal.EditorAssetLibrary.save_asset(path)
    report.append("material criado: " + path)
    return m


def import_prop_mesh(fbx, name):
    task = unreal.AssetImportTask()
    task.filename = fbx
    task.destination_path = PROP_DIR
    task.destination_name = name
    task.automated = True
    task.replace_existing = True
    task.save = False
    opts = unreal.FbxImportUI()
    opts.set_editor_property("import_mesh", True)
    opts.set_editor_property("import_materials", False)
    opts.set_editor_property("import_textures", False)
    opts.set_editor_property("import_as_skeletal", False)
    opts.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_STATIC_MESH)
    data = opts.get_editor_property("static_mesh_import_data")
    data.set_editor_property("combine_meshes", True)
    data.set_editor_property("build_nanite", True)
    data.set_editor_property("generate_lightmap_u_vs", False)
    task.options = opts
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    mesh = unreal.load_asset(PROP_DIR + "/" + name)
    if not isinstance(mesh, unreal.StaticMesh):
        raise LookdevError("A importação não gerou a malha " + PROP_DIR + "/" + name)
    return mesh


def import_props(models_root, report):
    """Importa os modelos que estiverem em models_root/<id>/ e devolve {id: StaticMesh}. Sem a pasta: nenhum (a cena sai sem eles)."""
    if not models_root or not os.path.isdir(models_root):
        report.append("mobiliário de rua: pasta de modelos ausente, cena sem ele")
        return {}
    check_game_path(PROP_DIR)
    defaults = {"cor": plain_texture("T_DNBrancoCor", 255, "diff"), "branco": plain_texture("T_DNBranco", 255, "mask"),
                "preto": plain_texture("T_DNPreto", 0, "mask")}
    opaque = build_prop_master("M_DNPropOpaco", False, defaults, report)
    masked = build_prop_master("M_DNPropMascarado", True, defaults, report)
    meshes = {}
    for ph_id in PROPS:
        folder = os.path.join(models_root, ph_id)
        fbx = [f for f in os.listdir(folder) if f.lower().endswith(".fbx")] if os.path.isdir(folder) else []
        if not fbx:
            report.append("mobiliário: %s não encontrado, ficou de fora" % ph_id)
            continue
        if unreal.EditorAssetLibrary.does_asset_exist(PROP_DIR + "/SM_" + ph_id):
            # já importado: reaproveita (a importação do jacarandá leva minutos); para reimportar, apague a pasta Props
            meshes[ph_id] = unreal.load_asset(PROP_DIR + "/SM_" + ph_id)
            report.append("mobiliário %s: reaproveitado" % ph_id)
            continue
        mesh = import_prop_mesh(os.path.join(folder, fbx[0]), "SM_" + ph_id)
        instances = {}
        for key, files in prop_texture_sets(folder).items():
            tex = {kind: import_prop_texture(path, "T_%s_%s" % (key, kind), kind) for kind, path in files.items()}
            is_masked = "opacity" in tex or "alpha" in tex
            params = {"Cor": tex.get("diff"), "Normal": tex.get("nor_gl"), "Aspereza": tex.get("rough"), "Metal": tex.get("metal") or defaults["preto"],
                      "Opacidade": tex.get("opacity") or tex.get("alpha")}
            instances[key] = instance(masked if is_masked else opaque, "MI_" + key, None, None, {k: v for k, v in params.items() if v is not None})
        slots = [str(s.material_slot_name) for s in mesh.get_editor_property("static_materials")]
        chosen = []
        for i, slot in enumerate(slots):
            # o conjunto cujo sufixo aparece no nome do slot; senão o conjunto com o nome do modelo
            low = slot.lower()
            best = next((k for k in instances if k != ph_id and k[len(ph_id) + 1:] and k[len(ph_id) + 1:] in low), None)
            best = best or (ph_id if ph_id in instances else next(iter(instances), None))
            if best:
                mesh.set_material(i, instances[best])
            chosen.append("%s->%s" % (slot, best))
        unreal.EditorAssetLibrary.save_asset(PROP_DIR + "/SM_" + ph_id)
        bounds = mesh.get_bounds()
        report.append("mobiliário %s: extensão %s cm, slots %s" % (ph_id, bounds.box_extent, ", ".join(chosen)))
        meshes[ph_id] = mesh
    return meshes


def build_props(b, props, report):
    """Postes perto do meio-fio, bancos e lixeiras junto aos prédios, hidrantes, e jacarandás nos recuos dos trechos baixos."""
    if not props:
        return
    front = LANE_CM + CURB_W + SIDEWALK_W
    count = 0

    def place(ph_id, x, y, yaw, z=SIDEWALK_TOP, scale=1.0):
        nonlocal count
        if ph_id not in props:
            return
        actor = b.spawn(unreal.StaticMeshActor, (x, y, z), unreal.Rotator(roll=0.0, pitch=0.0, yaw=yaw), "%s_%d" % (ph_id, count))
        actor.static_mesh_component.set_static_mesh(props[ph_id])
        actor.set_actor_scale3d(unreal.Vector(scale, scale, scale))
        count += 1

    for side in (-1, 1):
        x = ROAD_X0 + 1000.0
        n = 0
        while x < ROAD_X1 - 500.0:
            place("street_lamp_01", x, side * (LANE_CM + CURB_W + 60.0), -90.0 * side)
            n += 1
            x += 2500.0
        x = ROAD_X0 + 1500.0 + (700.0 if side > 0 else 0.0)
        while x < ROAD_X1 - 500.0:
            place("modular_street_seating", x, side * (front - 90.0), 90.0 * side)
            place("metal_trash_can", x + 300.0, side * (front - 70.0), 0.0)
            x += 6000.0
        x = ROAD_X0 + 3200.0 + (1800.0 if side > 0 else 0.0)
        while x < ROAD_X1 - 500.0:
            place("fire_hydrant", x, side * (LANE_CM + CURB_W + 45.0), 0.0)
            x += 7000.0
    # lanterna de parede (street_lamp_02 é a cabeça sem poste): no meio da fachada, a 2,8 m da calçada, de prédio sim, prédio não
    for i, (fx0, fx1, side, y_face) in enumerate(FACADES):
        if i % 2 == 0:
            place("street_lamp_02", (fx0 + fx1) * 0.5, y_face - side * 25.0, -90.0 * side, z=SIDEWALK_TOP + 280.0)
    for z0, z1, side in LOW_ZONES:
        x = z0 + 600.0
        while x < z1 - 600.0:
            place("jacaranda_tree", x, side * (front + 450.0), (x * 37.0) % 360.0, scale=0.6)  # o modelo tem 19 m de altura
            x += 1200.0
    report.append("mobiliário de rua colocado: %d atores" % count)


def build_map(mats, report, props=None):
    levels = open_empty_map(report)
    b = Builder(mats)
    build_light(b)
    build_street(b)
    build_buildings(b, report)
    build_props(b, props or {}, report)
    if not levels.save_current_level():
        raise LookdevError("Não consegui salvar o mapa " + MAP_PATH)
    report.append("mapa criado: %s (%d atores)" % (MAP_PATH, b.count))


def main():
    report = []
    error = None
    report_path = None
    try:
        args = parse_args(sys.argv[1:])
        report_path = resolve_report(args.report)
        source = os.path.abspath(args.textures)
        textures = import_textures(source, report)
        mats = build_materials(textures, report)
        props = import_props(os.path.abspath(args.models) if args.models else None, report)
        build_map(mats, report, props)
    except LookdevError as exc:
        error = str(exc)
    except Exception as exc:  # qualquer outro erro também derruba o processo, com mensagem
        error = "Erro inesperado: %r" % (exc,)

    if error:
        report.append("ERRO: " + error)
        unreal.log_error("[create_lookdev] " + error)
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
