"""
Cria o material de pós-processamento do desfoque nas bordas (M_DNEdgeBlur, lote 18, docs/regras-camera.md) por script, sem abrir o
editor.

Rodar SEM janela, pelo commandlet de Python do Unreal (testado no UE 5.8):

    UnrealEditor-Cmd.exe <projeto>.uproject -run=PythonScript ^
        -Script="<caminho>/create_edge_blur_material.py --dest /Game/Detonauto/Camera" ^
        -unattended -nullrhi -nosplash -nosound

Existe um invólucro em PowerShell: Tools/Unreal/create_edge_blur_material.ps1.

O QUE O MATERIAL FAZ
    Desfoque radial a partir do centro da tela, só nas bordas: cada pixel tira a média de amostras da cor da cena no caminho até o
    centro. Não usa o vetor de movimento da engine, então nunca borra as rodas (o Felipe não quer roda borrada; o desfoque de
    movimento da engine está zerado no ADNVehicle).
        r      distância ao centro em pixels, dividida pela meia diagonal da tela (0 no centro, 1 no canto)
        força  Strength x smoothstep(InnerRadius, 1, r): zero dentro de InnerRadius, onde está o carro
        rastro BlurLength x força x (vetor até o centro): a amostra mais longe anda essa fração do caminho até o centro
    Parâmetros: Strength (o C++ manda EdgeBlurMax x S(v) a cada quadro; 0 desliga), InnerRadius (EdgeBlurInnerRadius de
    Data/CameraFx.csv) e BlurLength (ajuste visual, só aqui).
    Local na cadeia: antes do bloom (depois do TSR, em cor linear e na resolução da tela): o TSR não desfaz o rastro e as luzes fortes
    borram como luz.
    Rodar de novo refaz o material no mesmo pacote (sem apagar).

LIMITE DE SEGURANÇA: só grava pacotes em /Game/ do projeto que o abriu (qualquer outro destino é recusado antes de qualquer escrita)
e o relatório só pode ficar dentro da pasta do projeto.
"""

import sys

# O Python embutido da engine gravaria .pyc dentro da instalação do Unreal. Desliga ANTES de qualquer outro import.
sys.dont_write_bytecode = True

import argparse
import os
import re

import unreal

DEST_PATTERN = re.compile(r"^/Game(/[A-Za-z0-9_]+)+$")
ASSET_NAME = "M_DNEdgeBlur"

# 14 = PPI_PostProcessInput0 (a cor da cena). A versão da amostra com Parameters desfaz a pré-exposição antes do tonemapper, como o
# nó SceneTexture (C) que entra aqui: as duas leituras ficam na mesma escala. 12 amostras com um deslocamento por pixel (ruído
# intercalado) para os degraus entre amostras virarem grão fino em vez de cópias do contorno.
EDGE_BLUR_HLSL = """
float2 vuv = GetViewportUV(Parameters);
float2 size = View.ViewSizeAndInvSize.xy;
float2 toCenter = 0.5 - vuv;
float r = length(toCenter * size) / max(0.5 * length(size), 1.0);
float m = saturate(Strength) * smoothstep(saturate(Inner), 1.0, r);
if (m <= 0.002)
{
    return C;
}
float2 pix = vuv * size;
float jitter = frac(52.9829189 * frac(dot(pix, float2(0.06711056, 0.00583715))));
float2 stepUV = toCenter * (max(Len, 0.0) * m) / 12.0;
float3 acc = 0;
[unroll]
for (int i = 0; i < 12; i++)
{
    float2 uv = vuv + stepUV * (i + jitter);
    float2 suv = ClampSceneTextureUV(ViewportUVToSceneTextureUV(uv, 14), 14);
    acc += SceneTextureLookup(Parameters, suv, 14, true).rgb;
}
return acc / 12.0;
"""


class EdgeBlurError(RuntimeError):
    """Erro esperado, com mensagem para o usuário."""


def log(message):
    unreal.log("[create_edge_blur_material] " + message)


def project_dir():
    return os.path.abspath(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))


def parse_args(argv):
    parser = argparse.ArgumentParser(prog="create_edge_blur_material.py", add_help=False)
    parser.add_argument("--dest", required=True)
    parser.add_argument("--report", default=None)
    try:
        return parser.parse_args(argv)
    except SystemExit:
        raise EdgeBlurError("Argumentos inválidos. Uso: --dest /Game/<pasta> [--report <arquivo>]")


def resolve_report(path_arg):
    project = project_dir()
    path = os.path.abspath(path_arg) if path_arg else os.path.join(project, "Saved", "EdgeBlurMaterialReport.txt")
    if not os.path.normcase(path).startswith(os.path.normcase(project) + os.sep):
        raise EdgeBlurError("Relatório recusado: '%s' fica fora da pasta do projeto (%s)." % (path, project))
    return path


def node(mel, material, cls, x, y):
    return mel.create_material_expression(material, cls, x, y)


def scalar_param(mel, material, name, value, x, y):
    n = node(mel, material, unreal.MaterialExpressionScalarParameter, x, y)
    n.set_editor_property("parameter_name", name)
    n.set_editor_property("default_value", value)
    return n


def build(dest, report):
    if not DEST_PATTERN.match(dest):
        raise EdgeBlurError("Destino recusado: '%s'. Só /Game/<pasta> do projeto é aceito." % dest)

    eal = unreal.EditorAssetLibrary
    mel = unreal.MaterialEditingLibrary
    path = dest + "/" + ASSET_NAME
    if eal.does_asset_exist(path):
        # refaz no mesmo pacote (apagar no commandlet deixa o arquivo no disco e a criação seguinte falha); o delete_all do 5.8 apaga
        # só metade dos nós por chamada: chama até zerar (ver create_dent_material.py)
        material = unreal.load_asset(path)
        left = mel.get_num_material_expressions(material)
        for _ in range(16):
            if left == 0:
                break
            mel.delete_all_material_expressions(material)
            left = mel.get_num_material_expressions(material)
        if left:
            raise EdgeBlurError("Sobraram %d nós no material depois de apagar tudo: %s" % (left, path))
        report.append("material existente refeito: " + path)
    else:
        material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(ASSET_NAME, dest, unreal.Material, unreal.MaterialFactoryNew())
    if material is None:
        raise EdgeBlurError("Não consegui criar o material em " + dest)

    material.set_editor_property("material_domain", unreal.MaterialDomain.MD_POST_PROCESS)
    material.set_editor_property("blendable_location", unreal.BlendableLocation.BL_SCENE_COLOR_BEFORE_BLOOM)

    # a cor da cena no próprio pixel: entra no nó Custom (é ela que liga a textura de entrada do pós-processamento ao material)
    scene = node(mel, material, unreal.MaterialExpressionSceneTexture, -700, 0)
    scene.set_editor_property("scene_texture_id", unreal.SceneTextureId.PPI_POST_PROCESS_INPUT0)

    custom = node(mel, material, unreal.MaterialExpressionCustom, -300, 0)
    custom.set_editor_property("code", EDGE_BLUR_HLSL)
    custom.set_editor_property("description", "Desfoque radial nas bordas")
    custom.set_editor_property("output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    inputs = []
    for name in ("C", "Strength", "Inner", "Len"):
        ci = unreal.CustomInput()
        ci.set_editor_property("input_name", name)
        inputs.append(ci)
    custom.set_editor_property("inputs", inputs)

    links = (
        (scene, "Color", "C"),
        (scalar_param(mel, material, "Strength", 0.0, -700, 200), "", "Strength"),
        (scalar_param(mel, material, "InnerRadius", 0.4, -700, 280), "", "Inner"),
        # com Strength 0,5 (o máximo do CSV) o canto anda 6% do caminho até o centro: ~80 px num quadro de 1920
        (scalar_param(mel, material, "BlurLength", 0.12, -700, 360), "", "Len"),
    )
    for src, out, pin in links:
        if not mel.connect_material_expressions(src, out, custom, pin):
            raise EdgeBlurError("Ligação de material falhou: %s -> %s" % (src.get_name(), pin))
    if not mel.connect_material_property(custom, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR):
        raise EdgeBlurError("Ligação da saída (Emissive Color) falhou")

    mel.recompile_material(material)
    if not eal.save_asset(path):
        raise EdgeBlurError("Não consegui salvar " + path)
    report.append("material criado: " + path)
    report.append("domínio pós-processamento, antes do bloom; parâmetros: Strength (0), InnerRadius (0,4), BlurLength (0,12)")


def main():
    report = []
    error = None
    report_path = None
    try:
        args = parse_args(sys.argv[1:])
        report_path = resolve_report(args.report)
        build(args.dest, report)
    except EdgeBlurError as exc:
        error = str(exc)
    except Exception as exc:  # qualquer outro erro também derruba o processo, com mensagem
        error = "Erro inesperado: %r" % (exc,)

    if error:
        report.append("ERRO: " + error)
        unreal.log_error("[create_edge_blur_material] " + error)

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
