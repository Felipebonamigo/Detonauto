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

--height-m é a altura do corte no espaço do modelo, em metros (para o jacarandá, que a cena põe com escala 0,6, 15 cm na cena
são 0,25 m no modelo).

--trunk-top-m (opcional, só a parte de cima): em vez da colisão automática (um convexo único do tamanho da árvore, que ao cair
encostava nos prédios e travava no ar), grava collision.json ao lado do .fbx com duas cápsulas: uma no tronco (do corte até
--trunk-top-m) e uma fina na copa, no mesmo eixo, com raio --canopy-radius-m (padrão 1,7 m no modelo, 1 m na cena). Caixa na
copa não serve: deitada, a árvore ficava apoiada no canto dela, inclinada e com o pé no ar. O create_lookdev.py aplica as duas
formas na malha importada. (Malhas UCX_ no FBX não servem: a importação da 5.8 gera convexos a mais.)
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
    parser.add_argument("--trunk-top-m", type=float, default=0.0, help="colisão tronco + copa na parte de cima (topo do tronco, m)")
    parser.add_argument("--canopy-radius-m", type=float, default=1.7)
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


def trunk_canopy_collision(obj, cut_m, trunk_top_m, canopy_radius):
    """Tronco e copa (duas cápsulas no eixo do tronco), em metros no espaço do Blender. Devolve o dicionário e uma linha de relatório."""
    import numpy as np
    co = np.empty(len(obj.data.vertices) * 3, dtype=np.float64)
    obj.data.vertices.foreach_get("co", co)
    co = co.reshape(-1, 3)
    band = co[(co[:, 2] >= cut_m) & (co[:, 2] <= cut_m + 0.6)]  # o tronco logo acima do corte
    if len(band) < 8:
        raise SystemExit("[split_prop] poucos vértices no tronco logo acima do corte; confira --height-m")
    cx, cy = float(np.median(band[:, 0])), float(np.median(band[:, 1]))
    radius = float(np.percentile(np.hypot(band[:, 0] - cx, band[:, 1] - cy), 75))
    canopy = co[co[:, 2] > trunk_top_m]
    if len(canopy) < 8:
        raise SystemExit("[split_prop] nada acima de --trunk-top-m; confira o valor")
    top = float(canopy[:, 2].max())
    all_lo, all_hi = co.min(axis=0), co.max(axis=0)
    data = {
        "espaco": "Blender, metros (a Unreal inverte o Y)",
        "tronco": {"centro": [cx, cy, (cut_m + trunk_top_m) / 2.0], "raio": radius, "altura": trunk_top_m - cut_m},
        "copa": {"centro": [cx, cy, (trunk_top_m + top) / 2.0], "raio": canopy_radius, "altura": top - trunk_top_m},
        "centro_da_malha": [float(v) for v in (all_lo + all_hi) / 2.0],
    }
    line = ("colisão: tronco raio %.2f m de %.2f a %.2f m em (%.2f, %.2f); copa raio %.2f m de %.2f a %.2f m"
            % (radius, cut_m, trunk_top_m, cx, cy, canopy_radius, trunk_top_m, top))
    return data, line


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
        if not keep_below and args.trunk_top_m > args.height_m:
            import json
            data, line = trunk_canopy_collision(obj, args.height_m, args.trunk_top_m, args.canopy_radius_m)
            os.makedirs(folder, exist_ok=True)
            with open(os.path.join(folder, "collision.json"), "w", encoding="utf-8") as f:
                json.dump(data, f, indent=2, ensure_ascii=False)
            report.append(line)
        path = export(obj, folder, name)
        report.append("%s: %s (%d de %d vértices, %d faces)" % ("de baixo" if keep_below else "de cima", path, verts, before, faces))
    for line in report:
        print("[split_prop] " + line)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
