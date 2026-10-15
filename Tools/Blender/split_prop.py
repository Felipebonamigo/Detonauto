"""
Corta um modelo de peça de rua em duas malhas numa altura (lote 17c, docs/regras-mobiliario.md, "Visual"): a parte de baixo
(base do poste, toco da árvore) fica no lugar quando o carro bate; a de cima solta e tomba. As duas saem com o mesmo pivô do
original (no chão), então ficam encaixadas no mesmo ator; os cortes são fechados (sem buraco à vista).

Usado para o poste (dn_street_light, 10 cm) e o jacarandá do Poly Haven (40 cm na cena). O modelo do Poly Haven é CC0, que
permite modificar; a modificação fica anotada em docs/licencas.csv.

Saída no formato de pasta que o Tools/Unreal/create_lookdev.py importa (um .fbx por pasta). Os nomes dos materiais ficam os do
original, para a cena reaproveitar as instâncias de material dele.

Uso:
    blender -b --python-exit-code 1 -P Tools/Blender/split_prop.py -- --input <modelo.fbx> --height-m 0.10 ^
        --below Sandbox/modelos_cc0/dn_street_light_base --above Sandbox/modelos_cc0/dn_street_light_top

--height-m é a altura do corte no espaço do modelo, em metros (para o jacarandá, que a cena põe com escala 0,6, 40 cm na cena
são 0,667 m no modelo).
"""

import os
import sys

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
ALLOWED_ROOT = os.path.abspath(os.path.join(SCRIPT_DIR, "..", "..", "Sandbox"))


def parse(argv):
    import argparse
    args = argv[argv.index("--") + 1:] if "--" in argv else []
    parser = argparse.ArgumentParser(prog="split_prop.py")
    parser.add_argument("--input", required=True)
    parser.add_argument("--height-m", type=float, required=True)
    parser.add_argument("--below", required=True, help="pasta da parte de baixo (dentro de Sandbox/)")
    parser.add_argument("--above", required=True, help="pasta da parte de cima (dentro de Sandbox/)")
    parsed = parser.parse_args(args)
    for folder in (parsed.below, parsed.above):
        full = os.path.normcase(os.path.abspath(folder))
        if not full.startswith(os.path.normcase(ALLOWED_ROOT) + os.sep):
            raise SystemExit("[split_prop] pasta recusada (fora de Sandbox/): " + folder)
    if not parsed.height_m > 0.0:
        raise SystemExit("[split_prop] --height-m precisa ser maior que zero")
    return parsed


def load_joined(path):
    import bpy
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.fbx(filepath=os.path.abspath(path))
    meshes = [o for o in bpy.data.objects if o.type == "MESH"]
    for o in list(bpy.data.objects):
        if o.type != "MESH":
            bpy.data.objects.remove(o)
    bpy.ops.object.select_all(action="DESELECT")
    for o in meshes:
        o.select_set(True)
    bpy.context.view_layer.objects.active = meshes[0]
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)  # em metros, pivô no chão
    if len(meshes) > 1:
        bpy.ops.object.join()
    return bpy.context.view_layer.objects.active


def cut(obj, height, keep_below):
    """Corta no plano z = height, fecha o corte e joga fora o lado que não fica."""
    import bmesh
    bm = bmesh.new()
    bm.from_mesh(obj.data)
    geom = bm.verts[:] + bm.edges[:] + bm.faces[:]
    bmesh.ops.bisect_plane(bm, geom=geom, plane_co=(0.0, 0.0, height), plane_no=(0.0, 0.0, 1.0),
                           clear_inner=not keep_below, clear_outer=keep_below)
    # fecha os buracos que o corte abriu (só as bordas que ficaram exatamente na altura do corte)
    edges = [e for e in bm.edges if e.is_boundary and all(abs(v.co.z - height) < 1e-4 for v in e.verts)]
    if edges:
        bmesh.ops.holes_fill(bm, edges=edges, sides=0)
    bm.to_mesh(obj.data)
    bm.free()
    return len(obj.data.vertices), len(obj.data.polygons)


def export(obj, folder, name):
    import bpy
    os.makedirs(folder, exist_ok=True)
    obj.name = name
    obj.data.name = name
    bpy.ops.object.select_all(action="DESELECT")
    obj.select_set(True)
    bpy.context.view_layer.objects.active = obj
    path = os.path.join(os.path.abspath(folder), name + ".fbx")
    bpy.ops.export_scene.fbx(filepath=path, use_selection=True, object_types={"MESH"}, axis_forward="-Z", axis_up="Y",
                             apply_unit_scale=True, global_scale=1.0, apply_scale_options="FBX_SCALE_NONE",
                             bake_space_transform=True, use_mesh_modifiers=True, mesh_smooth_type="OFF",
                             add_leaf_bones=False, path_mode="AUTO")
    return path


def main(argv):
    args = parse(argv)
    import bpy
    report = []
    for keep_below, folder in ((True, args.below), (False, args.above)):
        obj = load_joined(args.input)
        before = len(obj.data.vertices)
        verts, faces = cut(obj, args.height_m, keep_below)
        name = os.path.basename(os.path.normpath(folder))
        path = export(obj, folder, name)
        report.append("%s: %s (%d de %d vértices, %d faces)" % ("de baixo" if keep_below else "de cima", path, verts, before, faces))
    for line in report:
        print("[split_prop] " + line)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
