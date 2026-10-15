"""
Cria o material-pai do protótipo de amassado (M_DNDentParent) por script, sem abrir o editor.

Rodar SEM janela, pelo commandlet de Python do Unreal (testado no UE 5.8):

    UnrealEditor-Cmd.exe <projeto>.uproject -run=PythonScript ^
        -Script="<caminho>/create_dent_material.py --dest /Game/Sandbox_Teste" ^
        -unattended -nullrhi -nosplash -nosound

Existe um invólucro em PowerShell: Tools/Unreal/create_dent_material.ps1.

O QUE O MATERIAL FAZ (protótipo, lote 10; não é código final)
    Desloca vértices (World Position Offset) para dentro, ao longo da normal do vértice, perto de até 8 amassados.
    Cada amassado é descrito por TRÊS parâmetros, no espaço local do componente (cm):
        D<i>P = (X, Y, Z)             posição (vetor)
        D<i>R = raio                  escalar; raio <= 0 desliga o amassado
        D<i>D = profundidade          escalar
    (O raio não vai no canal alfa do vetor: o nó Custom recebe só RGB de um parâmetro vetorial.)
    O deslocamento de um vértice é  -normal x profundidade x f,  com  f = suavização de (1 - distância / raio).
    Também escurece um pouco a cor dentro do amassado (marca de batida). A normal NÃO é recalculada: a iluminação do
    amassado usa a normal original da malha (risco anotado em docs/prototipo-amassado.md).

    PINTURA (lote 16): o mesmo material é a pintura automotiva da carroceria. Modelo de sombreamento Clear Coat: verniz liso por cima
    (ClearCoat, ClearCoatRoughness) e base metálica com flocos por baixo (normal de flocos T_DNFlocos_N, gerada por este script, em
    FlakeTiling x UV, força FlakeStrength). Cor = Base Texture x Paint Tint. Os parâmetros Base Texture, MRA (oclusão no B), Normal,
    Paint Tint e MetalPaintMetallic têm os nomes do material do carro do template: o ADNVehicle copia os valores originais de cada
    slot (chassi e janelas continuam com as texturas deles) e só a pintura fica com o tom deste material (vermelho escuro metálico).
    Roughness é a aspereza da base. Tudo sai pelos atributos de material, o único caminho do Python 5.8 até o verniz.
    Rodar de novo refaz o material no mesmo pacote (sem apagar).

LIMITE DE SEGURANÇA: só grava pacotes em /Game/ do projeto que o abriu (qualquer outro destino é recusado antes de
qualquer escrita) e o relatório só pode ficar dentro da pasta do projeto.
"""

import sys

# O Python embutido da engine gravaria .pyc dentro da instalação do Unreal. Desliga ANTES de qualquer outro import.
sys.dont_write_bytecode = True

import argparse
import math
import os
import random
import re
import struct
import zlib

import unreal

DEST_PATTERN = re.compile(r"^/Game(/[A-Za-z0-9_]+)+$")
ASSET_NAME = "M_DNDentParent"
MAX_DENTS = 8

# Núcleo do deslocamento. Entradas: P (posição local, cm), N (normal do vértice no mundo) e os 24 parâmetros dos amassados.
# Saída: float4 (deslocamento xyz no mundo, máscara do amassado em w).
HLSL = """
float3 offset = float3(0, 0, 0);
float mask = 0;
float3 pos[8] = { D0P, D1P, D2P, D3P, D4P, D5P, D6P, D7P };
float rad[8] = { D0R, D1R, D2R, D3R, D4R, D5R, D6R, D7R };
float dep[8] = { D0D, D1D, D2D, D3D, D4D, D5D, D6D, D7D };
[unroll]
for (int i = 0; i < 8; i++)
{
    float r = rad[i];
    if (r > 0.001)
    {
        float d = distance(P, pos[i]);
        float f = saturate(1.0 - d / r);
        f = f * f * (3.0 - 2.0 * f);
        offset += -N * (dep[i] * f);
        mask = max(mask, f * saturate(dep[i] / 5.0));
    }
}
return float4(offset, mask);
"""


# Desgaste da pintura (lote 17b, camada 3 do dano visual): máscaras por pixel a partir da posição local P (cm), da caixa da carroceria
# (C centro, E extensão), do peso de cada zona (ZA = frente, traseira, esquerda; ZB = direita, teto; o C++ manda os mesmos pesos dos
# alvos de deformação), da cor de vértice VC (onde cada zona amassa, com as cristas dos vincos; gravada pelo
# Tools/Blender/make_damage_morphs.py: R frente, G traseira, B laterais, A teto), da máscara dos amassados DM (perto do ponto de
# batida), da normal local NL e de ruído 3D procedural (nenhuma textura). Eixos do Unreal: +X frente, +Y direita, +Z cima.
# Saída: float4 (primer, metal nu, risco, fuligem), cada um de 0 a 1.
# Fim do 17b (revisão): as lascas pareciam confete (retângulos claros espalhados por igual). Agora só nascem onde o dano local
# passa de 0,08, em grupos, mais densas perto da batida; a borda vem de ruído com distorção de domínio; o teto não pega a asa. Os
# "retângulos" eram os riscos: o ruído deles é comprido em X e curto em Y e Z, e nas faces de frente (plano Y-Z) virava quadradinho
# de 3 cm. Raspão corre ao longo do carro: agora some nas faces viradas para a frente e para trás.
WEAR_HLSL = """
struct DNWearNoise
{
    float h(float3 p) { return frac(sin(dot(p, float3(127.1, 311.7, 74.7))) * 43758.5453); }
    float v(float3 x)
    {
        float3 i = floor(x);
        float3 f = frac(x);
        f = f * f * (3.0 - 2.0 * f);
        return lerp(lerp(lerp(h(i), h(i + float3(1, 0, 0)), f.x), lerp(h(i + float3(0, 1, 0)), h(i + float3(1, 1, 0)), f.x), f.y),
                    lerp(lerp(h(i + float3(0, 0, 1)), h(i + float3(1, 0, 1)), f.x), lerp(h(i + float3(0, 1, 1)), h(i + float3(1, 1, 1)), f.x), f.y), f.z);
    }
    float fbm(float3 x) { return 0.5 * v(x) + 0.3 * v(x * 2.03 + 11.1) + 0.2 * v(x * 4.01 + 23.7); }
};
DNWearNoise n;
float3 q = (P - C) / max(E, float3(1.0, 1.0, 1.0));
float fx = q.x * 0.5 + 0.5;
float hz = q.z * 0.5 + 0.5;
// onde amassou: o peso de cada zona vezes a cor de vértice (cristas dos vincos), dentro da região larga da zona (a caixa limita a
// carroceria estática de reserva, que não tem a cor de vértice e chega toda branca). O teto só sobre a cabine: a asa traseira, alta,
// é da traseira
float wf = 1.0 - smoothstep(0.0, 0.35, 1.0 - fx);
float wr = 1.0 - smoothstep(0.0, 0.35, fx);
float wt = smoothstep(0.6, 0.9, hz) * smoothstep(0.12, 0.25, fx) * (1.0 - smoothstep(0.70, 0.80, fx));
float sideW = q.y < 0.0 ? ZA.z * smoothstep(0.3, 0.8, -q.y) : ZB.x * smoothstep(0.3, 0.8, q.y);
float d = saturate(ZA.x * wf * VC.r + ZA.y * wr * VC.g + sideW * VC.b + ZB.y * wt * VA);
// a ponta da frente e a da traseira são a chapa que mais amassa: contam como dobra, com o dano da zona. P é a posição já amassada
// (a frente em 1,0 recua ~60 cm), então a faixa vai até 25% do comprimento a partir da caixa intacta
float tipF = 1.0 - smoothstep(0.0, 0.25, 1.0 - fx);
float tipR = 1.0 - smoothstep(0.0, 0.25, fx);
d = saturate(d + 0.45 * (ZA.x * tipF + ZA.y * tipR));
// concentração: nada em chapa lisa (dano local abaixo de 0,08), cheio nas cristas e perto da batida (máscara dos amassados)
float k = saturate((d - 0.08) / 0.5 + DM * 3.0 * step(0.001, d));
// grupos de tamanhos variados: manchas largas que ligam e desligam a lasca
float cluster = smoothstep(0.42, 0.62, n.fbm(P / 55.0 + 7.7));
// borda irregular: o campo da lasca é lido num espaço distorcido por outro ruído (sem célula quadrada à vista)
float3 warp = float3(n.fbm(P / 16.0 + 3.1), n.fbm(P / 16.0 + 17.3), n.fbm(P / 16.0 + 41.7)) - 0.5;
float3 Q = P + warp * 18.0;
float field = n.fbm(Q / 28.0) * 0.55 + n.fbm(Q / max(Size, 1.0)) * 0.45;
float th = 0.77 - 0.30 * saturate(Cov * 5.0) * k * (0.35 + 0.65 * cluster);
float chip = smoothstep(th, th + 0.015, field) * step(0.05, k) * Paint;
// no miolo das lascas, metal nu (18b: o miolo cresceu e a borda de primer ficou mais estreita; revisão do 17b)
float metal = smoothstep(th + 0.045, th + 0.06, field) * step(0.05, k) * Paint;
float primer = saturate(chip - metal);
// riscos compridos ao longo do carro (raspão), finos na altura, só onde amassou e só nas faces que correm ao longo do carro
float s = n.v(float3(P.x / 120.0, P.y / 2.5, P.z / 2.5));
float along = 1.0 - smoothstep(0.35, 0.65, abs(normalize(NL).x));
float scratch = smoothstep(0.74, 0.79, s) * Scr * saturate(d * 2.0) * along * Paint;
// fuligem e sujeira: manchas largas, mais fortes onde amassou mais
float soot = Soot * d * (0.6 + 0.4 * n.v(P / 40.0));
return float4(primer, metal, scratch, soot);
"""

# Cor em camadas (fim do 17b): primer cinza (albedo 0,35; era 0,5 e saltava na foto, revisão do 17b feita no 18b), metal nu
# cinza-aço; o risco mostra primer (antes clareava a tinta e saía rosa); a fuligem escurece
WEAR_COLOR_HLSL = """
float3 c = lerp(Col, float3(0.35, 0.35, 0.34), M.x);
c = lerp(c, float3(0.56, 0.57, 0.58), M.y);
c = lerp(c, float3(0.45, 0.45, 0.44), 0.8 * M.z);
return c * (1.0 - 0.8 * M.w);
"""

# Trinca de vidro (lote 18b, docs/regras-pecas.md): só nos vidros (GlassCrack > 0; a carroceria fica com 0). Em volta do centro GC
# (o ponto da batida mais perto do vidro, no espaço do carro), linhas radiais que serpenteiam e pedaços de anel em polígono (cordas
# entre duas radiais, como na teia de um para-brisa), num alcance que cresce com a trinca,
# uma teia densa perto do centro e o miolo esmagado. As linhas são medidas em cm no plano do vidro (base tirada da normal local NL).
# Lote 19 (revisão do 18b: "teia desenhada"): a linha não pinta o vidro de branco. Ela é fina (~0,4 cm), fica áspera e inclina a
# normal, então só brilha onde pega luz, e some na borda. Só o miolo e a teia do centro esbranquiçam. Saída: (linha, miolo,
# inclinação da normal em x e y no espaço tangente).
GLASS_CRACK_HLSL = """
if (Cr <= 0.001)
{
    return float4(0, 0, 0, 0);
}
float3 d = P - GC;
float r = length(d);
float reach = 20.0 + 140.0 * Cr;
float fade = 1.0 - smoothstep(0.55 * reach, reach, r);
if (fade <= 0.0)
{
    return float4(0, 0, 0, 0);
}
float3 n = normalize(NL);
float3 t1 = normalize(cross(n, abs(n.z) < 0.9 ? float3(0, 0, 1) : float3(1, 0, 0)));
float3 t2 = cross(n, t1);
float2 q = float2(dot(d, t1), dot(d, t2));
float turn = atan2(q.y, q.x) / 6.2831853 + 0.5;
float s = turn * 16.0;
float sector = floor(s);
float seed = frac(sin(sector * 37.719 + 4.1) * 43758.5453);
float wobble = 0.12 * sin(r * (0.06 + 0.06 * seed) + seed * 6.2831853);
float radialCm = abs(frac(s) - 0.5 - wobble) * 6.2831853 * max(r, 0.5) / 16.0;
float radial = 1.0 - smoothstep(0.10, 0.22, radialCm);
float cell = fmod(floor(s + 0.5), 16.0);
float cellSeed = frac(sin(cell * 91.137 + 2.3) * 43758.5453);
float chord = r * cos((frac(s + 0.5) - 0.5) * 6.2831853 / 16.0) + cellSeed * 5.0;
float ring = floor(chord / 8.0);
float keep = step(0.40, frac(sin(ring * 78.233 + cell * 12.9898) * 43758.5453));
float ringCm = abs(chord - (ring + 0.5) * 8.0);
float rings = (1.0 - smoothstep(0.08, 0.18, ringCm)) * keep * step(4.0, r);
float b = frac(turn * 48.0);
float webCm = min(b, 1.0 - b) * 6.2831853 * max(r, 0.5) / 48.0;
// a teia do centro em pedaços: cada raio fino aparece em trechos de 3 cm ao acaso (inteira, parecia um sol desenhado)
float webKeep = step(0.45, frac(sin(floor(turn * 48.0) * 12.9898 + floor(r / 3.0) * 7.31) * 43758.5453));
float web = (1.0 - smoothstep(0.12, 0.25, webCm)) * (1.0 - smoothstep(8.0, 18.0, r)) * webKeep;
float core = 1.0 - smoothstep(3.0, 6.0, r);
float strength = fade * smoothstep(0.0, 0.3, Cr);
float edge = 1.0 - 0.85 * smoothstep(0.15 * reach, reach, r);
float crackLine = saturate(max(radial, rings)) * edge * strength;  // "line" é palavra reservada do HLSL
float crackCenter = saturate(max(web, core)) * strength;
float angle = (radial >= rings ? seed : cellSeed) * 6.2831853;
return float4(crackLine, crackCenter, 0.55 * cos(angle), 0.55 * sin(angle));
"""

# Lote 19: o vidro trincado a partir da máscara K da trinca. A cor só esbranquiça no miolo; a linha deixa a superfície áspera e
# sem verniz e inclina a normal (brilha onde pega luz).
GLASS_CRACK_COLOR_HLSL = "return lerp(Col, float3(0.62, 0.65, 0.67), saturate(0.85 * K.y + 0.08 * K.x));"
GLASS_CRACK_SURFACE_HLSL = """
float rough = lerp(lerp(Rgh, 0.5, K.x), 0.8, K.y);
float coat = Coat * (1.0 - max(K.x, K.y));
return float3(rough, coat, 0.0);
"""
GLASS_CRACK_NORMAL_HLSL = "return normalize(N + float3(K.z, K.w, 0.0) * max(K.x, 0.5 * K.y));"

# Lote 19 (revisão do 18b: o vão era "preto chapado e facetado"): ruído 3D pela posição no carro, sem UV (o vão não tem), só na
# instância com a chave SurfaceNoise ligada (MI_DNVao). Duas oitavas de ruído de valor em cm: manchas de ~25 cm e grão de ~4 cm.
SURFACE_NOISE_HLSL = """
float total = 0.0;
float amp = 0.65;
float3 p = P / 25.0;
for (int o = 0; o < 2; ++o)
{
    float3 i = floor(p);
    float3 f = frac(p);
    f = f * f * (3.0 - 2.0 * f);
    float n000 = frac(sin(dot(i, float3(127.1, 311.7, 74.7))) * 43758.5453);
    float n100 = frac(sin(dot(i + float3(1, 0, 0), float3(127.1, 311.7, 74.7))) * 43758.5453);
    float n010 = frac(sin(dot(i + float3(0, 1, 0), float3(127.1, 311.7, 74.7))) * 43758.5453);
    float n110 = frac(sin(dot(i + float3(1, 1, 0), float3(127.1, 311.7, 74.7))) * 43758.5453);
    float n001 = frac(sin(dot(i + float3(0, 0, 1), float3(127.1, 311.7, 74.7))) * 43758.5453);
    float n101 = frac(sin(dot(i + float3(1, 0, 1), float3(127.1, 311.7, 74.7))) * 43758.5453);
    float n011 = frac(sin(dot(i + float3(0, 1, 1), float3(127.1, 311.7, 74.7))) * 43758.5453);
    float n111 = frac(sin(dot(i + float3(1, 1, 1), float3(127.1, 311.7, 74.7))) * 43758.5453);
    float v = lerp(lerp(lerp(n000, n100, f.x), lerp(n010, n110, f.x), f.y), lerp(lerp(n001, n101, f.x), lerp(n011, n111, f.x), f.y), f.z);
    total += v * amp;
    amp = 0.35;
    p *= 6.0;
}
return total;
"""
SURFACE_NOISE_COLOR_HLSL = "return Col * (0.65 + 0.7 * N);"
SURFACE_NOISE_ROUGH_HLSL = "return saturate(Rgh + (N - 0.5) * 0.25);"

# (metálico, aspereza, verniz): primer fosco e sem verniz, metal nu sem verniz, risco tira metade do verniz, fuligem deixa fosco
WEAR_SURFACE_HLSL = """
float metal = lerp(lerp(Met, 0.0, M.x), 1.0, M.y);
float rough = lerp(lerp(lerp(Rgh, 0.70, M.x), 0.35, M.y), 0.6, M.z);
rough = lerp(rough, 0.85, M.w);
float coat = Coat * (1.0 - M.x - M.y) * (1.0 - 0.5 * M.z) * (1.0 - 0.7 * M.w);
return float3(metal, saturate(rough), saturate(coat));
"""


def custom_node(mel, material, code, description, names, output_type, x, y):
    n = node(mel, material, unreal.MaterialExpressionCustom, x, y)
    n.set_editor_property("code", code)
    n.set_editor_property("description", description)
    n.set_editor_property("output_type", output_type)
    inputs = []
    for name in names:
        ci = unreal.CustomInput()
        ci.set_editor_property("input_name", name)
        inputs.append(ci)
    n.set_editor_property("inputs", inputs)
    return n


def channel(mel, material, src, which, x, y):
    m = node(mel, material, unreal.MaterialExpressionComponentMask, x, y)
    for c in "rgba":
        m.set_editor_property(c, c == which)
    mel.connect_material_expressions(src, "", m, "")
    return m


class DentError(RuntimeError):
    """Erro esperado, com mensagem para o usuário."""


def log(message):
    unreal.log("[create_dent_material] " + message)


def project_dir():
    return os.path.abspath(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))


def parse_args(argv):
    parser = argparse.ArgumentParser(prog="create_dent_material.py", add_help=False)
    parser.add_argument("--dest", required=True)
    parser.add_argument("--report", default=None)
    # só refaz as instâncias e veste o carro, sem tocar no material-pai: para mudança só de valores (mais rápido, sem recompilar)
    parser.add_argument("--instances-only", action="store_true")
    try:
        return parser.parse_args(argv)
    except SystemExit:
        raise DentError("Argumentos inválidos. Uso: --dest /Game/<pasta> [--report <arquivo>] [--instances-only]")


def resolve_report(path_arg):
    project = project_dir()
    path = os.path.abspath(path_arg) if path_arg else os.path.join(project, "Saved", "DentMaterialReport.txt")
    if not os.path.normcase(path).startswith(os.path.normcase(project) + os.sep):
        raise DentError("Relatório recusado: '%s' fica fora da pasta do projeto (%s)." % (path, project))
    return path


def node(mel, material, cls, x, y):
    return mel.create_material_expression(material, cls, x, y)


def scalar_param(mel, material, name, value, x, y):
    n = node(mel, material, unreal.MaterialExpressionScalarParameter, x, y)
    n.set_editor_property("parameter_name", name)
    n.set_editor_property("default_value", value)
    return n


def vector_param(mel, material, name, value, x, y):
    n = node(mel, material, unreal.MaterialExpressionVectorParameter, x, y)
    n.set_editor_property("parameter_name", name)
    n.set_editor_property("default_value", unreal.LinearColor(*value))
    return n


def static_switch(mel, material, name, when_on, when_off, x, y):
    """Chave estática (desligada por padrão): a instância que liga ganha a própria compilação; nas outras o ramo some."""
    n = node(mel, material, unreal.MaterialExpressionStaticSwitchParameter, x, y)
    n.set_editor_property("parameter_name", name)
    n.set_editor_property("default_value", False)
    mel.connect_material_expressions(when_on, "", n, "True")
    mel.connect_material_expressions(when_off, "", n, "False")
    return n


def write_flake_png(path, size=256, cell=2, tilt=0.35, seed=7):
    """Normal map de flocos metálicos (conteúdo próprio, gerado aqui): células de cell x cell px, cada uma com uma normal inclinada
    ao acaso até 'tilt' (~20 graus). PNG RGB 8 bits escrito só com a biblioteca padrão."""
    rng = random.Random(seed)
    cells = size // cell
    grid = []
    for _ in range(cells):
        row = []
        for _ in range(cells):
            nx, ny = rng.uniform(-tilt, tilt), rng.uniform(-tilt, tilt)
            nz = math.sqrt(max(0.0, 1.0 - nx * nx - ny * ny))
            row.append(bytes(int(round((c * 0.5 + 0.5) * 255.0)) for c in (nx, ny, nz)))
        grid.append(row)
    rows = [b"".join(grid[y // cell][x // cell] for x in range(size)) for y in range(size)]
    raw = b"".join(b"\x00" + r for r in rows)

    def chunk(kind, data):
        return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data) & 0xFFFFFFFF)

    png = b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", size, size, 8, 2, 0, 0, 0)) + chunk(b"IDAT", zlib.compress(raw, 9)) + chunk(b"IEND", b"")
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "wb") as handle:
        handle.write(png)


def import_flakes(dest, report):
    """Gera e importa T_DNFlocos_N em <dest> (normal map, sem sRGB)."""
    png = os.path.join(project_dir(), "Saved", "DentMaterial", "T_DNFlocos_N.png")
    write_flake_png(png)
    task = unreal.AssetImportTask()
    task.filename = png
    task.destination_path = dest
    task.destination_name = "T_DNFlocos_N"
    task.automated = True
    task.replace_existing = True
    task.save = False
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    tex = unreal.load_asset(dest + "/T_DNFlocos_N")
    if not isinstance(tex, unreal.Texture2D):
        raise DentError("Não consegui importar a textura de flocos em " + dest)
    tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP)
    tex.set_editor_property("srgb", False)
    unreal.EditorAssetLibrary.save_asset(dest + "/T_DNFlocos_N")
    report.append("textura de flocos gerada: " + dest + "/T_DNFlocos_N")
    return tex


def find_texture(name, fallback):
    """A textura do carro do template pelo nome (o caminho muda entre o projeto e o Sandbox); senão, a da engine."""
    for a in unreal.AssetRegistryHelpers.get_asset_registry().get_assets_by_path("/Game", recursive=True):
        if str(a.asset_name) == name:
            tex = unreal.load_asset(str(a.package_name))
            if isinstance(tex, unreal.Texture):
                return tex
    return unreal.load_asset(fallback)


def build(dest, report):
    if not DEST_PATTERN.match(dest):
        raise DentError("Destino recusado: '%s'. Só /Game/<pasta> do projeto é aceito." % dest)

    eal = unreal.EditorAssetLibrary
    path = dest + "/" + ASSET_NAME
    mel = unreal.MaterialEditingLibrary
    flakes = import_flakes(dest, report)
    if eal.does_asset_exist(path):
        # refaz no mesmo pacote: apagar no commandlet deixa o arquivo no disco e a criação seguinte falha
        material = unreal.load_asset(path)
        # o delete_all do 5.8 apaga só metade dos nós por chamada (medido no lote 17b: 85 -> 42). Os restos se acumulavam a cada
        # execução até haver duas saídas da normal de baixo do verniz, e aí o material não compila e o jogo mostra o padrão cinza
        # ("The material can contain only one ClearCoatBottomNormal node"). Chama até zerar.
        left = mel.get_num_material_expressions(material)
        for _ in range(16):
            if left == 0:
                break
            mel.delete_all_material_expressions(material)
            left = mel.get_num_material_expressions(material)
        if left:
            raise DentError("Sobraram %d nós no material depois de apagar tudo: %s" % (left, path))
        report.append("material existente refeito: " + path)
    else:
        material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(ASSET_NAME, dest, unreal.Material, unreal.MaterialFactoryNew())
    if material is None:
        raise DentError("Não consegui criar o material em " + dest)

    # Usado em malha estática e esquelética (os pedaços do carro são estáticos presos ao esqueleto).
    material.set_editor_property("used_with_skeletal_mesh", True)
    # Lote 17: a carroceria virou malha com esqueleto e alvos de deformação (SK_DNSportsCarBody); sem esta flag o jogo mostra o
    # material padrão (xadrez cinza) no lugar da pintura.
    material.set_editor_property("used_with_morph_targets", True)
    # A malha da carroceria é Nanite: sem esta flag a engine troca o material pelo padrão (aviso "missing usage flag Nanite").
    material.set_editor_property("used_with_nanite", True)
    # Nanite precisa saber o quanto o material pode deslocar um vértice (para não cortar o objeto por engano).
    try:
        material.set_editor_property("max_world_position_offset_displacement", 40.0)
    except Exception as exc:  # o nome da propriedade pode variar entre versões da engine: registra e segue
        unreal.log_warning("[create_dent_material] não consegui definir o deslocamento máximo do WPO: %r" % (exc,))

    # Pintura automotiva (lote 16): verniz (clear coat) liso por cima e base metálica com flocos por baixo. O verniz só entra pelos
    # atributos de material (o Python da 5.8 não expõe o pino de verniz direto no material).
    material.set_editor_property("use_material_attributes", True)
    material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_CLEAR_COAT)

    # --- parâmetros com os MESMOS nomes do material do carro do template (o C++ copia o visual de cada slot) ---
    def tex_param(name, texture, sampler, x, y):
        n = node(mel, material, unreal.MaterialExpressionTextureSampleParameter2D, x, y)
        n.set_editor_property("parameter_name", name)
        n.set_editor_property("texture", texture)
        # o tipo de amostragem tem que casar com a compressão da textura, senão o material não compila
        compression = texture.get_editor_property("compression_settings")
        if compression == unreal.TextureCompressionSettings.TC_NORMALMAP:
            sampler = unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL
        elif compression == unreal.TextureCompressionSettings.TC_MASKS:
            sampler = unreal.MaterialSamplerType.SAMPLERTYPE_MASKS
        elif sampler != unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL:
            sampler = unreal.MaterialSamplerType.SAMPLERTYPE_COLOR if texture.get_editor_property("srgb") else unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR
        n.set_editor_property("sampler_type", sampler)
        return n

    base_tex = tex_param("Base Texture", find_texture("T_SportsCar_Body_D", "/Engine/EngineResources/WhiteSquareTexture"),
                         unreal.MaterialSamplerType.SAMPLERTYPE_COLOR, -1700, -700)
    mra_tex = tex_param("MRA", find_texture("T_SportsCar_Body_MRA", "/Engine/EngineResources/WhiteSquareTexture"),
                        unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR, -1700, -450)
    body_n = tex_param("Normal", find_texture("T_SportsCar_Body_BotN", "/Engine/EngineMaterials/DefaultNormal"),
                       unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL, -1700, -200)
    tint = vector_param(mel, material, "Paint Tint", (0.16, 0.012, 0.012, 1.0), -1700, -850)  # vermelho escuro metálico
    metallic = scalar_param(mel, material, "MetalPaintMetallic", 0.7, -1400, -560)
    roughness = scalar_param(mel, material, "Roughness", 0.4, -1400, -500)
    coat = scalar_param(mel, material, "ClearCoat", 1.0, -1400, -440)
    coat_rough = scalar_param(mel, material, "ClearCoatRoughness", 0.04, -1400, -380)  # 16b: 0,03 a 0,05 (revisão do par)

    color = node(mel, material, unreal.MaterialExpressionMultiply, -1400, -750)
    mel.connect_material_expressions(base_tex, "RGB", color, "A")
    mel.connect_material_expressions(tint, "", color, "B")

    # flocos: normal procedural repetida pelo UV, misturada à normal da carroceria na camada de baixo do verniz
    uv = node(mel, material, unreal.MaterialExpressionTextureCoordinate, -1900, 50)
    tiling = scalar_param(mel, material, "FlakeTiling", 60.0, -1900, 150)  # 16b: flocos menores (30 granulava de perto)
    flake_uv = node(mel, material, unreal.MaterialExpressionMultiply, -1700, 80)
    mel.connect_material_expressions(uv, "", flake_uv, "A")
    mel.connect_material_expressions(tiling, "", flake_uv, "B")
    flake_n = node(mel, material, unreal.MaterialExpressionTextureSample, -1500, 80)
    flake_n.set_editor_property("texture", flakes)
    flake_n.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
    mel.connect_material_expressions(flake_uv, "", flake_n, "UVs")
    strength = scalar_param(mel, material, "FlakeStrength", 0.25, -1500, 250)  # 16b: 0,6 era forte demais (pintura parecia lixa)
    blend = node(mel, material, unreal.MaterialExpressionCustom, -1200, 0)
    blend.set_editor_property("code", "return normalize(float3(A.xy + B.xy * S, A.z));")
    blend.set_editor_property("description", "Flocos na normal")
    blend.set_editor_property("output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    blend_inputs = []
    for name in ("A", "B", "S"):
        ci = unreal.CustomInput()
        ci.set_editor_property("input_name", name)
        blend_inputs.append(ci)
    blend.set_editor_property("inputs", blend_inputs)
    mel.connect_material_expressions(body_n, "RGB", blend, "A")
    mel.connect_material_expressions(flake_n, "RGB", blend, "B")
    mel.connect_material_expressions(strength, "", blend, "S")
    bottom = node(mel, material, unreal.MaterialExpressionClearCoatNormalCustomOutput, -900, 0)
    mel.connect_material_expressions(blend, "", bottom, "")

    # --- posição local sem deslocamentos e normal do vértice ---
    world = node(mel, material, unreal.MaterialExpressionWorldPosition, -1400, 100)
    world.set_editor_property("world_position_shader_offset", unreal.WorldPositionIncludedOffsets.WPT_EXCLUDE_ALL_SHADER_OFFSETS)
    local = node(mel, material, unreal.MaterialExpressionTransformPosition, -1150, 100)
    local.set_editor_property("transform_source_type", unreal.MaterialPositionTransformSource.TRANSFORMPOSSOURCE_WORLD)
    local.set_editor_property("transform_type", unreal.MaterialPositionTransformSource.TRANSFORMPOSSOURCE_LOCAL)
    mel.connect_material_expressions(world, "", local, "")
    normal = node(mel, material, unreal.MaterialExpressionVertexNormalWS, -1150, 250)

    # --- nó Custom com o laço dos 8 amassados ---
    custom = node(mel, material, unreal.MaterialExpressionCustom, -700, 300)
    custom.set_editor_property("code", HLSL)
    custom.set_editor_property("description", "Amassados (WPO)")
    custom.set_editor_property("output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT4)
    input_names = ["P", "N"] + ["D%dP" % i for i in range(MAX_DENTS)] + ["D%dR" % i for i in range(MAX_DENTS)] + ["D%dD" % i for i in range(MAX_DENTS)]
    inputs = []
    for name in input_names:
        ci = unreal.CustomInput()
        ci.set_editor_property("input_name", name)
        inputs.append(ci)
    custom.set_editor_property("inputs", inputs)

    mel.connect_material_expressions(local, "", custom, "P")
    mel.connect_material_expressions(normal, "", custom, "N")
    for i in range(MAX_DENTS):
        pos = vector_param(mel, material, "D%dP" % i, (0.0, 0.0, 0.0, 0.0), -1500, 500 + i * 130)
        rad = scalar_param(mel, material, "D%dR" % i, 0.0, -1250, 500 + i * 130)
        dep = scalar_param(mel, material, "D%dD" % i, 0.0, -1000, 500 + i * 130)
        mel.connect_material_expressions(pos, "", custom, "D%dP" % i)
        mel.connect_material_expressions(rad, "", custom, "D%dR" % i)
        mel.connect_material_expressions(dep, "", custom, "D%dD" % i)

    # --- saídas: deslocamento -> WPO; máscara -> escurece a cor ---
    offset = node(mel, material, unreal.MaterialExpressionComponentMask, -400, 300)
    offset.set_editor_property("r", True)
    offset.set_editor_property("g", True)
    offset.set_editor_property("b", True)
    offset.set_editor_property("a", False)
    mel.connect_material_expressions(custom, "", offset, "")

    mask = node(mel, material, unreal.MaterialExpressionComponentMask, -400, 450)
    mask.set_editor_property("r", False)
    mask.set_editor_property("g", False)
    mask.set_editor_property("b", False)
    mask.set_editor_property("a", True)
    mel.connect_material_expressions(custom, "", mask, "")

    darkening = scalar_param(mel, material, "DentDarkening", 0.6, -400, 600)
    scaled = node(mel, material, unreal.MaterialExpressionMultiply, -150, 500)
    mel.connect_material_expressions(mask, "", scaled, "A")
    mel.connect_material_expressions(darkening, "", scaled, "B")
    keep = node(mel, material, unreal.MaterialExpressionOneMinus, 50, 500)
    mel.connect_material_expressions(scaled, "", keep, "")
    painted = node(mel, material, unreal.MaterialExpressionMultiply, 250, 0)
    mel.connect_material_expressions(color, "", painted, "A")
    mel.connect_material_expressions(keep, "", painted, "B")

    # --- desgaste da pintura por zona (lote 17b): o C++ manda a caixa da carroceria, os pesos das zonas e os valores de Damage.csv ---
    wear = custom_node(mel, material, WEAR_HLSL, "Desgaste por zona (lascas, metal, riscos, fuligem)",
                       ("P", "C", "E", "ZA", "ZB", "Cov", "Size", "Scr", "Soot", "Paint", "VC", "VA", "DM", "NL"), unreal.CustomMaterialOutputType.CMOT_FLOAT4, 0, 800)
    mel.connect_material_expressions(local, "", wear, "P")
    # a normal no espaço do carro: os riscos correm ao longo do carro e somem nas faces de frente e de trás
    local_n = node(mel, material, unreal.MaterialExpressionTransform, -400, 1750)
    local_n.set_editor_property("transform_source_type", unreal.MaterialVectorCoordTransformSource.TRANSFORMSOURCE_WORLD)
    local_n.set_editor_property("transform_type", unreal.MaterialVectorCoordTransform.TRANSFORM_LOCAL)
    mel.connect_material_expressions(normal, "", local_n, "")
    mel.connect_material_expressions(local_n, "", wear, "NL")
    mel.connect_material_expressions(mask, "", wear, "DM")  # perto da batida: a máscara dos amassados
    # a saída principal do nó de cor de vértice tem 3 canais (RGB): o alfa (teto) vai por um pino próprio
    vertex_color = node(mel, material, unreal.MaterialExpressionVertexColor, -400, 1680)
    mel.connect_material_expressions(vertex_color, "", wear, "VC")
    mel.connect_material_expressions(vertex_color, "A", wear, "VA")
    wear_params = (
        (vector_param(mel, material, "BodyCenter", (0.0, 0.0, 60.0, 0.0), -400, 800), "C"),
        (vector_param(mel, material, "BodyExtent", (235.0, 114.0, 62.0, 0.0), -400, 930), "E"),
        (vector_param(mel, material, "ZoneWearA", (0.0, 0.0, 0.0, 0.0), -400, 1060), "ZA"),
        (vector_param(mel, material, "ZoneWearB", (0.0, 0.0, 0.0, 0.0), -400, 1190), "ZB"),
        (scalar_param(mel, material, "WearChipCoverage", 0.2, -400, 1320), "Cov"),
        (scalar_param(mel, material, "WearChipSizeCm", 6.0, -400, 1390), "Size"),
        (scalar_param(mel, material, "WearScratch", 0.6, -400, 1460), "Scr"),
        (scalar_param(mel, material, "WearSoot", 0.5, -400, 1530), "Soot"),
        # 1 na pintura; o chassi (janelas, grade e plástico preto) fica sem lascas nem riscos, só com fuligem (MI_DNChassi põe 0)
        (scalar_param(mel, material, "WearPaint", 1.0, -400, 1600), "Paint"),
    )
    for src, pin in wear_params:
        mel.connect_material_expressions(src, "", wear, pin)
    # lote 19: o vão (MI_DNVao, chave SurfaceNoise) ganha manchas e grão pela posição; nas outras instâncias a chave some na compilação
    noise = custom_node(mel, material, SURFACE_NOISE_HLSL, "Ruído de superfície (vão)", ("P",), unreal.CustomMaterialOutputType.CMOT_FLOAT1, 0, 1950)
    mel.connect_material_expressions(local, "", noise, "P")
    noisy_color = custom_node(mel, material, SURFACE_NOISE_COLOR_HLSL, "Cor com ruído", ("Col", "N"), unreal.CustomMaterialOutputType.CMOT_FLOAT3, 200, 1950)
    mel.connect_material_expressions(painted, "", noisy_color, "Col")
    mel.connect_material_expressions(noise, "", noisy_color, "N")
    color_in = static_switch(mel, material, "SurfaceNoise", noisy_color, painted, 350, 1950)
    worn_color = custom_node(mel, material, WEAR_COLOR_HLSL, "Cor com desgaste", ("Col", "M"), unreal.CustomMaterialOutputType.CMOT_FLOAT3, 450, 300)
    mel.connect_material_expressions(color_in, "", worn_color, "Col")
    mel.connect_material_expressions(wear, "", worn_color, "M")
    # lote 18b: a trinca dos vidros por cima (o carro manda GlassCrack e GlassCrackCenter nos materiais dos vidros); lote 19: máscara K
    crack = custom_node(mel, material, GLASS_CRACK_HLSL, "Trinca de vidro", ("P", "GC", "Cr", "NL"), unreal.CustomMaterialOutputType.CMOT_FLOAT4, 650, 300)
    mel.connect_material_expressions(local, "", crack, "P")
    mel.connect_material_expressions(vector_param(mel, material, "GlassCrackCenter", (0.0, 0.0, 0.0, 0.0), 450, 150), "", crack, "GC")
    mel.connect_material_expressions(scalar_param(mel, material, "GlassCrack", 0.0, 450, 220), "", crack, "Cr")
    mel.connect_material_expressions(local_n, "", crack, "NL")
    cracked = custom_node(mel, material, GLASS_CRACK_COLOR_HLSL, "Cor do vidro trincado", ("Col", "K"), unreal.CustomMaterialOutputType.CMOT_FLOAT3, 850, 300)
    mel.connect_material_expressions(worn_color, "", cracked, "Col")
    mel.connect_material_expressions(crack, "", cracked, "K")
    surface = custom_node(mel, material, WEAR_SURFACE_HLSL, "Superfície com desgaste", ("Met", "Rgh", "Coat", "M"),
                          unreal.CustomMaterialOutputType.CMOT_FLOAT3, 450, 600)
    for src, pin in ((metallic, "Met"), (roughness, "Rgh"), (coat, "Coat"), (wear, "M")):
        mel.connect_material_expressions(src, "", surface, pin)
    worn_metal = channel(mel, material, surface, "r", 650, 550)
    worn_rough = channel(mel, material, surface, "g", 650, 650)
    worn_coat = channel(mel, material, surface, "b", 650, 750)
    noisy_rough = custom_node(mel, material, SURFACE_NOISE_ROUGH_HLSL, "Aspereza com ruído", ("Rgh", "N"), unreal.CustomMaterialOutputType.CMOT_FLOAT1, 200, 2100)
    mel.connect_material_expressions(worn_rough, "", noisy_rough, "Rgh")
    mel.connect_material_expressions(noise, "", noisy_rough, "N")
    rough_in = static_switch(mel, material, "SurfaceNoise", noisy_rough, worn_rough, 350, 2100)
    crack_surface = custom_node(mel, material, GLASS_CRACK_SURFACE_HLSL, "Superfície do vidro trincado", ("Rgh", "Coat", "K"),
                                unreal.CustomMaterialOutputType.CMOT_FLOAT3, 850, 650)
    mel.connect_material_expressions(rough_in, "", crack_surface, "Rgh")
    mel.connect_material_expressions(worn_coat, "", crack_surface, "Coat")
    mel.connect_material_expressions(crack, "", crack_surface, "K")
    final_rough = channel(mel, material, crack_surface, "r", 1000, 650)
    final_coat = channel(mel, material, crack_surface, "g", 1000, 750)
    crack_normal = custom_node(mel, material, GLASS_CRACK_NORMAL_HLSL, "Normal do vidro trincado", ("N", "K"), unreal.CustomMaterialOutputType.CMOT_FLOAT3, 850, 850)
    mel.connect_material_expressions(body_n, "RGB", crack_normal, "N")
    mel.connect_material_expressions(crack, "", crack_normal, "K")

    # --- tudo pelos atributos de material (é o único caminho do Python para o verniz) ---
    attrs = node(mel, material, unreal.MaterialExpressionMakeMaterialAttributes, 1200, 0)
    for src, out, pin in ((cracked, "", "BaseColor"), (worn_metal, "", "Metallic"), (final_rough, "", "Roughness"), (crack_normal, "", "Normal"),
                          (offset, "", "WorldPositionOffset"), (final_coat, "", "ClearCoat"), (coat_rough, "", "ClearCoatRoughness"),
                          (mra_tex, "B", "AmbientOcclusion")):
        if not mel.connect_material_expressions(src, out, attrs, pin):
            raise DentError("Ligação de material falhou: %s -> %s" % (src.get_name(), pin))
    mel.connect_material_property(attrs, "", unreal.MaterialProperty.MP_MATERIAL_ATTRIBUTES)

    mel.recompile_material(material)
    if not eal.save_asset(path):
        raise DentError("Não consegui salvar " + path)
    report.append("material criado: " + path)
    report.append("parâmetros: Paint Tint, Base Texture, MRA, Normal, MetalPaintMetallic, Roughness, ClearCoat, ClearCoatRoughness, FlakeTiling, "
                  "FlakeStrength, DentDarkening, D0P..D7P, D0R..D7R, D0D..D7D, BodyCenter, BodyExtent, ZoneWearA, ZoneWearB, "
                  "WearChipCoverage, WearChipSizeCm, WearScratch, WearSoot, WearPaint, GlassCrack, GlassCrackCenter")
    dress_car(dest, material, report)


def find_asset(name, cls):
    for a in unreal.AssetRegistryHelpers.get_asset_registry().get_assets_by_path("/Game", recursive=True):
        if str(a.asset_name) == name:
            asset = unreal.load_asset(str(a.package_name))
            if isinstance(asset, cls):
                return asset
    return None


def make_instance(dest, name, parent, scalars=None, vectors=None, textures=None, switches=None):
    """Instância de material em <dest>, refeita no mesmo pacote se já existe."""
    mel = unreal.MaterialEditingLibrary
    path = dest + "/" + name
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        mi = unreal.load_asset(path)
    else:
        mi = unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, dest, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    mel.set_material_instance_parent(mi, parent)
    mel.clear_all_material_instance_parameters(mi)
    for k, v in (scalars or {}).items():
        mel.set_material_instance_scalar_parameter_value(mi, k, v)
    for k, v in (vectors or {}).items():
        mel.set_material_instance_vector_parameter_value(mi, k, unreal.LinearColor(v[0], v[1], v[2], 1.0))
    for k, v in (textures or {}).items():
        if v is not None:
            mel.set_material_instance_texture_parameter_value(mi, k, v)
    for k, v in (switches or {}).items():
        # o retorno do set é False mesmo quando liga (5.8, medido num estudo): confere lendo de volta
        mel.set_material_instance_static_switch_parameter_value(mi, k, v)
        if mel.get_material_instance_static_switch_parameter_value(mi, k) != v:
            raise DentError("Chave estática '%s' não ficou em %s em %s" % (k, v, name))
    mel.update_material_instance(mi)
    unreal.EditorAssetLibrary.save_asset(path)
    return mi


def dress_car(dest, paint, report):
    """Veste o carro do template: pintura e chassi/janelas na carroceria, roda e pneu na roda (lote 16). Só mexe nas malhas que achar."""
    body = find_asset("SM_SportsCar", unreal.StaticMesh)
    if body is not None:
        # 16b: os valores da pintura ficam explícitos na instância (não dependem do padrão do material-pai): flocos mais finos e
        # mais fracos, verniz na faixa 0,03 a 0,05 pedida na revisão
        pintura = make_instance(dest, "MI_DNPintura", paint, {"FlakeStrength": 0.25, "FlakeTiling": 60.0, "ClearCoatRoughness": 0.04})
        # chassi, grade e janelas: as texturas do chassi do template, verniz bem liso e sem flocos (vidro fumê opaco: o carro não tem interior)
        chassi = make_instance(dest, "MI_DNChassi", paint, {"MetalPaintMetallic": 1.0, "FlakeStrength": 0.0, "ClearCoatRoughness": 0.03, "Roughness": 0.3,
                                                                      "WearPaint": 0.0},
                               {"Paint Tint": (1.0, 0.992, 0.99)},
                               {"Base Texture": find_texture("T_SportsCar_Chasis_D", "/Engine/EngineResources/WhiteSquareTexture"),
                                "MRA": find_texture("T_SportsCar_Chasis_MRA", "/Engine/EngineResources/WhiteSquareTexture"),
                                "Normal": find_texture("T_SportsCar_Chasis_N", "/Engine/EngineMaterials/DefaultNormal")})
        # lote 19: o vão das peças (split_car_parts.py põe o slot MI_DNVao): metal escuro áspero com manchas pela posição, sem verniz
        # e sem lascas (revisão do 18b: albedo ~0,04 a 0,06, aspereza ~0,6, um pouco de ruído). Com metálico 0,3 e aspereza 0,6 o fundo
        # do vão espalhava o reflexo do céu e saía mais claro que o asfalto (captura do lote 19): sem metálico, 0,75 e 0,04
        make_instance(dest, "MI_DNVao", paint, {"MetalPaintMetallic": 0.0, "Roughness": 0.75, "ClearCoat": 0.0, "WearPaint": 0.0, "FlakeStrength": 0.0},
                      {"Paint Tint": (0.04, 0.04, 0.042)},
                      {"Base Texture": unreal.load_asset("/Engine/EngineResources/WhiteSquareTexture"),
                       "MRA": unreal.load_asset("/Engine/EngineResources/WhiteSquareTexture"),
                       "Normal": unreal.load_asset("/Engine/EngineMaterials/DefaultNormal")},
                      {"SurfaceNoise": True})
        report.append("vão: MI_DNVao (metal escuro com ruído)")
        slots = [str(s.material_slot_name) for s in body.get_editor_property("static_materials")]
        for i, slot in enumerate(slots):
            body.set_material(i, pintura if "Body" in slot else chassi)
        unreal.EditorAssetLibrary.save_asset(body.get_path_name().split(".")[0])
        report.append("carroceria %s: slots %s -> pintura no 'Body', chassi nos outros" % (body.get_path_name(), slots))
    wheel = find_asset("SM_SportsCar_Wheel", unreal.StaticMesh)
    wheel_parent = find_asset("M_SportsCar_Wheel", unreal.Material)
    if wheel is not None and wheel_parent is not None:
        roda = make_instance(dest, "MI_DNRoda", wheel_parent, {"Metallic1": 1.0, "Roughness1": 0.22, "Metallic2": 1.0, "Roughness2": 0.12},
                             {"BaseColor1": (0.55, 0.56, 0.58), "BaseColor2": (0.9, 0.9, 0.9)})
        pneu = make_instance(dest, "MI_DNPneu", wheel_parent, {"Metallic1": 0.0, "Roughness1": 0.88, "Metallic2": 0.0, "Roughness2": 0.75},
                             {"BaseColor1": (0.022, 0.022, 0.022), "BaseColor2": (0.09, 0.09, 0.09)},
                             {"Normal": find_texture("T_SportsCar_Tire_N", "/Engine/EngineMaterials/DefaultNormal"),
                              "Masks": find_texture("T_SportsCar_Tire_M_TGA", "/Engine/EngineResources/WhiteSquareTexture")})
        slots = [str(s.material_slot_name) for s in wheel.get_editor_property("static_materials")]
        for i, slot in enumerate(slots):
            wheel.set_material(i, pneu if "Tire" in slot else roda)
        unreal.EditorAssetLibrary.save_asset(wheel.get_path_name().split(".")[0])
        report.append("roda %s: slots %s -> pneu no 'Tire', aro nos outros" % (wheel.get_path_name(), slots))


def main():
    report = []
    error = None
    report_path = None
    try:
        args = parse_args(sys.argv[1:])
        report_path = resolve_report(args.report)
        if args.instances_only:
            if not DEST_PATTERN.match(args.dest):
                raise DentError("Destino recusado: '%s'. Só /Game/<pasta> do projeto é aceito." % args.dest)
            parent = unreal.load_asset(args.dest + "/" + ASSET_NAME)
            if not isinstance(parent, unreal.Material):
                raise DentError("--instances-only precisa do material-pai já criado em " + args.dest)
            dress_car(args.dest, parent, report)
        else:
            build(args.dest, report)
    except DentError as exc:
        error = str(exc)
    except Exception as exc:  # qualquer outro erro também derruba o processo, com mensagem
        error = "Erro inesperado: %r" % (exc,)

    if error:
        report.append("ERRO: " + error)
        unreal.log_error("[create_dent_material] " + error)

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
