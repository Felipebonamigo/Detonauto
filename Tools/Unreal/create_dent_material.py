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

    Parâmetros extras: BaseColor (cor da tinta), Metallic, Roughness.

LIMITE DE SEGURANÇA: só grava pacotes em /Game/ do projeto que o abriu (qualquer outro destino é recusado antes de
qualquer escrita) e o relatório só pode ficar dentro da pasta do projeto.
"""

import sys

# O Python embutido da engine gravaria .pyc dentro da instalação do Unreal. Desliga ANTES de qualquer outro import.
sys.dont_write_bytecode = True

import argparse
import os
import re

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
    try:
        return parser.parse_args(argv)
    except SystemExit:
        raise DentError("Argumentos inválidos. Uso: --dest /Game/<pasta> [--report <arquivo>]")


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


def build(dest, report):
    if not DEST_PATTERN.match(dest):
        raise DentError("Destino recusado: '%s'. Só /Game/<pasta> do projeto é aceito." % dest)

    eal = unreal.EditorAssetLibrary
    path = dest + "/" + ASSET_NAME
    if eal.does_asset_exist(path):
        eal.delete_asset(path)  # sempre recria: o resultado tem que sair do script, não de uma edição à mão
        report.append("material anterior apagado: " + path)

    mel = unreal.MaterialEditingLibrary
    material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(ASSET_NAME, dest, unreal.Material, unreal.MaterialFactoryNew())
    if material is None:
        raise DentError("Não consegui criar o material em " + dest)

    # Usado em malha estática e esquelética (os pedaços do carro são estáticos presos ao esqueleto).
    material.set_editor_property("used_with_skeletal_mesh", True)
    # A malha da carroceria é Nanite: sem esta flag a engine troca o material pelo padrão (aviso "missing usage flag Nanite").
    material.set_editor_property("used_with_nanite", True)
    # Nanite precisa saber o quanto o material pode deslocar um vértice (para não cortar o objeto por engano).
    try:
        material.set_editor_property("max_world_position_offset_displacement", 40.0)
    except Exception as exc:  # o nome da propriedade pode variar entre versões da engine: registra e segue
        unreal.log_warning("[create_dent_material] não consegui definir o deslocamento máximo do WPO: %r" % (exc,))

    # --- cor, metal e aspereza ---
    color = vector_param(mel, material, "BaseColor", (0.85, 0.18, 0.02, 1.0), -1400, -400)
    metallic = scalar_param(mel, material, "Metallic", 0.6, -1400, -250)
    roughness = scalar_param(mel, material, "Roughness", 0.35, -1400, -150)

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
    mel.connect_material_property(offset, "", unreal.MaterialProperty.MP_WORLD_POSITION_OFFSET)

    mask = node(mel, material, unreal.MaterialExpressionComponentMask, -400, 450)
    mask.set_editor_property("r", False)
    mask.set_editor_property("g", False)
    mask.set_editor_property("b", False)
    mask.set_editor_property("a", True)
    mel.connect_material_expressions(custom, "", mask, "")

    strength = scalar_param(mel, material, "DentDarkening", 0.6, -400, 600)
    scaled = node(mel, material, unreal.MaterialExpressionMultiply, -150, 500)
    mel.connect_material_expressions(mask, "", scaled, "A")
    mel.connect_material_expressions(strength, "", scaled, "B")
    keep = node(mel, material, unreal.MaterialExpressionOneMinus, 50, 500)
    mel.connect_material_expressions(scaled, "", keep, "")
    painted = node(mel, material, unreal.MaterialExpressionMultiply, 250, 0)
    mel.connect_material_expressions(color, "", painted, "A")
    mel.connect_material_expressions(keep, "", painted, "B")

    mel.connect_material_property(painted, "", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(metallic, "", unreal.MaterialProperty.MP_METALLIC)
    mel.connect_material_property(roughness, "", unreal.MaterialProperty.MP_ROUGHNESS)

    mel.recompile_material(material)
    if not eal.save_asset(path):
        raise DentError("Não consegui salvar " + path)
    report.append("material criado: " + path)
    report.append("parâmetros: BaseColor, Metallic, Roughness, DentDarkening, D0P..D7P, D0R..D7R, D0D..D7D")


def main():
    report = []
    error = None
    report_path = None
    try:
        args = parse_args(sys.argv[1:])
        report_path = resolve_report(args.report)
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
