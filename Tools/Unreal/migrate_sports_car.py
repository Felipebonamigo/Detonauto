"""
Renomeia o conjunto mínimo do carro esportivo do template (22 assets) para /Game/Detonauto/Vehicle/SportsCar/, com os nomes
do padrão do CLAUDE.md, corrigindo as referências entre eles.

Rodar SEM janela, pelo commandlet de Python do Unreal (testado no UE 5.8). Use o invólucro Tools/Unreal/migrate_sports_car.ps1,
que antes COPIA os 22 arquivos do projeto de teste para a pasta de passagem /Game/Vehicles/SportsCar do projeto principal.

O QUE ESTE SCRIPT FAZ
    1. confere que os 22 assets estão na pasta de passagem;
    2. renomeia cada um para o destino (rename_asset: atualiza as referências e deixa um redirecionador no lugar antigo);
    3. apaga a pasta de passagem inteira (só /Game/Vehicles, com os redirecionadores);
    4. confere que os assets novos existem, que nenhum aponta para /Game/Vehicles e que as classes estão certas.
    Termina com erro (código diferente de zero) se qualquer conferência falhar.

NOMES (padrão do CLAUDE.md: SK_ malha com esqueleto, SM_ malha estática, M_ material, MI_ instância, T_ textura)
    SKM_SportsCar        -> SK_SportsCar        (malha esquelética)
    SK_SportsCar         -> SKEL_SportsCar      (esqueleto; o nome SK_ passa a ser da malha)
    M_SportsCarChasis    -> MI_SportsCarChasis  (é uma instância de material com nome de material)
    PA_SportsCar, SM_*, M_*, MI_*, T_*  mantêm o nome

LIMITE DE SEGURANÇA: só mexe em /Game/Vehicles (passagem) e /Game/Detonauto/Vehicle do projeto que o abriu. O relatório só pode
ficar dentro da pasta do projeto.
"""

import sys

# O Python embutido da engine gravaria .pyc dentro da instalação do Unreal. Desliga ANTES de qualquer outro import.
sys.dont_write_bytecode = True

import argparse
import os

import unreal

STAGING = "/Game/Vehicles/SportsCar"
DEST = "/Game/Detonauto/Vehicle/SportsCar"

# (nome antigo relativo à passagem, nome novo relativo ao destino, classe esperada)
ASSETS = [
    ("SKM_SportsCar", "SK_SportsCar", "SkeletalMesh"),
    ("SK_SportsCar", "SKEL_SportsCar", "Skeleton"),
    ("PA_SportsCar", "PA_SportsCar", "PhysicsAsset"),
    ("SM_SportsCar", "SM_SportsCar", "StaticMesh"),
    ("SM_SportsCar_Glass", "SM_SportsCar_Glass", "StaticMesh"),
    ("SM_SportsCar_Wheel", "SM_SportsCar_Wheel", "StaticMesh"),
    ("Materials/M_SportsCarBase", "Materials/M_SportsCarBase", "Material"),
    ("Materials/M_SportsCar_Lights", "Materials/M_SportsCar_Lights", "Material"),
    ("Materials/M_SportsCar_Wheel", "Materials/M_SportsCar_Wheel", "Material"),
    ("Materials/M_SportsCarChasis", "Materials/MI_SportsCarChasis", "MaterialInstanceConstant"),
    ("Materials/MI_SportsCarBody", "Materials/MI_SportsCarBody", "MaterialInstanceConstant"),
    ("Materials/MI_SportsCar_Tire", "Materials/MI_SportsCar_Tire", "MaterialInstanceConstant"),
    ("Materials/Textures/T_SportsCar_Body_BotN", "Materials/Textures/T_SportsCar_Body_BotN", "Texture2D"),
    ("Materials/Textures/T_SportsCar_Body_D", "Materials/Textures/T_SportsCar_Body_D", "Texture2D"),
    ("Materials/Textures/T_SportsCar_Body_MRA", "Materials/Textures/T_SportsCar_Body_MRA", "Texture2D"),
    ("Materials/Textures/T_SportsCar_Chasis_D", "Materials/Textures/T_SportsCar_Chasis_D", "Texture2D"),
    ("Materials/Textures/T_SportsCar_Chasis_MRA", "Materials/Textures/T_SportsCar_Chasis_MRA", "Texture2D"),
    ("Materials/Textures/T_SportsCar_Chasis_N", "Materials/Textures/T_SportsCar_Chasis_N", "Texture2D"),
    ("Materials/Textures/T_SportsCar_Tire_M_TGA", "Materials/Textures/T_SportsCar_Tire_M_TGA", "Texture2D"),
    ("Materials/Textures/T_SportsCar_Tire_N", "Materials/Textures/T_SportsCar_Tire_N", "Texture2D"),
    ("Materials/Textures/T_SportsCar_Wheel_M", "Materials/Textures/T_SportsCar_Wheel_M", "Texture2D"),
    ("Materials/Textures/T_SportsCar_Wheel_N", "Materials/Textures/T_SportsCar_Wheel_N", "Texture2D"),
]


class MigrateError(RuntimeError):
    """Erro esperado, com mensagem para o usuário."""


def log(message):
    unreal.log("[migrate_sports_car] " + message)


def project_dir():
    return os.path.abspath(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))


def parse_args(argv):
    parser = argparse.ArgumentParser(prog="migrate_sports_car.py", add_help=False)
    parser.add_argument("--report", default=None)
    try:
        return parser.parse_args(argv)
    except SystemExit:
        raise MigrateError("Argumentos inválidos. Uso: [--report <arquivo>]")


def resolve_report(path_arg):
    project = project_dir()
    path = os.path.abspath(path_arg) if path_arg else os.path.join(project, "Saved", "MigrateSportsCarReport.txt")
    if not os.path.normcase(path).startswith(os.path.normcase(project) + os.sep):
        raise MigrateError("Relatório recusado: '%s' fica fora da pasta do projeto (%s)." % (path, project))
    return path


def asset_class(path):
    data = unreal.EditorAssetLibrary.find_asset_data(path)
    return str(data.asset_class_path.asset_name) if data and data.is_valid() else None


def run(report):
    eal = unreal.EditorAssetLibrary

    # 1. a pasta de passagem tem os 22
    missing = [old for old, _, _ in ASSETS if not eal.does_asset_exist("%s/%s" % (STAGING, old))]
    if missing:
        raise MigrateError("Faltam na pasta de passagem %s: %s" % (STAGING, ", ".join(missing)))
    report.append("passagem ok: %d assets em %s" % (len(ASSETS), STAGING))

    # 2. renomeia (cria as pastas de destino sozinho)
    for old, new, _ in ASSETS:
        src = "%s/%s" % (STAGING, old)
        dst = "%s/%s" % (DEST, new)
        if eal.does_asset_exist(dst):
            raise MigrateError("O destino já existe, não vou sobrescrever: " + dst)
        if not eal.rename_asset(src, dst):
            raise MigrateError("rename_asset falhou: %s -> %s" % (src, dst))
        report.append("renomeado: %s -> %s" % (old, new))

    # 3. apaga a passagem (só redirecionadores restam)
    if eal.does_directory_exist("/Game/Vehicles"):
        if not eal.delete_directory("/Game/Vehicles"):
            raise MigrateError("Não consegui apagar a pasta de passagem /Game/Vehicles")
        report.append("pasta de passagem apagada: /Game/Vehicles")

    # 4. conferências
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    options = unreal.AssetRegistryDependencyOptions()
    options.include_hard_package_references = True
    options.include_soft_package_references = True
    problems = []
    for old, new, expected in ASSETS:
        path = "%s/%s" % (DEST, new)
        if not eal.does_asset_exist(path):
            problems.append("não existe: " + path)
            continue
        found = asset_class(path)
        if found != expected:
            problems.append("%s: classe %s, esperava %s" % (path, found, expected))
        for dep in registry.get_dependencies(unreal.Name(path), options) or []:
            if str(dep).startswith("/Game/Vehicles"):
                problems.append("%s ainda aponta para %s" % (path, dep))
        if not eal.save_asset(path, only_if_is_dirty=False):
            problems.append("não consegui salvar " + path)
    if problems:
        raise MigrateError("Conferências falharam:\n  " + "\n  ".join(problems))
    report.append("conferido: %d assets novos existem, com a classe certa, sem referência a /Game/Vehicles" % len(ASSETS))


def main():
    report = []
    error = None
    report_path = None
    try:
        args = parse_args(sys.argv[1:])
        report_path = resolve_report(args.report)
        run(report)
    except MigrateError as exc:
        error = str(exc)
    except Exception as exc:  # qualquer outro erro também derruba o processo, com mensagem
        error = "Erro inesperado: %r" % (exc,)

    if error:
        report.append("ERRO: " + error)
        unreal.log_error("[migrate_sports_car] " + error)

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
