"""
Poste de iluminação moderno (lote 16b), feito do zero por script: coluna cônica de aço pintado, braço curvo e luminária de LED
achatada. Conteúdo próprio do projeto, sem textura de terceiros. Substitui o lampião antigo da cena de aparência.

Saída no formato dos modelos do Poly Haven que o Tools/Unreal/create_lookdev.py já importa:
    <out>/dn_street_light.fbx
    <out>/textures/dn_street_light_{diff,rough,metal}_1k.png          (coluna, braço e corpo da luminária)
    <out>/textures/dn_street_light_lente_{diff,rough,metal}_1k.png    (a lente embaixo da luminária)
Os nomes dos materiais (dn_street_light e dn_street_light_lente) casam os slots com as texturas.

Eixos: pivô no pé da coluna, braço apontando para +X (no Unreal, o yaw do ator gira o braço para a rua). Unidades em metros no
Blender; o FBX chega em centímetros (mesmas opções do export_fbx_batch.py, testadas no Blender 5.2.2 e no Unreal 5.8).

Uso:
    blender -b --python-exit-code 1 -P Tools/Blender/make_street_light.py -- --out Sandbox/modelos_cc0/dn_street_light
"""

import math
import os
import struct
import sys
import zlib

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
ALLOWED_ROOT = os.path.abspath(os.path.join(SCRIPT_DIR, "..", "..", "Sandbox"))

POLE_HEIGHT = 8.0       # m: poste de avenida
POLE_BASE_R = 0.095
POLE_TOP_R = 0.055
ARM_REACH = 1.7         # m: quanto o braço avança sobre a pista
HEAD_SIZE = (0.72, 0.30, 0.07)  # comprimento, largura e espessura da luminária
# cor (sRGB 0-255), aspereza e metal (0-255) de cada material
MATERIALS = {
    "dn_street_light": (78, 120, 0),        # aço pintado cinza-grafite, acetinado
    "dn_street_light_lente": (205, 40, 0),  # policarbonato leitoso
}


class LightError(RuntimeError):
    pass


def solid_png(path, value):
    """PNG 4x4 de um valor só (como as texturas lisas do create_lookdev.py)."""
    row = b"\x00" + bytes([value, value, value]) * 4

    def chunk(kind, data):
        return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data) & 0xFFFFFFFF)

    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "wb") as handle:
        handle.write(b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", 4, 4, 8, 2, 0, 0, 0))
                     + chunk(b"IDAT", zlib.compress(row * 4)) + chunk(b"IEND", b""))


def build(out_dir):
    import bpy
    import bmesh
    from mathutils import Matrix, Vector

    bpy.ops.wm.read_factory_settings(use_empty=True)
    mats = {}
    for name in MATERIALS:
        mats[name] = bpy.data.materials.new(name)

    bm = bmesh.new()

    def add_cone(r1, r2, depth, z, segments=24):
        bmesh.ops.create_cone(bm, cap_ends=True, cap_tris=False, segments=segments, radius1=r1, radius2=r2, depth=depth,
                              matrix=Matrix.Translation((0.0, 0.0, z + depth / 2.0)))

    add_cone(0.17, 0.17, 0.03, 0.0)                         # chapa da base
    add_cone(0.13, 0.115, 0.55, 0.03)                       # capa da base (parafusos escondidos)
    add_cone(POLE_BASE_R, POLE_TOP_R, POLE_HEIGHT - 0.58, 0.58)  # coluna cônica
    add_cone(POLE_TOP_R, 0.03, 0.06, POLE_HEIGHT)           # tampa

    # braço: tubo que sai da coluna a 0,45 m do topo e faz uma curva suave até a horizontal, subindo um pouco
    arm_r = 0.034
    points = []
    steps = 14
    for i in range(steps + 1):
        t = i / steps
        x = ARM_REACH * t
        z = POLE_HEIGHT - 0.45 + 0.55 * math.sin(min(1.0, t * 1.6) * math.pi / 2.0) + 0.04 * t
        points.append(Vector((x, 0.0, z)))
    ring = 12
    rings = []
    for i, p in enumerate(points):
        tangent = (points[min(i + 1, steps)] - points[max(i - 1, 0)]).normalized()
        side = Vector((0.0, 1.0, 0.0))
        up = tangent.cross(side).normalized()
        verts = []
        for k in range(ring):
            a = 2.0 * math.pi * k / ring
            verts.append(bm.verts.new(p + (side * math.cos(a) + up * math.sin(a)) * arm_r))
        rings.append(verts)
    for a, b in zip(rings, rings[1:]):
        for k in range(ring):
            bm.faces.new((a[k], a[(k + 1) % ring], b[(k + 1) % ring], b[k]))
    bm.faces.new(list(reversed(rings[0])))
    bm.faces.new(rings[-1])

    # luminária: caixa achatada com as quinas chanfradas, inclinada 5 graus para cima na ponta
    hx, hy, hz = HEAD_SIZE[0] / 2.0, HEAD_SIZE[1] / 2.0, HEAD_SIZE[2] / 2.0
    head_center = points[-1] + Vector((hx - 0.08, 0.0, -0.01))
    tilt = Matrix.Rotation(math.radians(-5.0), 4, "Y")
    head = bmesh.ops.create_cube(bm, size=1.0, matrix=Matrix.Translation(head_center) @ tilt @ Matrix.Diagonal((HEAD_SIZE[0], HEAD_SIZE[1], HEAD_SIZE[2], 1.0)))
    head_edges = list({e for v in head["verts"] for e in v.link_edges})
    bmesh.ops.bevel(bm, geom=head_edges, offset=0.018, segments=3, affect="EDGES", profile=0.5)

    # lente: placa um pouco menor, logo abaixo da luminária (o único pedaço com o segundo material)
    lens_center = head_center + tilt.to_3x3() @ Vector((0.03, 0.0, -hz - 0.004))
    lens = bmesh.ops.create_cube(bm, size=1.0, matrix=Matrix.Translation(lens_center) @ tilt @ Matrix.Diagonal((HEAD_SIZE[0] * 0.78, HEAD_SIZE[1] * 0.78, 0.008, 1.0)))
    lens_faces = {f for v in lens["verts"] for f in v.link_faces}

    for f in bm.faces:
        f.material_index = 1 if f in lens_faces else 0
        f.smooth = True
    # quinas vivas acima de 35 graus (a base, a tampa e a caixa da luminária); o resto, liso
    for e in bm.edges:
        e.smooth = not (len(e.link_faces) == 2 and e.calc_face_angle(0.0) > math.radians(35.0))

    mesh = bpy.data.meshes.new("dn_street_light")
    bm.to_mesh(mesh)
    bm.free()
    mesh.materials.append(mats["dn_street_light"])
    mesh.materials.append(mats["dn_street_light_lente"])
    obj = bpy.data.objects.new("dn_street_light", mesh)
    bpy.context.scene.collection.objects.link(obj)
    bpy.context.view_layer.objects.active = obj
    obj.select_set(True)

    os.makedirs(out_dir, exist_ok=True)
    fbx = os.path.join(out_dir, "dn_street_light.fbx")
    bpy.ops.export_scene.fbx(filepath=fbx, use_selection=True, object_types={"MESH"}, axis_forward="-Z", axis_up="Y",
                             apply_unit_scale=True, global_scale=1.0, apply_scale_options="FBX_SCALE_NONE",
                             bake_space_transform=True, use_mesh_modifiers=True, mesh_smooth_type="OFF",
                             add_leaf_bones=False, path_mode="AUTO")
    for name, (color, rough, metal) in MATERIALS.items():
        for kind, value in (("diff", color), ("rough", rough), ("metal", metal)):
            solid_png(os.path.join(out_dir, "textures", "%s_%s_1k.png" % (name, kind)), value)
    return fbx, len(mesh.vertices), len(mesh.polygons)


def main(argv):
    args = argv[argv.index("--") + 1:] if "--" in argv else []
    if len(args) != 2 or args[0] != "--out":
        print("[make_street_light] uso: -- --out <pasta dentro de Sandbox>", file=sys.stderr)
        return 1
    out_dir = os.path.abspath(args[1])
    if not os.path.normcase(out_dir).startswith(os.path.normcase(ALLOWED_ROOT) + os.sep):
        print("[make_street_light] saída recusada (fora de Sandbox/): " + out_dir, file=sys.stderr)
        return 1
    try:
        fbx, verts, faces = build(out_dir)
    except Exception as exc:  # noqa: BLE001
        print("[make_street_light] ERRO: %r" % (exc,), file=sys.stderr)
        return 1
    print("[make_street_light] %s: %d vértices, %d faces" % (fbx, verts, faces))
    return 0


if __name__ == "__main__":
    code = main(sys.argv)
    if code:
        sys.exit(code)
