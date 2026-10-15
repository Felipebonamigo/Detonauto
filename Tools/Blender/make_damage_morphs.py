"""
Gera os 5 alvos de deformação do dano por zona (lote 17, contrato em docs/regras-dano-visual.md) e exporta
uma malha com esqueleto de 1 osso para o Unreal.

Entrada: um ou mais .fbx do carro (a carroceria e as peças que precisam acompanhar a lataria, como os faróis),
exportados do Unreal. Saída: um .fbx com a malha unida, o osso "root" e os alvos DN_Front, DN_Rear, DN_Left,
DN_Right e DN_Roof (os mesmos nomes de FDNDamageMorph::TargetName), mais uma imagem de conferência por zona.

A deformação é um CAMPO ESPACIAL SUAVE: uma função da posição no espaço do carro, igual para todas as cascas da
malha. Vértices que coincidem (borda do capô e do para-lama, por exemplo) andam juntos, então não abrem frestas.
Sobre a forma principal de cada zona vai uma ondulação de baixa frequência (2 a 4 dobras), com semente fixa.

Eixos (conferidos em 14 out 2026 com o SM_SportsCar): o FBX do Unreal abre no Blender com +X do Unreal em +X
(a frente do carro) e a esquerda do carro em +Y; na volta, +X do Blender chega como +X do Unreal (ver
export_fbx_batch.py). EDNDamageZone::Left é o lado -Y do Unreal, que é o +Y daqui.

Uso (modo background, sem janela; Blender 5.2):

    blender -b --python-exit-code 1 -P Tools/Blender/make_damage_morphs.py -- ^
        --inputs Sandbox/dano/SM_SportsCar.fbx Sandbox/dano/SM_SportsCar_Glass.fbx ^
        --out Sandbox/dano/SK_DNSportsCarBody.fbx --wheels Sandbox/dano/wheels.json --renders docs/img/dano-zonas

Argumentos
    --inputs        um ou mais .fbx, unidos numa malha só (cada material vira um slot)
    --out           o .fbx de saída (dentro de Sandbox/)
    --wheels        wheels.json com os eixos e o raio da roda em cm (Tools/Unreal/damage_body.py --mode export grava)
    --renders       pasta das imagens de conferência (dentro de docs/img/); sem ela, não renderiza
    --seed          semente da ondulação (padrão 17)
    --subdivide     níveis de subdivisão simples antes dos alvos (padrão 1; 0 desliga)
    --allowed-root  pasta onde é permitido gravar o .fbx. Padrão: Sandbox/ deste repositório.
"""

import math
import os
import random
import sys

# Esta parte não depende do Blender.

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
REPO_ROOT = os.path.abspath(os.path.join(SCRIPT_DIR, "..", ".."))
DEFAULT_ALLOWED_ROOT = os.path.join(REPO_ROOT, "Sandbox")
RENDERS_ROOT = os.path.join(REPO_ROOT, "docs", "img")

# Na ordem de EDNDamageZone (sem a Under, que não tem alvo).
ZONES = ("DN_Front", "DN_Rear", "DN_Left", "DN_Right", "DN_Roof")

# Valores em fração do tamanho do carro. Frente e traseira mudaram do contrato (recuo de 18% e 14% do comprimento caindo a
# zero a 35%): com eles a lataria recuava para trás das rodas, que ficavam soltas na frente (captura do lote 17). Agora o
# amassado forte fica no balanço, da ponta até o para-lama, e só uma cauda leve passa da roda.
FRONT_CRUSH = 0.75      # a ponta da frente recua esta fração da distância da ponta até o começo do para-lama (17b: 0,45, depois
                        # 0,55; no fechamento do 17b a batida real a 100 km/h ainda saía tímida). Com a queda suave o recuo não
                        # passa de ~0,66 sem dobrar (determinante 1 - 1,5 x recuo); a frente usa a rampa crush_ramp, de inclinação 1.
                        # Mais que isso o nariz passaria das rodas (o balanço é só 12% do comprimento)
REAR_CRUSH = 0.50       # (17b: era 0,40; a revisão pediu mais amplitude em 1,0, até onde a conferência de dobra deixa)
ARCH_MARGIN = 0.02      # o amassado forte acaba um pouco depois do começo do para-lama (fração do comprimento)
TAIL_DEPTH = 0.025      # cauda leve: recuo extra na ponta, fração do comprimento...
TAIL_REACH = 0.35       # ...que cai a zero a esta fração do comprimento, a partir da ponta (o alcance do contrato)
SIDE_DEPTH = 0.12       # afundamento da porta, fração da largura
ROOF_DEPTH = 0.20       # afundamento do teto, fração da altura da cabine
BELT_LINE = 0.55        # altura da linha da cintura (base da cabine), fração da altura
RIPPLE = 0.10           # ondulação larga: tira até 10% da forma principal (17b: era 25%; os vincos abaixo fazem o relevo)
# Vincos (17b): dobras com crista viva, perfil 1 - |sen| (ponta aguda, vale redondo), como chapa que dobra em vez de afundar macio
CREASES = 3.0           # quantas cristas cabem na região amassada de cada zona
CREASE_HOOD = 0.05      # frente e traseira: altura das cristas no capô e no porta-malas, fração da altura do carro...
CREASE_FENDER = 0.035   # ...e nos para-lamas, fração da largura (0,035 e 0,025 quase não apareciam no capô)
CREASE_SIDE = 0.30      # portas e teto: amplitude das cristas, fração do afundamento principal da zona
HOOD_RISE = 0.14        # o capô sobe em "V" (em tenda) até esta fração da altura (fechamento do 17b: era 0,08)
REAR_RISE = 0.048       # o porta-malas sobe até esta fração (60% do 0,08 de antes: a traseira não mudou no fechamento)
FRONT_FOLD_GAIN = 1.6   # a frente dobra 1,6 vez mais que a traseira (fechamento do 17b: a frente em 1,0 saía tímida)
CORNER_BIAS = 0.25      # frente e traseira: a quina menos atingida recua até 25% menos que a outra
PILLAR_LEAN = 0.04      # as colunas inclinam: o topo da cabine anda de lado esta fração da largura


class MorphError(RuntimeError):
    """Erro esperado, com mensagem para o usuário."""


def smoothstep(t):
    t = min(max(t, 0.0), 1.0)
    return t * t * (3.0 - 2.0 * t)


def falloff(t):
    """1 até t = 0, 0 a partir de t = 1, com derivada contínua no meio."""
    return 1.0 - smoothstep(t)


def crush_ramp(t):
    """Recuo da frente ao longo do balanço (t = distância da ponta / alcance): 1 na ponta, cai em linha reta até t = 0,8 e
    encosta em 0 em t = 1,2 sem quina. Inclinação de no máximo 1: com recuo C o determinante fica em 1 - C, longe de dobrar."""
    t = max(t, 0.0)
    if t <= 0.8:
        return 1.0 - t
    if t >= 1.2:
        return 0.0
    u = t - 0.8
    return 0.2 - u + 1.25 * u * u


def bump(t):
    """1 em t = 0, 0 em |t| >= 1."""
    return falloff(abs(t))


class Box:
    """Limites do carro (metros, eixos do Blender: frente +X, esquerda +Y, cima +Z)."""

    def __init__(self, xmin, xmax, half_width, zmin, zmax):
        self.xmin, self.xmax, self.half_width, self.zmin, self.zmax = xmin, xmax, half_width, zmin, zmax
        self.length = xmax - xmin
        self.width = 2.0 * half_width
        self.height = zmax - zmin
        self.front_arch = 0.15  # distância da ponta até o para-lama, em fração do comprimento (set_wheels mede)
        self.rear_arch = 0.15

    def set_wheels(self, front_axle, rear_axle, radius):
        """Eixos (X) e raio da roda em metros: onde começa cada para-lama, contado da ponta do carro."""
        self.front_arch = max(0.05, (self.xmax - (front_axle + radius)) / self.length)
        self.rear_arch = max(0.05, ((rear_axle - radius) - self.xmin) / self.length)

    def normalized(self, x, y, z):
        """fx: 0 na traseira, 1 na ponta da frente; sy: -1 a 1, + à esquerda; hz: 0 no chão do carro, 1 no teto."""
        return ((x - self.xmin) / self.length, y / self.half_width, (z - self.zmin) / self.height)


class Ripple:
    """Ondulação de baixa frequência num plano (u, v em 0..1): 1 a 2 ondas inteiras em cada eixo, fase sorteada."""

    def __init__(self, rng):
        self.ku, self.kv = rng.uniform(1.0, 2.0), rng.uniform(1.0, 2.0)
        self.pu, self.pv = rng.uniform(0.0, 1.0), rng.uniform(0.0, 1.0)

    def __call__(self, u, v):
        return math.sin(2.0 * math.pi * (self.ku * u + self.pu)) * math.sin(2.0 * math.pi * (self.kv * v + self.pv))


def wave_factor(r):
    """Ondulação r (-1 a 1) virando fator da forma principal: de 1 - RIPPLE a 1, nunca acima da profundidade."""
    return 1.0 - RIPPLE * (0.5 + 0.5 * r)


def ripples(seed):
    """Uma ondulação por zona, sempre a mesma para a mesma semente."""
    rng = random.Random(seed)
    return [Ripple(rng) for _ in ZONES]


def crest(u):
    """Perfil de vinco de -0,5 a 0,5: ponta aguda nos inteiros (onde |sen| tem a quina), vale redondo no meio."""
    return 0.5 - abs(math.sin(math.pi * u))


def crease_phase(zone, box, ripple, fx, sy, hz):
    """Onde o ponto cai no padrão de vincos da zona (as cristas ficam nos inteiros). Usado pela forma e pela cor de vértice."""
    if zone in (0, 1):
        front = zone == 0
        reach = (box.front_arch if front else box.rear_arch) + ARCH_MARGIN
        d = (1.0 - fx) if front else fx
        return CREASES * d / reach + 0.35 * sy + ripple.pu   # sanfona na região amassada, um pouco torta
    if zone in (2, 3):
        return CREASES * 1.6 * hz + 0.6 * fx + ripple.pv      # linhas quase horizontais na porta, inclinadas para a frente
    return CREASES * (fx - 0.09) / 0.66 + 0.3 * sy + ripple.pu  # de lado a lado no teto, um pouco em diagonal


def displacement(zone, box, ripple, x, y, z, lean_sign=1.0):
    """Deslocamento (dx, dy, dz) em metros do ponto (x, y, z) com a zona totalmente amassada (peso 1)."""
    fx, sy, hz = box.normalized(x, y, z)
    phase = crease_phase(zone, box, ripple, fx, sy, hz)
    top = smoothstep((hz - 0.40) / 0.25)  # só a parte de cima (capô, porta-malas) dobra para cima
    if zone in (0, 1):
        front = zone == 0
        arch = box.front_arch if front else box.rear_arch
        reach = arch + ARCH_MARGIN
        d = (1.0 - fx) if front else fx       # distância da ponta atingida, em fração do comprimento
        g = falloff(d / reach)
        shape = crush_ramp(d / reach) if front else g   # a frente recua em rampa (mais fundo sem dobrar); a traseira, suave
        depth = (FRONT_CRUSH if front else REAR_CRUSH) * reach * shape + TAIL_DEPTH * falloff(d / TAIL_REACH)
        wave = wave_factor(ripple(0.5 + 0.5 * sy, hz))
        # batida fora do centro: uma quina recua inteira e a outra um pouco menos (a frente pega a esquerda)
        corner = 1.0 - CORNER_BIAS * (0.5 - 0.5 * sy * (1.0 if front else -1.0))
        dx = (-1.0 if front else 1.0) * depth * box.length * wave * corner
        # dobra em "V" no capô (ou no porta-malas), no meio do balanço: a crista fica mais perto da ponta no meio do carro
        ridge = arch * (0.45 + 0.30 * abs(sy))
        rise = (HOOD_RISE if front else REAR_RISE) * box.height
        dz = rise * bump((d - ridge) / (0.35 * arch)) * (1.0 - min(sy * sy, 1.0)) * top * corner
        dy = 0.02 * box.width * sy * g      # a lataria esmagada abre um pouco para os lados
        # vincos em sanfona na região amassada, um pouco tortos (a crista anda com o lado do carro)
        fold = crest(phase) * g * corner * (FRONT_FOLD_GAIN if front else 1.0)
        dz += CREASE_HOOD * box.height * fold * top
        flank = smoothstep((abs(sy) - 0.5) / 0.4) * (1.0 - top)
        dy += CREASE_FENDER * box.width * fold * flank * (1.0 if sy >= 0.0 else -1.0)
        return dx, dy, dz
    if zone in (2, 3):
        side = 1.0 if zone == 2 else -1.0     # esquerda é +Y
        gs = smoothstep((side * sy - 0.2) / 0.8)
        gx = bump((fx - 0.48) / 0.42)         # cai a zero nas pontas do carro
        gz = bump((hz - 0.42) / 0.38)         # e perto do assoalho e do teto
        wave = wave_factor(ripple(fx, hz))
        main = SIDE_DEPTH * box.width * gs * gx * gz
        fold = crest(phase)  # vincos na porta (a chapa dobra em linhas)
        return 0.0, -side * main * (wave + CREASE_SIDE * 2.0 * fold), 0.0
    if zone == 4:
        cabin = box.height * (1.0 - BELT_LINE)
        gh = smoothstep((hz - BELT_LINE) / (1.0 - BELT_LINE))
        gx = bump((fx - 0.42) / 0.33)
        gy = 1.0 - 0.5 * min(sy * sy, 1.0)
        wave = wave_factor(ripple(fx, 0.5 + 0.5 * sy))
        main = ROOF_DEPTH * cabin * gh * gx * gy
        fold = crest(phase)  # vincos atravessando o teto
        dz = -main * (wave + CREASE_SIDE * 2.0 * fold)
        dy = lean_sign * PILLAR_LEAN * box.width * gh * gx
        return 0.0, dy, dz
    raise MorphError("zona inválida: %r" % (zone,))


def fold_check(zone, box, ripple, steps=14, h=0.01):
    """
    Menor determinante do jacobiano de p -> p + d(p) numa grade dentro da caixa. Abaixo de 0 a malha dobra sobre
    si mesma (faces de dentro aparecem); o contrato quer longe disso.
    """
    worst = float("inf")
    for i in range(steps + 1):
        for j in range(steps + 1):
            for k in range(steps + 1):
                x = box.xmin + box.length * i / steps
                y = -box.half_width + box.width * j / steps
                z = box.zmin + box.height * k / steps
                cols = []
                for axis in range(3):
                    a = [x, y, z]; b = [x, y, z]
                    a[axis] -= h; b[axis] += h
                    da = displacement(zone, box, ripple, *a)
                    db = displacement(zone, box, ripple, *b)
                    cols.append([(b[n] - a[n] + db[n] - da[n]) / (2.0 * h) for n in range(3)])
                m = cols  # colunas
                det = (m[0][0] * (m[1][1] * m[2][2] - m[2][1] * m[1][2])
                       - m[1][0] * (m[0][1] * m[2][2] - m[2][1] * m[0][2])
                       + m[2][0] * (m[0][1] * m[1][2] - m[1][1] * m[0][2]))
                worst = min(worst, det)
    return worst


def read_wheels(path):
    """wheels.json gravado por Tools/Unreal/damage_body.py --mode export (centímetros, espaço do carro no Unreal)."""
    import json
    try:
        with open(path, encoding="utf-8") as handle:
            data = json.load(handle)
        values = {k: float(data[k]) for k in ("front_axle_cm", "rear_axle_cm", "wheel_radius_cm")}
    except (OSError, ValueError, KeyError) as exc:
        raise MorphError("Não consegui ler as rodas em %s: %r" % (path, exc))
    if not (values["front_axle_cm"] > values["rear_axle_cm"] and values["wheel_radius_cm"] > 0.0):
        raise MorphError("Rodas estranhas em %s: %s" % (path, values))
    return values


def is_inside(path, root):
    path = os.path.normcase(os.path.abspath(path))
    root = os.path.normcase(os.path.abspath(root))
    return path == root or path.startswith(root + os.sep)


def script_args(argv):
    return argv[argv.index("--") + 1:] if "--" in argv else []


def parse_args(args):
    import argparse
    parser = argparse.ArgumentParser(prog="make_damage_morphs.py", add_help=False)
    parser.add_argument("--inputs", nargs="+", required=True)
    parser.add_argument("--out", required=True)
    parser.add_argument("--wheels", required=True)
    parser.add_argument("--renders", default=None)
    parser.add_argument("--seed", type=int, default=17)
    parser.add_argument("--subdivide", type=int, default=1)
    parser.add_argument("--allowed-root", default=DEFAULT_ALLOWED_ROOT)
    try:
        parsed = parser.parse_args(args)
    except SystemExit:
        raise MorphError("Argumentos inválidos. Uso: --inputs <a.fbx> [b.fbx ...] --out <saida.fbx> --wheels <wheels.json> "
                         "[--renders <pasta>] [--seed 17]")
    if not is_inside(parsed.out, parsed.allowed_root):
        raise MorphError("Saída recusada: '%s' fica fora de '%s'." % (os.path.abspath(parsed.out), parsed.allowed_root))
    if parsed.renders and not is_inside(parsed.renders, RENDERS_ROOT):
        raise MorphError("Pasta de imagens recusada: '%s' fica fora de '%s'." % (os.path.abspath(parsed.renders), RENDERS_ROOT))
    return parsed


# Esta parte usa o Blender (bpy). Rodada sem erro no Blender 5.2.2.

def load_and_join(paths, report):
    import bpy
    bpy.ops.wm.read_factory_settings(use_empty=True)
    for path in paths:
        if not os.path.isfile(path):
            raise MorphError("Arquivo não encontrado: %s" % path)
        bpy.ops.import_scene.fbx(filepath=os.path.abspath(path))
    meshes = [o for o in bpy.data.objects if o.type == "MESH"]
    if not meshes:
        raise MorphError("Nenhuma malha nos arquivos de entrada.")
    for o in bpy.data.objects:
        if o.type != "MESH":
            bpy.data.objects.remove(o)
    bpy.ops.object.select_all(action="DESELECT")
    for o in meshes:
        o.select_set(True)
        report.append("entrada: %s (%d vértices, materiais %s)" % (o.name, len(o.data.vertices), [m.name for m in o.data.materials]))
    bpy.context.view_layer.objects.active = meshes[0]
    # o FBX do Unreal chega em centímetros com escala 0,01 no objeto: aplica para trabalhar em metros
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
    if len(meshes) > 1:
        bpy.ops.object.join()
    body = bpy.context.view_layer.objects.active
    body.name = "SK_DNSportsCarBody"
    body.data.name = "SK_DNSportsCarBody"
    return body


def subdivide(body, levels):
    """
    Subdivisão SIMPLES (a forma não muda): mais vértices onde os vincos dobram. O carro do template tem 6,5 mil vértices na lataria e
    os vincos do 17b saíam serrilhados. As normais do arquivo (customizadas) e os UVs são interpolados, então o carro intacto fica igual.
    """
    import bpy
    if levels <= 0:
        return
    bpy.context.view_layer.objects.active = body
    modifier = body.modifiers.new("Subdivisao", "SUBSURF")
    modifier.subdivision_type = "SIMPLE"
    modifier.levels = levels
    modifier.render_levels = levels
    if hasattr(modifier, "use_custom_normals"):
        modifier.use_custom_normals = True
    bpy.ops.object.modifier_apply(modifier=modifier.name)


def mark_sharp_edges(body, degrees=40.0):
    """Arestas vivas acima de ~40 graus ficam marcadas: o Unreal recalcula as normais dos alvos respeitando-as."""
    import bmesh
    bm = bmesh.new()
    bm.from_mesh(body.data)
    limit = math.radians(degrees)
    marked = 0
    for edge in bm.edges:
        if len(edge.link_faces) == 2 and edge.calc_face_angle(0.0) > limit:
            edge.smooth = False
            marked += 1
    bm.to_mesh(body.data)
    bm.free()
    return marked


def car_box(body):
    xs = [v.co.x for v in body.data.vertices]
    ys = [v.co.y for v in body.data.vertices]
    zs = [v.co.z for v in body.data.vertices]
    return Box(min(xs), max(xs), max(abs(min(ys)), abs(max(ys))), min(zs), max(zs))


def add_shape_keys(body, box, waves, report):
    body.shape_key_add(name="Basis", from_mix=False)
    for zone, name in enumerate(ZONES):
        key = body.shape_key_add(name=name, from_mix=False)
        biggest = 0.0
        for i, v in enumerate(body.data.vertices):
            d = displacement(zone, box, waves[zone], v.co.x, v.co.y, v.co.z)
            key.data[i].co = (v.co.x + d[0], v.co.y + d[1], v.co.z + d[2])
            biggest = max(biggest, math.sqrt(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]))
        worst = fold_check(zone, box, waves[zone])
        report.append("%s: maior deslocamento %.1f cm | menor determinante do jacobiano %.2f (abaixo de 0 dobraria)"
                      % (name, biggest * 100.0, worst))
        if worst <= 0.0:
            raise MorphError("%s dobra a malha sobre si mesma (determinante %.2f)" % (name, worst))


def wear_colors(body, box, waves, report):
    """
    Cor de vértice para o desgaste da pintura (lote 17b): quanto cada zona amassa naquele ponto, puxado para as cristas dos vincos,
    onde a tinta solta primeiro. R = frente, G = traseira, B = laterais (o material escolhe o lado pelo sinal de Y), A = teto. O
    material multiplica pelo peso da zona: as lascas e os riscos aparecem onde a chapa dobrou, e não espalhados pela zona inteira.
    """
    verts = body.data.vertices
    mags = []
    for zone in range(len(ZONES)):
        row = []
        for v in verts:
            d = displacement(zone, box, waves[zone], v.co.x, v.co.y, v.co.z)
            row.append(math.sqrt(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]))
        mags.append(row)
    attr = body.data.color_attributes.new("DN_Desgaste", "FLOAT_COLOR", "POINT")
    peaks = [max(max(row), 1e-6) for row in mags]
    covered = 0
    for i, v in enumerate(verts):
        fx, sy, hz = box.normalized(v.co.x, v.co.y, v.co.z)
        w = []
        for zone in range(len(ZONES)):
            ridge = crest(crease_phase(zone, box, waves[zone], fx, sy, hz)) + 0.5   # 1 na crista, 0 no vale
            w.append((mags[zone][i] / peaks[zone]) ** 0.7 * (0.3 + 0.7 * ridge ** 3))
        attr.data[i].color = (w[0], w[1], max(w[2], w[3]), w[4])
        covered += 1 if max(w) > 0.5 else 0
    body.data.color_attributes.active_color = attr
    report.append("cor de vértice do desgaste: %d de %d vértices com peso acima de 0,5" % (covered, len(verts)))


def add_root_bone(body):
    """Esqueleto de 1 osso: o Unreal só aceita alvos de deformação em malha com esqueleto."""
    import bpy
    arm_data = bpy.data.armatures.new("Armature")
    arm = bpy.data.objects.new("Armature", arm_data)  # "Armature" o Unreal não vira um osso a mais
    bpy.context.scene.collection.objects.link(arm)
    bpy.context.view_layer.objects.active = arm
    bpy.ops.object.mode_set(mode="EDIT")
    bone = arm_data.edit_bones.new("root")
    bone.head = (0.0, 0.0, 0.0)
    bone.tail = (0.0, 0.0, 0.5)
    bpy.ops.object.mode_set(mode="OBJECT")
    group = body.vertex_groups.new(name="root")
    group.add(list(range(len(body.data.vertices))), 1.0, "REPLACE")
    body.parent = arm
    modifier = body.modifiers.new("Armature", "ARMATURE")
    modifier.object = arm
    return arm


def export(body, arm, path):
    import bpy
    bpy.ops.object.select_all(action="DESELECT")
    body.select_set(True)
    arm.select_set(True)
    bpy.context.view_layer.objects.active = arm
    os.makedirs(os.path.dirname(os.path.abspath(path)), exist_ok=True)
    bpy.ops.export_scene.fbx(
        filepath=os.path.abspath(path),
        use_selection=True,
        object_types={"ARMATURE", "MESH"},
        axis_forward="-Z", axis_up="Y",           # mesmo par conferido no export_fbx_batch.py
        apply_unit_scale=True, global_scale=1.0, apply_scale_options="FBX_SCALE_NONE",
        bake_space_transform=False,              # com esqueleto, o Unreal converte os eixos na importação
        use_mesh_modifiers=False,                # aplicar modificadores apagaria os alvos (shape keys)
        mesh_smooth_type="FACE",
        colors_type="LINEAR",                    # a cor de vértice é dado (pesos do desgaste), não cor: sem conversão sRGB
        add_leaf_bones=False,
        bake_anim=False,
        use_armature_deform_only=True,
        path_mode="AUTO",
    )


def render_zones(body, box, out_dir, report):
    """Uma imagem por zona com o alvo em 1,0, mais o carro intacto (Workbench: mostra a forma, não a pintura)."""
    import bpy
    from mathutils import Vector
    os.makedirs(out_dir, exist_ok=True)
    scene = bpy.context.scene
    scene.render.engine = "BLENDER_WORKBENCH"
    scene.render.resolution_x, scene.render.resolution_y = 1280, 720
    shading = scene.display.shading
    shading.light = "STUDIO"
    shading.color_type = "SINGLE"
    shading.single_color = (0.62, 0.64, 0.68)
    shading.show_cavity = True
    shading.cavity_type = "BOTH"
    shading.show_shadows = True
    scene.display.render_aa = "16"
    center = Vector(((box.xmin + box.xmax) / 2.0, 0.0, (box.zmin + box.zmax) / 2.0))
    target = bpy.data.objects.new("Alvo", None)
    target.location = center
    scene.collection.objects.link(target)
    cam_data = bpy.data.cameras.new("Camera")
    cam_data.lens = 50.0
    cam = bpy.data.objects.new("Camera", cam_data)
    scene.collection.objects.link(cam)
    track = cam.constraints.new("TRACK_TO")
    track.target = target
    track.track_axis, track.up_axis = "TRACK_NEGATIVE_Z", "UP_Y"
    scene.camera = cam
    views = {
        "intacto": (None, (6.5, 4.2, 2.0)),
        "DN_Front": (0, (6.5, 4.2, 2.0)),
        "DN_Rear": (1, (-6.5, -4.2, 2.0)),
        "DN_Left": (2, (1.0, 8.0, 1.8)),
        "DN_Right": (3, (1.0, -8.0, 1.8)),
        "DN_Roof": (4, (-1.0, 7.5, 1.6)),
    }
    keys = body.data.shape_keys.key_blocks
    for label, (zone, offset) in views.items():
        for name in ZONES:
            keys[name].value = 0.0
        if zone is not None:
            keys[ZONES[zone]].value = 1.0
        cam.location = center + Vector(offset)
        scene.render.filepath = os.path.join(os.path.abspath(out_dir), "%s.png" % label)
        bpy.ops.render.render(write_still=True)
        report.append("imagem: %s" % scene.render.filepath)
    for name in ZONES:
        keys[name].value = 0.0


def main(argv):
    report = []
    error = None
    try:
        args = parse_args(script_args(argv))
        body = load_and_join(args.inputs, report)
        box = car_box(body)
        report.append("carro: %.2f m de comprimento, %.2f de largura, %.2f de altura (x %.2f a %.2f)"
                      % (box.length, box.width, box.height, box.xmin, box.xmax))
        wheels = read_wheels(args.wheels)
        box.set_wheels(wheels["front_axle_cm"] / 100.0, wheels["rear_axle_cm"] / 100.0, wheels["wheel_radius_cm"] / 100.0)
        report.append("para-lamas a %.1f%% (frente) e %.1f%% (traseira) do comprimento, contados da ponta"
                      % (box.front_arch * 100.0, box.rear_arch * 100.0))
        subdivide(body, args.subdivide)
        report.append("subdivisão simples %d: %d vértices" % (args.subdivide, len(body.data.vertices)))
        report.append("arestas vivas marcadas (> 40 graus): %d" % mark_sharp_edges(body))
        waves = ripples(args.seed)
        add_shape_keys(body, box, waves, report)
        wear_colors(body, box, waves, report)
        arm = add_root_bone(body)
        export(body, arm, args.out)
        report.append("saída: %s" % os.path.abspath(args.out))
        if args.renders:
            render_zones(body, box, args.renders, report)
    except MorphError as exc:
        error = str(exc)
    except Exception as exc:  # noqa: BLE001
        error = "Erro inesperado: %r" % (exc,)
    if error:
        report.append("ERRO: " + error)
    print("\n".join("[make_damage_morphs] " + line for line in report))
    return 1 if error else 0


if __name__ == "__main__":
    code = main(sys.argv)
    if code:
        sys.exit(code)
