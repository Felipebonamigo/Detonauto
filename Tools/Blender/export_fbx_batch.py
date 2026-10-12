"""
Exporta em lote os objetos de uma coleção do Blender como FBX, com escala, eixos e unidade
certos para o Unreal Engine.

*** NÃO TESTADO CONTRA O BLENDER REAL ***
O Blender não está instalado no PC onde este script foi escrito (nem no PATH, nem no registro,
nem em Program Files). Só a lógica que não usa o Blender (argumentos, trava da pasta de saída,
relatório) foi testada, com um módulo `bpy` de mentira. Todas as chamadas de `bpy` abaixo foram
escritas de memória e estão marcadas como SUPOSIÇÃO onde não foram conferidas contra a
documentação do Blender instalado. Para validar: instale o Blender e rode o exemplo abaixo.

Uso (modo background, sem janela):

    blender -b arquivo.blend --python-exit-code 1 -P Tools/Blender/export_fbx_batch.py -- ^
        --collection Carro --out Sandbox/saida_fbx

  (`--python-exit-code 1` faz o Blender devolver código 1 se o script falhar; sem ele o código
   de saída seria 0 mesmo com erro. Tudo depois do `--` é lido por este script.)

Argumentos
    --collection   nome da coleção cujos objetos (malhas) serão exportados
    --out          pasta de saída dos .fbx
    --report       arquivo de relatório (padrão: <out>/relatorio_export.txt)
    --forward      eixo para frente (padrão -Z)        --up  eixo para cima (padrão Y)
    --center       exporta cada objeto com a origem em (0,0,0) (padrão: ligado; --no-center desliga)
    --allowed-root pasta-raiz onde é permitido gravar. Padrão: a pasta Sandbox/ deste repositório.
                   Só passe outra raiz se o Felipe aprovou.

Limite de segurança: a pasta --out precisa ficar DENTRO de --allowed-root. Qualquer outra é recusada
antes de gravar qualquer coisa.
"""

import os
import sys

# Esta parte não depende do Blender e é testável fora dele.

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
DEFAULT_ALLOWED_ROOT = os.path.abspath(os.path.join(SCRIPT_DIR, "..", "..", "Sandbox"))

# SUPOSIÇÃO: -Z para frente e Y para cima é o padrão do exportador FBX do Blender e a combinação
# mais citada para Unreal. O Unreal converte do eixo gravado no FBX para o seu (X à frente, Z para
# cima). Se, depois de importar, a frente do modelo apontar para o lado errado, troque com
# --forward / --up. Confira com um modelo que tenha uma frente óbvia (um carro).
DEFAULT_FORWARD = "-Z"
DEFAULT_UP = "Y"
VALID_AXES = ("X", "Y", "Z", "-X", "-Y", "-Z")


class ExportError(RuntimeError):
    """Erro esperado, com mensagem para o usuário."""


def script_args(argv):
    """Só o que vem depois do '--' (o resto é do Blender)."""
    return argv[argv.index("--") + 1:] if "--" in argv else []


def parse_args(args):
    import argparse
    parser = argparse.ArgumentParser(prog="export_fbx_batch.py", add_help=False)
    parser.add_argument("--collection", required=True)
    parser.add_argument("--out", required=True)
    parser.add_argument("--report", default=None)
    parser.add_argument("--forward", default=DEFAULT_FORWARD, choices=VALID_AXES)
    parser.add_argument("--up", default=DEFAULT_UP, choices=VALID_AXES)
    parser.add_argument("--allowed-root", default=DEFAULT_ALLOWED_ROOT)
    center = parser.add_mutually_exclusive_group()
    center.add_argument("--center", dest="center", action="store_true", default=True)
    center.add_argument("--no-center", dest="center", action="store_false")
    try:
        parsed = parser.parse_args(args)
    except SystemExit:
        raise ExportError("Argumentos inválidos. Uso: --collection <nome> --out <pasta> "
                          "[--report <arquivo>] [--forward -Z] [--up Y] [--no-center] [--allowed-root <pasta>]")
    if parsed.forward.lstrip("-") == parsed.up.lstrip("-"):
        raise ExportError("--forward e --up precisam ser eixos diferentes.")
    return parsed


def is_inside(path, root):
    path = os.path.normcase(os.path.abspath(path))
    root = os.path.normcase(os.path.abspath(root))
    return path == root or path.startswith(root + os.sep)


def resolve_out_dir(out, allowed_root):
    full = os.path.abspath(out)
    if not is_inside(full, allowed_root):
        raise ExportError("Pasta de saída recusada: '%s' fica fora de '%s'. Só grava dentro da pasta permitida."
                          % (full, os.path.abspath(allowed_root)))
    return full


def safe_filename(name):
    keep = "".join(c if (c.isalnum() or c in "_-") else "_" for c in name)
    return keep or "objeto"


def fbx_options(forward, up):
    """Opções de bpy.ops.export_scene.fbx, com o porquê de cada uma."""
    return {
        # Exporta só o que está selecionado (um objeto por arquivo).
        "use_selection": True,
        # Só malhas: luzes, câmeras e vazios não interessam a um Static Mesh.
        "object_types": {"MESH"},
        # Eixos gravados no arquivo. O Unreal lê esses eixos e converte para o seu sistema.
        "axis_forward": forward,
        "axis_up": up,
        # Unidade: o Blender trabalha em metros, o Unreal em centímetros. Com apply_unit_scale
        # ligado o FBX carrega a unidade do cena e 1 m no Blender vira 100 unidades no Unreal.
        # SUPOSIÇÃO: vale para uma cena com Unit System = Metric e Unit Scale = 1.
        "apply_unit_scale": True,
        # global_scale 1 + FBX_SCALE_NONE: nenhuma escala extra por cima da conversão acima.
        "global_scale": 1.0,
        "apply_scale_options": "FBX_SCALE_NONE",
        # Aplica a conversão de eixos nos vértices em vez de só gravar uma rotação no objeto,
        # para o modelo não chegar ao Unreal girado 90 graus. SUPOSIÇÃO: costuma ser o desejado
        # para Static Mesh; confira ao importar.
        "bake_space_transform": True,
        # Modificadores (Subdivision, Mirror...) valem na exportação.
        "use_mesh_modifiers": True,
        # Suavização por face é o que o Unreal espera de malhas importadas.
        "mesh_smooth_type": "FACE",
        # Sem ossos extras: são malhas estáticas.
        "add_leaf_bones": False,
        # Caminho absoluto das texturas no arquivo (não copia texturas junto).
        "path_mode": "AUTO",
    }


def write_report(path, lines):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "w", encoding="utf-8") as handle:
        handle.write("\n".join(lines) + "\n")


# Esta parte usa o Blender (bpy). SUPOSIÇÃO: nomes e parâmetros conferidos só de memória.

def export_collection(args, report):
    import bpy  # só existe dentro do Blender

    out_dir = resolve_out_dir(args.out, args.allowed_root)
    collection = bpy.data.collections.get(args.collection)
    if collection is None:
        raise ExportError("Coleção não encontrada no arquivo: '%s'. Existentes: %s"
                          % (args.collection, ", ".join(c.name for c in bpy.data.collections) or "(nenhuma)"))

    objects = sorted((o for o in collection.all_objects if o.type == "MESH"), key=lambda o: o.name)
    if not objects:
        raise ExportError("A coleção '%s' não tem objetos de malha." % args.collection)

    os.makedirs(out_dir, exist_ok=True)
    report.append("coleção: %s" % args.collection)
    report.append("saída: %s" % out_dir)
    report.append("eixos: frente %s, cima %s | unidade: centímetros (apply_unit_scale)" % (args.forward, args.up))

    options = fbx_options(args.forward, args.up)
    failed = []
    for obj in objects:
        path = os.path.join(out_dir, safe_filename(obj.name) + ".fbx")
        original_location = obj.location.copy()
        try:
            # só este objeto selecionado e ativo
            bpy.ops.object.select_all(action="DESELECT")
            obj.select_set(True)
            bpy.context.view_layer.objects.active = obj

            if args.center:
                obj.location = (0.0, 0.0, 0.0)  # pivô do Static Mesh na origem

            # Aplica escala e rotação: o objeto sai com escala 1 e rotação 0, sem que o Unreal
            # herde uma escala estranha. (Posição fica de fora, ela foi tratada acima.)
            bpy.ops.object.transform_apply(location=False, rotation=True, scale=True)

            bpy.ops.export_scene.fbx(filepath=path, **options)
            report.append("OK: %s -> %s" % (obj.name, os.path.basename(path)))
        except Exception as exc:  # noqa: BLE001 - registra e segue para o próximo
            failed.append(obj.name)
            report.append("FALHOU: %s (%r)" % (obj.name, exc))
        finally:
            if args.center:
                obj.location = original_location

    report.append("resumo: %d exportado(s), %d falha(s)" % (len(objects) - len(failed), len(failed)))
    if failed:
        raise ExportError("Falhou em: " + ", ".join(failed))
    return out_dir


def main(argv):
    report = []
    error = None
    report_path = None
    try:
        args = parse_args(script_args(argv))
        out_dir = resolve_out_dir(args.out, args.allowed_root)
        report_path = args.report or os.path.join(out_dir, "relatorio_export.txt")
        if args.report:
            report_path = resolve_out_dir(args.report, args.allowed_root)
        export_collection(args, report)
    except ExportError as exc:
        error = str(exc)
    except Exception as exc:  # noqa: BLE001
        error = "Erro inesperado: %r" % (exc,)

    if error:
        report.append("ERRO: " + error)
    if report_path:
        write_report(report_path, report)
    print("\n".join(report))
    if error:
        print("[export_fbx_batch] " + error, file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    code = main(sys.argv)
    if code:
        sys.exit(code)
