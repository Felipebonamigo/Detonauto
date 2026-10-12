"""
Importa uma pasta de modelos para o projeto como Static Mesh e cria um material por modelo.

Rodar SEM janela, pelo commandlet de Python do Unreal (testado no UE 5.8):

    UnrealEditor-Cmd.exe <projeto>.uproject -run=PythonScript ^
        -Script="<caminho>\\import_models.py --src <pasta> --dest /Game/Pasta" ^
        -unattended -nullrhi -nosplash -nosound

O script sai com erro (código diferente de zero no processo) se algo falhar.
Existe um invólucro em PowerShell: Tools/Unreal/import_models.ps1.

Argumentos
    --src            pasta no disco com .fbx (e .obj) para importar (não recursivo)
    --dest           pasta de destino, SEMPRE dentro de /Game/ do projeto (ex.: /Game/Sandbox_Teste)
    --base-material  material-base em /Game/... a instanciar. Se omitido, o script cria
                     <dest>/M_DNBase, com um parâmetro de cor "BaseColor".
    --report         arquivo de relatório (padrão: <projeto>/Saved/ImportReport.txt)

Convenção de nomes (CLAUDE.md): SM_ para malha estática, MI_ para instância de material,
M_ para o material-base.

LIMITE DE SEGURANÇA: o script só grava pacotes em /Game/ do projeto que o abriu. Qualquer
destino que não comece com /Game/ (por exemplo /Engine, /Script ou um plugin) é recusado antes
de qualquer escrita. O material-base informado também precisa estar em /Game/. O relatório só
pode ficar dentro da pasta do projeto.
"""

import sys

# O Python embutido da engine gravaria arquivos __pycache__/*.pyc dentro da instalação do Unreal
# ao importar a biblioteca padrão. Desliga isso ANTES de qualquer outro import.
sys.dont_write_bytecode = True

import argparse
import os
import re

import unreal

SUPPORTED_EXTENSIONS = (".fbx", ".obj")

# /Game seguido de uma ou mais pastas com letras, números e _  (sem "..", sem barras duplas)
DEST_PATTERN = re.compile(r"^/Game(/[A-Za-z0-9_]+)+$")


class ImportError_(RuntimeError):
    """Erro esperado, com mensagem para o usuário."""


def log(message):
    unreal.log("[import_models] " + message)


def validate_game_path(path, what):
    """Recusa tudo que não seja uma pasta /Game/... simples."""
    if not DEST_PATTERN.match(path):
        raise ImportError_(
            "%s recusado: '%s'. Só são aceitos caminhos dentro de /Game/ do projeto, "
            "como /Game/Sandbox_Teste (letras, números e _; sem '..', sem /Engine, sem plugins)." % (what, path)
        )


def parse_args(argv):
    parser = argparse.ArgumentParser(prog="import_models.py", add_help=False)
    parser.add_argument("--src", required=True)
    parser.add_argument("--dest", required=True)
    parser.add_argument("--base-material", default=None)
    parser.add_argument("--report", default=None)
    try:
        return parser.parse_args(argv)
    except SystemExit:
        raise ImportError_("Argumentos inválidos. Uso: --src <pasta> --dest /Game/<pasta> [--base-material /Game/...] [--report <arquivo>]")


def asset_name(stem, prefix):
    clean = re.sub(r"[^A-Za-z0-9_]", "_", stem)
    return prefix + clean


def project_dir():
    return os.path.abspath(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))


def resolve_report(path_arg):
    project = project_dir()
    default = os.path.join(project, "Saved", "ImportReport.txt")
    path = os.path.abspath(path_arg) if path_arg else default
    inside = os.path.normcase(path).startswith(os.path.normcase(project) + os.sep)
    if not inside:
        raise ImportError_("Relatório recusado: '%s' fica fora da pasta do projeto (%s)." % (path, project))
    return path


def ensure_base_material(dest, base_arg, report):
    """Devolve o material-base. Cria M_DNBase com um parâmetro de cor se nenhum foi informado."""
    eal = unreal.EditorAssetLibrary

    if base_arg:
        validate_game_path(os.path.dirname(base_arg), "Material-base")
        if not eal.does_asset_exist(base_arg):
            raise ImportError_("Material-base não existe: %s" % base_arg)
        report.append("material-base existente: %s" % base_arg)
        return eal.load_asset(base_arg)

    base_path = dest + "/M_DNBase"
    if eal.does_asset_exist(base_path):
        report.append("material-base reaproveitado: %s" % base_path)
        return eal.load_asset(base_path)

    tools = unreal.AssetToolsHelpers.get_asset_tools()
    material = tools.create_asset("M_DNBase", dest, unreal.Material, unreal.MaterialFactoryNew())
    if material is None:
        raise ImportError_("Não consegui criar o material-base em %s" % dest)

    mel = unreal.MaterialEditingLibrary
    color = mel.create_material_expression(material, unreal.MaterialExpressionVectorParameter, -400, 0)
    color.set_editor_property("parameter_name", "BaseColor")
    color.set_editor_property("default_value", unreal.LinearColor(0.8, 0.8, 0.8, 1.0))
    mel.connect_material_property(color, "", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.recompile_material(material)
    eal.save_asset(base_path)
    report.append("material-base criado: %s" % base_path)
    return material


def import_one(file_path, dest):
    """Importa um arquivo. Devolve a lista de caminhos de assets criados."""
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", file_path)
    task.set_editor_property("destination_path", dest)
    task.set_editor_property("automated", True)  # sem diálogos
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("save", True)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    return [str(p) for p in task.get_editor_property("imported_object_paths")]


def run(args, report):
    src = os.path.abspath(args.src)
    if not os.path.isdir(src):
        raise ImportError_("Pasta de entrada não existe: %s" % src)

    validate_game_path(args.dest, "Destino")
    if args.base_material:
        validate_game_path(os.path.dirname(args.base_material), "Material-base")

    files = sorted(
        f for f in os.listdir(src)
        if os.path.isfile(os.path.join(src, f)) and f.lower().endswith(SUPPORTED_EXTENSIONS)
    )
    if not files:
        raise ImportError_("Nenhum arquivo %s em %s" % ("/".join(SUPPORTED_EXTENSIONS), src))

    report.append("entrada: %s" % src)
    report.append("destino: %s" % args.dest)

    eal = unreal.EditorAssetLibrary
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    base = ensure_base_material(args.dest, args.base_material, report)

    created = 0
    failed = []
    for name in files:
        stem = os.path.splitext(name)[0]
        mesh_name = asset_name(stem, "SM_")
        mi_name = asset_name(stem, "MI_")

        imported = import_one(os.path.join(src, name), args.dest)
        meshes = [p for p in imported if isinstance(eal.load_asset(p), unreal.StaticMesh)]
        if not meshes:
            failed.append(name)
            report.append("FALHOU: %s (nada importado como Static Mesh)" % name)
            continue

        # o importador nomeia pelo arquivo; renomeia para a convenção SM_
        mesh_path = meshes[0]
        wanted = args.dest + "/" + mesh_name
        if mesh_path.split(".")[0] != wanted:
            if eal.does_asset_exist(wanted):
                eal.delete_asset(wanted)
            if not eal.rename_asset(mesh_path.split(".")[0], wanted):
                failed.append(name)
                report.append("FALHOU: %s (não consegui renomear para %s)" % (name, mesh_name))
                continue
        mesh = eal.load_asset(wanted)

        mi_path = args.dest + "/" + mi_name
        mi = eal.load_asset(mi_path) if eal.does_asset_exist(mi_path) else tools.create_asset(
            mi_name, args.dest, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
        mi.set_editor_property("parent", base)
        mesh.set_material(0, mi)
        eal.save_asset(mi_path)
        eal.save_asset(wanted)

        report.append("OK: %s -> %s + %s" % (name, mesh_name, mi_name))
        created += 1

    report.append("resumo: %d modelo(s) importado(s), %d falha(s)" % (created, len(failed)))
    if failed:
        raise ImportError_("Falhou em: " + ", ".join(failed))


def main():
    report = []
    error = None
    report_path = None
    try:
        args = parse_args(sys.argv[1:])
        report_path = resolve_report(args.report)
        run(args, report)
    except ImportError_ as exc:
        error = str(exc)
    except Exception as exc:  # qualquer outro erro também derruba o processo, com mensagem
        error = "Erro inesperado: %r" % (exc,)

    if error:
        report.append("ERRO: " + error)
        unreal.log_error("[import_models] " + error)

    if report_path:
        os.makedirs(os.path.dirname(report_path), exist_ok=True)
        with open(report_path, "w", encoding="utf-8") as handle:
            handle.write("\n".join(report) + "\n")
        log("relatório: " + report_path)

    for line in report:
        log(line)

    if error:
        # derruba o commandlet com código diferente de zero
        raise RuntimeError(error)


main()
