"""
Carroceria com os alvos de deformação do dano por zona (lote 17, docs/regras-dano-visual.md), lado do Unreal.

Rodar SEM janela, pelo commandlet de Python do Unreal (testado no UE 5.8), pelo invólucro Tools/Unreal/damage-body.ps1,
que faz as três etapas em ordem:
    1. --mode export: exporta SM_SportsCar e SM_SportsCar_Glass para .fbx (a lataria e os faróis andam juntos) e grava
       wheels.json com a posição dos eixos e o raio da roda (dos ossos Phys_Wheel_* do SKEL_SportsCar);
    2. (fora daqui) Tools/Blender/make_damage_morphs.py une as duas, cria os 5 alvos e o osso "root";
    3. --mode import: importa o resultado como SK_DNSportsCarBody (malha com esqueleto SKEL_DNSportsCarBody, sem
       física), com os alvos ligados e as normais do arquivo, e põe nos slots os mesmos materiais das malhas de origem.

Conferências do import (termina com erro se falhar): os 5 alvos existem com os nomes de FDNDamageMorph::TargetName, e
os limites da malha nova batem com os das malhas de origem (eixos e escala certos na ida e volta pelo Blender).

LIMITE DE SEGURANÇA: só grava em /Game/Detonauto/Vehicle/SportsCar e os .fbx e o relatório dentro da pasta do projeto.
"""

import sys

# O Python embutido da engine gravaria .pyc dentro da instalação do Unreal. Desliga ANTES de qualquer outro import.
sys.dont_write_bytecode = True

import argparse
import json
import os

import unreal

CAR_DIR = "/Game/Detonauto/Vehicle/SportsCar"
SOURCES = ("SM_SportsCar", "SM_SportsCar_Glass")
BODY = "SK_DNSportsCarBody"
SKELETON = "SKEL_DNSportsCarBody"
MORPHS = ("DN_Front", "DN_Rear", "DN_Left", "DN_Right", "DN_Roof")
WHEEL_BONES = ("Phys_Wheel_FL", "Phys_Wheel_FR", "Phys_Wheel_BL", "Phys_Wheel_BR")
BOUNDS_TOLERANCE_CM = 2.0


class BodyError(RuntimeError):
    """Erro esperado, com mensagem para o usuário."""


def project_dir():
    return os.path.normpath(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))


def inside_project(path):
    full = os.path.normcase(os.path.abspath(path))
    root = os.path.normcase(project_dir())
    if not full.startswith(root + os.sep):
        raise BodyError("Caminho recusado (fora do projeto): " + path)
    return os.path.abspath(path)


def load(name, cls):
    asset = unreal.load_asset(CAR_DIR + "/" + name)
    if not isinstance(asset, cls):
        raise BodyError("Asset ausente ou de outra classe: %s/%s" % (CAR_DIR, name))
    return asset


def export_sources(out_dir, report):
    out_dir = inside_project(out_dir)
    os.makedirs(out_dir, exist_ok=True)
    for name in SOURCES:
        task = unreal.AssetExportTask()
        task.object = load(name, unreal.StaticMesh)
        task.filename = os.path.join(out_dir, name + ".fbx")
        task.automated = True
        task.replace_identical = True
        task.prompt = False
        options = unreal.FbxExportOption()
        options.set_editor_property("collision", False)
        options.set_editor_property("level_of_detail", False)
        task.options = options
        if not unreal.Exporter.run_asset_export_task(task) or not os.path.isfile(task.filename):
            raise BodyError("Falhou ao exportar " + name)
        report.append("exportado: %s (%d bytes)" % (task.filename, os.path.getsize(task.filename)))
    export_wheels(out_dir, report)


def export_wheels(out_dir, report):
    """
    Onde ficam as rodas (eixos em X e raio, em cm, no espaço do carro): o Blender usa para o amassado da frente e da traseira
    parar antes do para-lama. Sem isso a lataria recuava para trás da roda (primeira captura do lote 17).
    """
    skeleton = load("SKEL_SportsCar", unreal.Skeleton)
    pose = unreal.AnimPoseExtensions.get_reference_pose(skeleton)
    xs = {}
    for bone in WHEEL_BONES:
        xs[bone] = unreal.AnimPoseExtensions.get_bone_pose(pose, bone, unreal.AnimPoseSpaces.WORLD).translation.x
    wheel = load("SM_SportsCar_Wheel", unreal.StaticMesh).get_bounds()
    data = {
        "front_axle_cm": (xs["Phys_Wheel_FL"] + xs["Phys_Wheel_FR"]) / 2.0,
        "rear_axle_cm": (xs["Phys_Wheel_BL"] + xs["Phys_Wheel_BR"]) / 2.0,
        "wheel_radius_cm": wheel.box_extent.z,
    }
    if not data["front_axle_cm"] > data["rear_axle_cm"]:
        raise BodyError("Eixos estranhos (a frente devia ter X maior): %s" % data)
    path = os.path.join(out_dir, "wheels.json")
    with open(path, "w", encoding="utf-8") as handle:
        json.dump(data, handle)
    report.append("rodas: %s" % ", ".join("%s %.1f" % (k, v) for k, v in data.items()))


def union_bounds(meshes):
    lo = [float("inf")] * 3
    hi = [float("-inf")] * 3
    for mesh in meshes:
        b = mesh.get_bounds()
        o = (b.origin.x, b.origin.y, b.origin.z)
        e = (b.box_extent.x, b.box_extent.y, b.box_extent.z)
        for i in range(3):
            lo[i] = min(lo[i], o[i] - e[i])
            hi[i] = max(hi[i], o[i] + e[i])
    return lo, hi


def morph_names(mesh):
    try:
        return [str(n) for n in mesh.get_all_morph_target_names()]
    except AttributeError:
        return [str(t.get_name()) for t in mesh.get_editor_property("morph_targets")]


def ensure_usage(material, report):
    """
    Sem as flags de uso de malha com esqueleto e de alvos de deformação, o jogo troca o material pelo padrão (xadrez cinza):
    visto na primeira captura do lote 17. O M_DNDentParent já nasce com elas (create_dent_material.py); o dos faróis é do template.
    """
    if material is None:
        raise BodyError("Slot com material sem material-base")
    changed = []
    for flag in ("used_with_skeletal_mesh", "used_with_morph_targets"):
        if not material.get_editor_property(flag):
            material.set_editor_property(flag, True)
            changed.append(flag)
    if changed:
        unreal.MaterialEditingLibrary.recompile_material(material)
        path = material.get_path_name().split(".")[0]
        if not unreal.EditorAssetLibrary.save_asset(path, only_if_is_dirty=False):
            raise BodyError("Não consegui salvar " + path)
        report.append("uso ligado em %s: %s" % (material.get_name(), ", ".join(changed)))


def import_body(fbx, report):
    fbx = inside_project(fbx)
    if not os.path.isfile(fbx):
        raise BodyError("Arquivo não encontrado: " + fbx)
    sources = [load(name, unreal.StaticMesh) for name in SOURCES]

    task = unreal.AssetImportTask()
    task.filename = fbx
    task.destination_path = CAR_DIR
    task.destination_name = BODY
    task.automated = True
    task.replace_existing = True
    task.save = False
    opts = unreal.FbxImportUI()
    opts.set_editor_property("import_mesh", True)
    opts.set_editor_property("import_as_skeletal", True)
    opts.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_SKELETAL_MESH)
    opts.set_editor_property("import_materials", False)
    opts.set_editor_property("import_textures", False)
    opts.set_editor_property("import_animations", False)
    opts.set_editor_property("create_physics_asset", False)  # a física é a do SK_SportsCar; esta malha é só visual
    skeleton = unreal.load_asset(CAR_DIR + "/" + SKELETON)
    if isinstance(skeleton, unreal.Skeleton):
        opts.set_editor_property("skeleton", skeleton)
    data = opts.get_editor_property("skeletal_mesh_import_data")
    data.set_editor_property("import_morph_targets", True)
    # a cor de vértice traz onde cada zona amassa mais (o desgaste da pintura usa, lote 17b)
    data.set_editor_property("vertex_color_import_option", unreal.VertexColorImportOption.REPLACE)
    data.set_editor_property("normal_import_method", unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS)
    data.set_editor_property("convert_scene", True)
    task.options = opts
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])

    body = load(BODY, unreal.SkeletalMesh)
    if not isinstance(skeleton, unreal.Skeleton):
        made = body.get_editor_property("skeleton")
        if made is None:
            raise BodyError("A importação não criou o esqueleto")
        if not unreal.EditorAssetLibrary.rename_asset(made.get_path_name().split(".")[0], CAR_DIR + "/" + SKELETON):
            raise BodyError("Não consegui renomear o esqueleto para " + SKELETON)
        body = load(BODY, unreal.SkeletalMesh)

    # mesmos materiais das malhas de origem, pelo nome que veio no .fbx
    by_name = {}
    for mesh in sources:
        for slot in mesh.get_editor_property("static_materials"):
            material = slot.get_editor_property("material_interface")
            if material is not None:
                by_name[material.get_name()] = material
    slots = body.get_editor_property("materials")
    for i in range(len(slots)):
        slot = slots[i]  # cópia do struct: mexer nela não muda o array, por isso volta pelo índice (visto no lote 17)
        name = str(slot.get_editor_property("material_slot_name"))
        material = by_name.get(name)
        if material is None:
            raise BodyError("Slot sem material de origem: %s (conhecidos: %s)" % (name, sorted(by_name)))
        slot.set_editor_property("material_interface", material)
        slots[i] = slot
        report.append("slot %s: %s" % (name, material.get_path_name()))
        ensure_usage(material.get_base_material(), report)
    body.set_editor_property("materials", slots)
    check_materials(body, by_name, "logo depois de pôr", report)

    names = morph_names(body)
    report.append("alvos: %s" % names)
    missing = [m for m in MORPHS if m not in names]
    if missing:
        raise BodyError("Alvos ausentes: %s" % missing)

    lo, hi = union_bounds(sources)
    b = body.get_bounds()
    got_lo = (b.origin.x - b.box_extent.x, b.origin.y - b.box_extent.y, b.origin.z - b.box_extent.z)
    got_hi = (b.origin.x + b.box_extent.x, b.origin.y + b.box_extent.y, b.origin.z + b.box_extent.z)
    report.append("limites das origens: %s a %s" % (tuple(round(v, 1) for v in lo), tuple(round(v, 1) for v in hi)))
    report.append("limites da malha nova: %s a %s" % (tuple(round(v, 1) for v in got_lo), tuple(round(v, 1) for v in got_hi)))
    worst = max(max(abs(got_lo[i] - lo[i]), abs(got_hi[i] - hi[i])) for i in range(3))
    if worst > BOUNDS_TOLERANCE_CM:
        raise BodyError("Limites não batem (diferença de %.1f cm): eixo ou escala errados na ida e volta" % worst)

    for path in (CAR_DIR + "/" + BODY, CAR_DIR + "/" + SKELETON):
        if not unreal.EditorAssetLibrary.save_asset(path, only_if_is_dirty=False):
            raise BodyError("Não consegui salvar " + path)
    report.append("salvo: %s e %s" % (BODY, SKELETON))
    check_materials(load(BODY, unreal.SkeletalMesh), by_name, "depois de salvar", report)


def check_materials(body, by_name, when, report):
    for slot in body.get_editor_property("materials"):
        name = str(slot.get_editor_property("material_slot_name"))
        got = slot.get_editor_property("material_interface")
        want = by_name.get(name)
        if got is None or want is None or got.get_path_name() != want.get_path_name():
            raise BodyError("Slot %s ficou com %s (%s)" % (name, got.get_path_name() if got else None, when))
    report.append("materiais conferidos %s" % when)


def main():
    parser = argparse.ArgumentParser(prog="damage_body.py")
    parser.add_argument("--mode", choices=("export", "import"), required=True)
    parser.add_argument("--dir", required=True, help="pasta dos .fbx (dentro do projeto)")
    args = parser.parse_args(sys.argv[1:])
    report = []
    report_path = os.path.join(project_dir(), "Saved", "DamageBodyReport.txt")
    error = None
    try:
        if args.mode == "export":
            export_sources(args.dir, report)
        else:
            import_body(os.path.join(args.dir, BODY + ".fbx"), report)
    except BodyError as exc:
        error = str(exc)
    except Exception as exc:  # noqa: BLE001
        error = "Erro inesperado: %r" % (exc,)
    if error:
        report.append("ERRO: " + error)
    os.makedirs(os.path.dirname(report_path), exist_ok=True)
    with open(report_path, "w", encoding="utf-8") as handle:
        handle.write("\n".join(report) + "\n")
    for line in report:
        unreal.log("[damage_body] " + line)
    if error:
        unreal.log_error("[damage_body] " + error)


main()
