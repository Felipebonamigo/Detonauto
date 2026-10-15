"""
Separa as peças que soltam e os vidros da carroceria (lote 18b, contrato em docs/regras-pecas.md) e exporta cada uma com os mesmos 5
alvos de deformação da carroceria, mais a carroceria sem elas e com os "vãos" escuros atrás de cada abertura.

Entrada: os .fbx do carro exportados do Unreal (Tools/Unreal/damage_body.py --mode export) e um mapa por carro,
Tools/Blender/parts/<CarId>.json, que diz onde fica cada peça:

    "box": [xmin, xmax, ymin, ymax, zmin, zmax]   metros, eixos do Blender (frente +X, ESQUERDA +Y, cima +Z)
    "take": "faces" | "islands"                  faces: corta a casca pelos planos da caixa e leva as faces com o centro dentro;
                                                 islands: leva as cascas soltas inteiras com o centro dentro (aerofólio, lentes)
    "normal": [nx, ny, nz, minimo]               opcional: só faces com normal · (nx, ny, nz) >= minimo
    "material": "nome"                           opcional: só faces deste material
    "thickness": m                               espessura da peça (fecha a casca; o lado de dentro sai com o material escuro)
    "pocket": m                                  profundidade do vão escuro deixado na carroceria (0 = sem vão)
    "mirror": "Nome"                             gera também a peça espelhada (Y trocado de sinal) com este nome

As peças são tiradas NA ORDEM do mapa: uma face levada por uma peça não entra na seguinte.

A deformação é a do Tools/Blender/make_damage_morphs.py (importado como biblioteca): um campo espacial, função só da posição, com a
caixa do carro INTEIRO. Por isso a peça presa amassa igual à carroceria, sem fresta. A cor de vértice do desgaste também usa os picos
do carro inteiro.

Saída, em --out-dir (dentro de Sandbox/): SK_DN<CarId>Body.fbx e um SK_DN<CarId>_<Peça>.fbx por peça, cada um com o osso "root" e
os alvos DN_Front, DN_Rear, DN_Left, DN_Right e DN_Roof. Imagens de conferência em --renders (dentro de docs/img/).

Uso (Blender 5.2, sem janela):

    blender -b --python-exit-code 1 -P Tools/Blender/split_car_parts.py -- ^
        --inputs Sandbox/dano/SM_SportsCar.fbx Sandbox/dano/SM_SportsCar_Glass.fbx --map Tools/Blender/parts/SportsCar.json ^
        --wheels Sandbox/dano/wheels.json --out-dir Sandbox/pecas --renders docs/img/pecas
"""

import sys

# importar o make_damage_morphs gravaria __pycache__ dentro do repositório: desliga ANTES do import
sys.dont_write_bytecode = True

import json
import math
import os

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
if SCRIPT_DIR not in sys.path:
    sys.path.insert(0, SCRIPT_DIR)

import make_damage_morphs as mdm  # noqa: E402  (o mesmo campo dos alvos da carroceria)

# material escuro do lado de dentro das peças (o slot do chassi do carro: plástico preto e vidro fumê)
DARK_MATERIAL_HINT = "Chassi"
# lote 19: o vão ganha slot próprio (metal escuro com ruído, MI_DNVao em create_dent_material.py); com o do chassi, sem UV, ele saía
# preto chapado
VAO_MATERIAL = "MI_DNVao"
# normais do vão e das faces de dentro (não têm normal do arquivo): suaves entre faces a menos deste ângulo
SMOOTH_ANGLE_DEG = 60.0
# atributo de canto com a normal do arquivo (store_corner_normals; sai antes de exportar)
NORMAL_ATTR = "dn_normal"


class SplitError(RuntimeError):
    """Erro esperado, com mensagem para o usuário."""


def parse_args(args):
    import argparse
    parser = argparse.ArgumentParser(prog="split_car_parts.py", add_help=False)
    parser.add_argument("--inputs", nargs="+", required=True)
    parser.add_argument("--map", required=True)
    parser.add_argument("--wheels", required=True)
    parser.add_argument("--out-dir", required=True)
    parser.add_argument("--renders", default=None)
    parser.add_argument("--seed", type=int, default=17)
    parser.add_argument("--subdivide", type=int, default=1)
    parser.add_argument("--allowed-root", default=mdm.DEFAULT_ALLOWED_ROOT)
    parser.add_argument("--no-export", action="store_true")  # só as imagens (iterar as caixas sem gravar FBX)
    try:
        parsed = parser.parse_args(args)
    except SystemExit:
        raise SplitError("Argumentos inválidos. Uso: --inputs <a.fbx> [b.fbx] --map <mapa.json> --wheels <wheels.json> --out-dir <pasta> "
                         "[--renders <pasta>] [--seed 17] [--subdivide 1] [--no-export]")
    if not mdm.is_inside(parsed.out_dir, parsed.allowed_root):
        raise SplitError("Saída recusada: '%s' fica fora de '%s'." % (os.path.abspath(parsed.out_dir), parsed.allowed_root))
    if parsed.renders and not mdm.is_inside(parsed.renders, mdm.RENDERS_ROOT):
        raise SplitError("Pasta de imagens recusada: '%s' fica fora de '%s'." % (os.path.abspath(parsed.renders), mdm.RENDERS_ROOT))
    return parsed


def read_map(path):
    try:
        with open(path, encoding="utf-8") as handle:
            data = json.load(handle)
    except (OSError, ValueError) as exc:
        raise SplitError("Não consegui ler o mapa %s: %r" % (path, exc))
    car = data.get("car")
    parts = data.get("parts")
    if not car or not isinstance(parts, list) or not parts:
        raise SplitError("Mapa sem 'car' ou sem 'parts': %s" % path)
    expanded = []
    for p in parts:
        if "name" not in p or "box" not in p or len(p["box"]) != 6:
            raise SplitError("Peça sem 'name' ou com 'box' errada no mapa: %r" % (p,))
        expanded.append(p)
        if p.get("mirror"):
            m = dict(p)
            m["name"] = p["mirror"]
            b = p["box"]
            m["box"] = [b[0], b[1], -b[3], -b[2], b[4], b[5]]
            if "normal" in p:
                n = p["normal"]
                m["normal"] = [n[0], -n[1], n[2], n[3]]
            m.pop("mirror", None)
            expanded.append(m)
    names = [p["name"] for p in expanded]
    if len(set(names)) != len(names):
        raise SplitError("Nomes de peça repetidos no mapa: %s" % names)
    return car, expanded


# --- geometria (bmesh) ---

def inside(co, box, pad=0.0, planes=()):
    """Dentro da caixa e do lado de dentro (produto escalar <= 0) de cada plano extra [px, py, pz, nx, ny, nz]."""
    if not (box[0] - pad <= co.x <= box[1] + pad and box[2] - pad <= co.y <= box[3] + pad and box[4] - pad <= co.z <= box[5] + pad):
        return False
    for p in planes:
        if (co.x - p[0]) * p[3] + (co.y - p[1]) * p[4] + (co.z - p[2]) * p[5] > pad:
            return False
    return True


def material_index(obj, hint):
    for i, m in enumerate(obj.data.materials):
        if m and hint.lower() in m.name.lower():
            return i
    return None


def islands(bm):
    """Listas de faces conectadas (cascas soltas)."""
    seen = set()
    out = []
    for f in bm.faces:
        if f.index in seen:
            continue
        stack = [f]
        seen.add(f.index)
        group = []
        while stack:
            cur = stack.pop()
            group.append(cur)
            for e in cur.edges:
                for nf in e.link_faces:
                    if nf.index not in seen:
                        seen.add(nf.index)
                        stack.append(nf)
        out.append(group)
    return out


def cut_by_box(bm, box, extra=()):
    """Corta a casca pelos 6 planos da caixa e pelos planos extras, só na região perto dela: a borda da peça sai reta."""
    import bmesh
    from mathutils import Vector
    planes = [((box[0], 0, 0), (1, 0, 0)), ((box[1], 0, 0), (1, 0, 0)), ((0, box[2], 0), (0, 1, 0)),
              ((0, box[3], 0), (0, 1, 0)), ((0, 0, box[4]), (0, 0, 1)), ((0, 0, box[5]), (0, 0, 1))]
    planes += [((p[0], p[1], p[2]), (p[3], p[4], p[5])) for p in extra]
    for co, no in planes:
        faces = [f for f in bm.faces if any(inside(v.co, box, 0.05) for v in f.verts)]
        if not faces:
            continue
        geom = list({e for f in faces for e in f.edges}) + faces + list({v for f in faces for v in f.verts})
        bmesh.ops.bisect_plane(bm, geom=geom, dist=1e-5, plane_co=Vector(co), plane_no=Vector(no), clear_inner=False, clear_outer=False)
    bm.faces.ensure_lookup_table()


def pick_faces(bm, part, mat_index):
    """As faces da peça, conforme o mapa."""
    from mathutils import Vector
    box = part["box"]
    planes = part.get("planes", ())
    normal = part.get("normal")
    nvec = Vector(normal[:3]).normalized() if normal else None
    if part.get("take", "faces") == "islands":
        chosen = []
        for group in islands(bm):
            vs = {v for f in group for v in f.verts}
            lo = Vector((min(v.co.x for v in vs), min(v.co.y for v in vs), min(v.co.z for v in vs)))
            hi = Vector((max(v.co.x for v in vs), max(v.co.y for v in vs), max(v.co.z for v in vs)))
            if inside((lo + hi) / 2.0, box, 0.0, planes) and (mat_index is None or any(f.material_index == mat_index for f in group)):
                chosen.extend(group)
        return chosen
    cut_by_box(bm, box, planes)
    chosen = []
    for f in bm.faces:
        if not inside(f.calc_center_median(), box, 0.0, planes):
            continue
        if mat_index is not None and f.material_index != mat_index:
            continue
        if nvec is not None and f.normal.dot(nvec) < normal[3]:
            continue
        chosen.append(f)
    return chosen


def boundary_loops_edges(faces):
    """Arestas de borda do conjunto de faces (as que têm só uma face do conjunto)."""
    fs = set(faces)
    return [e for f in faces for e in f.edges if sum(1 for lf in e.link_faces if lf in fs) == 1]


class Core:
    """
    O eixo central do carro: um segmento ao longo de X, a 45% da altura, 0,9 m antes de cada ponta. "Para dentro" de um ponto é rumo ao
    ponto mais próximo dele. A normal da face não serve: o FBX do Unreal chega espelhado no Blender e o sinal dela não é confiável
    nesta malha (a primeira tentativa afundou os vãos para fora e o carro saiu espetado).
    """

    def __init__(self, box):
        from mathutils import Vector
        zc = box.zmin + 0.45 * box.height
        self.a = Vector((box.xmin + 0.9, 0.0, zc))
        self.b = Vector((box.xmax - 0.9, 0.0, zc))

    def inward(self, co):
        from mathutils import Vector
        ab = self.b - self.a
        t = max(0.0, min(1.0, (co - self.a).dot(ab) / ab.length_squared))
        d = (self.a + ab * t) - co
        return d.normalized() if d.length > 1e-6 else Vector((0.0, 0.0, -1.0))


CORE = None  # definido em main() com a caixa do carro inteiro


def vertex_normals(faces):
    """Direção PARA FORA por vértice (o contrário de Core.inward), para afundar as cópias."""
    return {v: -CORE.inward(v.co) for f in faces for v in f.verts}


def build_shell(src_faces, depth, dark, keep_outer, outer_flip=False):
    """
    Nova malha (bmesh) com as faces copiadas e, atrás delas, uma cópia afundada 'depth' metros rumo ao eixo do carro, ligadas pela borda.
    keep_outer=True: a peça fechada (a face de fora com o material dela; a de dentro e a borda escuras, viradas para fora do volume).
    keep_outer=False: o vão (só a cópia afundada, virada para FORA do carro, e as paredes da borda dos dois lados), todo escuro.
    """
    import bmesh
    bm = bmesh.new()
    normals = vertex_normals(src_faces)
    outer = {}
    inner = {}
    for v in normals:
        outer[v] = bm.verts.new(v.co)
        inner[v] = bm.verts.new(v.co - normals[v] * depth)
    for f in src_faces:
        if keep_outer:
            nf = bm.faces.new([outer[v] for v in f.verts])
            nf.material_index = f.material_index
            nf.smooth = f.smooth
        else:
            nf = bm.faces.new([inner[v] for v in f.verts])  # virada para fora, vista pela abertura
            nf.material_index = dark
    if keep_outer and depth > 0.0:
        for f in src_faces:
            back = bm.faces.new([inner[v] for v in reversed(f.verts)])  # virada para dentro do carro
            back.material_index = dark
    if keep_outer and depth <= 0.0:
        return bm  # casca já fechada (aerofólio, lentes): só a cópia
    for e in boundary_loops_edges(src_faces):
        a, b = e.verts
        # orientação da parede: a da face da borda, para a peça fechada sair com as normais para fora do volume
        f = next(lf for lf in e.link_faces if lf in set(src_faces))
        loop = next(l for l in f.loops if l.edge == e)
        if loop.vert != a:
            a, b = b, a
        quad = [outer[b], outer[a], inner[a], inner[b]]
        try:
            w = bm.faces.new(quad)
            w.material_index = dark
            if not keep_outer:
                w2 = bm.faces.new(list(reversed(quad)))  # vão: parede dos dois lados (vista de qualquer ângulo)
                w2.material_index = dark
        except ValueError:
            pass  # face repetida (borda dupla): ignora
    return bm


def copy_loop_data(src_faces, dst_bm, src_bm):
    """
    Os dados de canto da face de fora, 1 para 1: todos os canais de UV na mesma ordem (o material lê o canal 0; copiando só o "ativo",
    o vidro saiu com o canal de lightmap) e a normal do arquivo guardada em NORMAL_ATTR (store_corner_normals).
    """
    pairs_uv = [(src, dst_bm.loops.layers.uv.new(name)) for name, src in src_bm.loops.layers.uv.items()]
    src_n = src_bm.loops.layers.float_vector.get(NORMAL_ATTR)
    dst_n = dst_bm.loops.layers.float_vector.new(NORMAL_ATTR) if src_n is not None else None
    # as primeiras faces do destino são as de fora (na mesma ordem das de origem quando keep_outer)
    dst_faces = list(dst_bm.faces)
    for i, f in enumerate(src_faces):
        if i >= len(dst_faces):
            break
        for ls, ld in zip(f.loops, dst_faces[i].loops):
            for src, dst in pairs_uv:
                ld[dst].uv = ls[src].uv
            if dst_n is not None:
                ld[dst_n] = ls[src_n]


def new_object_from_bmesh(name, bm, materials):
    import bpy
    me = bpy.data.meshes.new(name)
    bm.to_mesh(me)
    bm.free()
    for m in materials:
        me.materials.append(m)
    obj = bpy.data.objects.new(name, me)
    bpy.context.scene.collection.objects.link(obj)
    return obj


def store_corner_normals(obj):
    """
    Guarda a normal de cada canto (a do arquivo, depois da subdivisão) num atributo: ele atravessa o bmesh e os cortes (o corte interpola),
    e a peça copia 1 para 1 (copy_loop_data). As normais personalizadas não sobrevivem a montar a peça pelo bmesh; transferir "pela face
    mais próxima" pegava a casca errada onde há duas coladas (o para-brisa saiu com manchas de luz, lote 18b).
    """
    me = obj.data
    attr = me.attributes.new(NORMAL_ATTR, "FLOAT_VECTOR", "CORNER")
    for i, c in enumerate(me.corner_normals):
        attr.data[i].vector = c.vector


def apply_saved_normals(obj):
    """
    As normais guardadas viram as normais personalizadas. Canto sem normal guardada (vão, parede, face de dentro): a média das faces do
    mesmo vértice a menos de SMOOTH_ANGLE_DEG da face (lote 19: com a normal da face o vão saía facetado). As paredes do vão são de
    dois lados (faces opostas no mesmo vértice) e ficam de fora uma da outra pelo ângulo.
    """
    import math
    from mathutils import Vector
    me = obj.data
    attr = me.attributes.get(NORMAL_ATTR)
    if attr is None:
        return
    limit = math.cos(math.radians(SMOOTH_ANGLE_DEG))
    faces_of = {}
    for poly in me.polygons:
        for vi in poly.vertices:
            faces_of.setdefault(vi, []).append(poly)
    normals = []
    for poly in me.polygons:
        for li in range(poly.loop_start, poly.loop_start + poly.loop_total):
            n = attr.data[li].vector
            if n.length > 0.5:
                normals.append(tuple(n))
                continue
            vi = me.loops[li].vertex_index
            acc = Vector((0.0, 0.0, 0.0))
            for other in faces_of[vi]:
                if other.normal.dot(poly.normal) >= limit:
                    acc += other.normal * other.area
            normals.append(tuple(acc.normalized()) if acc.length > 1e-9 else tuple(poly.normal))
    me.normals_split_custom_set(normals)
    me.attributes.remove(me.attributes[NORMAL_ATTR])


def dark_index(body):
    dark = material_index(body, DARK_MATERIAL_HINT)
    if dark is None:
        raise SplitError("Não achei o material escuro ('%s') entre %s" % (DARK_MATERIAL_HINT, [m.name for m in body.data.materials]))
    return dark


def vao_index(body):
    """O slot do vão (VAO_MATERIAL), criado no fim da lista se ainda não existe; o Unreal o liga ao MI_DNVao pelo nome."""
    import bpy
    found = material_index(body, VAO_MATERIAL)
    if found is not None:
        return found
    material = bpy.data.materials.get(VAO_MATERIAL) or bpy.data.materials.new(VAO_MATERIAL)
    body.data.materials.append(material)
    return len(body.data.materials) - 1


def take_parts(bm, body, parts, on_part):
    """Percorre as peças na ordem do mapa tirando as faces de cada uma de bm; on_part(peça, faces) recebe as faces antes de saírem."""
    import bmesh
    for part in parts:
        mat = material_index(body, part["material"]) if part.get("material") else None
        if part.get("material") and mat is None:
            raise SplitError("Peça %s: material '%s' não existe" % (part["name"], part["material"]))
        faces = pick_faces(bm, part, mat)
        if not faces:
            raise SplitError("Peça %s: nenhuma face na caixa %s" % (part["name"], part["box"]))
        on_part(part, faces)
        bmesh.ops.delete(bm, geom=faces, context="FACES_ONLY")
        loose = [v for v in bm.verts if not v.link_faces]
        bmesh.ops.delete(bm, geom=loose, context="VERTS")
        bm.faces.ensure_lookup_table()


def coarse_pockets(body, parts):
    """
    Os vãos, feitos na malha ainda SEM subdivisão: uma cópia da casca de cada peça afundada para dentro, com as paredes da borda. A
    subdivisão simples não muda a forma, então a borda grossa coincide com a fina (sem fresta) e o vão fica com 1/4 dos vértices. Feitos
    na malha subdividida, eles dobravam a carroceria (de 37,8 mil para 71 mil vértices).
    """
    import bmesh
    vao = vao_index(body)
    bm = bmesh.new()
    bm.from_mesh(body.data)
    bm.faces.ensure_lookup_table()
    pockets = []

    def on_part(part, faces):
        depth = float(part.get("pocket", 0.0))
        if depth > 0.0:
            pockets.append(build_shell(faces, depth, vao, keep_outer=False))

    take_parts(bm, body, parts, on_part)
    bm.free()
    return pockets


def split(body, parts, pockets, report):
    """Tira cada peça da carroceria (na ordem do mapa) e põe os vãos (coarse_pockets) no lugar. Devolve {nome: objeto}."""
    import bmesh
    bm = bmesh.new()
    bm.from_mesh(body.data)
    bm.faces.ensure_lookup_table()
    dark = dark_index(body)
    materials = list(body.data.materials)
    out = {}

    def on_part(part, faces):
        thickness = float(part.get("thickness", 0.01))
        piece = build_shell(faces, thickness, dark, keep_outer=True)
        copy_loop_data(faces, piece, bm)
        out[part["name"]] = new_object_from_bmesh(part["name"], piece, materials)
        report.append("peça %s: %d faces, %d vértices na casca, espessura %.1f cm, vão %.0f cm"
                      % (part["name"], len(faces), len({v for f in faces for v in f.verts}), thickness * 100.0, float(part.get("pocket", 0.0)) * 100.0))

    take_parts(bm, body, parts, on_part)
    # os vãos entram na carroceria (sempre lá, escondidos atrás da peça até ela sair)
    for pbm in pockets:
        tmp = bmesh.new()
        me_tmp = __import__("bpy").data.meshes.new("vao_tmp")
        pbm.to_mesh(me_tmp)
        pbm.free()
        tmp.from_mesh(me_tmp)
        __import__("bpy").data.meshes.remove(me_tmp)
        vmap = {}
        for v in tmp.verts:
            vmap[v.index] = bm.verts.new(v.co)
        bm.verts.ensure_lookup_table()
        for f in tmp.faces:
            try:
                nf = bm.faces.new([vmap[v.index] for v in f.verts])
                nf.material_index = f.material_index
            except ValueError:
                pass
        tmp.free()
    bm.to_mesh(body.data)
    bm.free()
    body.data.update()
    report.append("carroceria sem as peças, com %d vão(s): %d vértices" % (len(pockets), len(body.data.vertices)))
    return out


def prepare_and_export(obj, name, box, waves, peaks, out_dir, export, report):
    """Normais do arquivo, alvos, cor do desgaste, osso e FBX. A subdivisão já foi feita no carro inteiro, antes dos cortes."""
    import bpy
    apply_saved_normals(obj)
    mdm.mark_sharp_edges(obj)
    mdm.add_shape_keys(obj, box, waves, [])
    mdm.wear_colors(obj, box, waves, [], peaks=peaks)
    if not export:
        return
    arm = mdm.add_root_bone(obj)
    path = os.path.join(out_dir, "%s.fbx" % name)
    mdm.export(obj, arm, path)
    # o próximo precisa do nome "Armature" livre (com outro nome o Unreal cria um osso a mais)
    obj.parent = None
    for m in list(obj.modifiers):
        if m.type == "ARMATURE":
            obj.modifiers.remove(m)
    bpy.data.objects.remove(arm)
    report.append("exportado: %s (%d vértices)" % (path, len(obj.data.vertices)))


def render_parts(objects, body, box, out_dir, report):
    """Conferência: o carro remontado com cada peça numa cor, intacto e com as 5 zonas em 1,0, de quatro lados; e o carro sem as peças."""
    import bpy
    from mathutils import Vector
    os.makedirs(out_dir, exist_ok=True)
    scene = bpy.context.scene
    scene.render.engine = "BLENDER_WORKBENCH"
    scene.render.resolution_x, scene.render.resolution_y = 1280, 720
    shading = scene.display.shading
    shading.light = "STUDIO"
    shading.color_type = "OBJECT"
    shading.show_cavity = True
    shading.cavity_type = "BOTH"
    shading.show_shadows = True
    shading.show_backface_culling = True
    scene.display.render_aa = "16"
    palette = [(0.85, 0.25, 0.2), (0.2, 0.55, 0.9), (0.95, 0.75, 0.1), (0.3, 0.8, 0.35), (0.7, 0.35, 0.85), (0.95, 0.5, 0.15),
               (0.2, 0.8, 0.8), (0.9, 0.4, 0.6), (0.55, 0.45, 0.3), (0.5, 0.75, 0.2), (0.35, 0.35, 0.9), (0.85, 0.85, 0.85)]
    body.color = (0.6, 0.62, 0.66, 1.0)
    for i, obj in enumerate(objects):
        c = palette[i % len(palette)]
        obj.color = (c[0], c[1], c[2], 1.0)
    center = Vector(((box.xmin + box.xmax) / 2.0, 0.0, (box.zmin + box.zmax) / 2.0))
    target = bpy.data.objects.new("Alvo", None)
    target.location = center
    scene.collection.objects.link(target)
    cam = bpy.data.objects.new("Camera", bpy.data.cameras.new("Camera"))
    cam.data.lens = 50.0
    scene.collection.objects.link(cam)
    track = cam.constraints.new("TRACK_TO")
    track.target = target
    track.track_axis, track.up_axis = "TRACK_NEGATIVE_Z", "UP_Y"
    scene.camera = cam
    everything = [body] + list(objects)

    def set_keys(value):
        for o in everything:
            if o.data.shape_keys:
                for name in mdm.ZONES:
                    o.data.shape_keys.key_blocks[name].value = value

    views = {"frente-esq": (6.0, 4.0, 2.2), "tras-dir": (-6.0, -4.0, 2.2), "lado-esq": (0.6, 7.5, 1.6), "cima": (0.2, 0.01, 8.5)}
    for state, value in (("0", 0.0), ("1", 1.0)):
        set_keys(value)
        for label, offset in views.items():
            cam.location = center + Vector(offset)
            scene.render.filepath = os.path.join(os.path.abspath(out_dir), "remontado-%s-%s.png" % (state, label))
            bpy.ops.render.render(write_still=True)
            report.append("imagem: %s" % scene.render.filepath)
    set_keys(0.0)
    # sem as peças: os vãos escuros no lugar (nenhum buraco transparente)
    for o in objects:
        o.hide_render = True
    for label in ("frente-esq", "lado-esq", "tras-dir", "cima"):
        cam.location = center + Vector(views[label])
        scene.render.filepath = os.path.join(os.path.abspath(out_dir), "sem-pecas-%s.png" % label)
        bpy.ops.render.render(write_still=True)
        report.append("imagem: %s" % scene.render.filepath)
    for o in objects:
        o.hide_render = False


def main(argv):
    report = []
    error = None
    try:
        args = parse_args(mdm.script_args(argv))
        car, parts = read_map(args.map)
        body = mdm.load_and_join(args.inputs, report)
        body.name = body.data.name = "SK_DN%sBody" % car
        box = mdm.car_box(body)
        global CORE
        CORE = Core(box)
        wheels = mdm.read_wheels(args.wheels)
        box.set_wheels(wheels["front_axle_cm"] / 100.0, wheels["rear_axle_cm"] / 100.0, wheels["wheel_radius_cm"] / 100.0)
        waves = mdm.ripples(args.seed)
        peaks = mdm.zone_peaks([v.co.copy() for v in body.data.vertices], box, waves)  # o 1,0 do desgaste: o carro inteiro
        report.append("carro %s: %.2f m x %.2f m x %.2f m" % (car, box.length, box.width, box.height))
        # subdivide ANTES de cortar: as peças e a carroceria ficam com a mesma densidade do lote 17b, e os vãos e a espessura (cópias)
        # não se multiplicam por quatro (subdividindo depois, a carroceria saía com 82 mil vértices)
        pockets = coarse_pockets(body, parts)
        mdm.subdivide(body, args.subdivide)
        report.append("subdivisão simples %d: %d vértices no carro inteiro" % (args.subdivide, len(body.data.vertices)))
        store_corner_normals(body)  # as normais do arquivo, por canto, antes de cortar (copy_loop_data, apply_saved_normals)
        pieces = split(body, parts, pockets, report)
        out_dir = os.path.abspath(args.out_dir)
        if not args.no_export:
            os.makedirs(out_dir, exist_ok=True)
        prepare_and_export(body, body.name, box, waves, peaks, out_dir, not args.no_export, report)
        for name, obj in pieces.items():
            prepare_and_export(obj, "SK_DN%s_%s" % (car, name), box, waves, peaks, out_dir, not args.no_export, report)
        total = len(body.data.vertices) + sum(len(o.data.vertices) for o in pieces.values())
        report.append("total: %d vértices (carroceria %d, peças e vidros %d)" % (total, len(body.data.vertices), total - len(body.data.vertices)))
        if args.renders:
            render_parts(list(pieces.values()), body, box, args.renders, report)
    except (SplitError, mdm.MorphError) as exc:
        error = str(exc)
    except Exception as exc:  # noqa: BLE001
        import traceback
        error = "Erro inesperado: %r\n%s" % (exc, traceback.format_exc())
    if error:
        report.append("ERRO: " + error)
    print("\n".join("[split_car_parts] " + line for line in report))
    return 1 if error else 0


if __name__ == "__main__":
    code = main(sys.argv)
    if code:
        sys.exit(code)
