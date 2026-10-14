# Dano visual, camada 1: deformação grande por zona

Contrato do lote 17. Faz parte do `docs/plano-visual.md` (seção "Dano: técnica
revisada"). A regra de quanto dano cada zona tem já existe (`FDNDamage`,
`ZoneDamage` de 0 a 1 por zona). Este lote decide **como isso aparece**.

## Ideia

Para cada zona (frente, traseira, lateral esquerda, lateral direita, teto),
um **alvo de deformação** (morph target) mostra a zona totalmente amassada. O
jogo mistura cada alvo pelo dano da zona. A deformação é feita **uma vez no
Blender, por script**, e chega ao Unreal com as normais certas: a luz mostra o
relevo, o que o deslocamento por vértice do protótipo não fazia.

## Script do Blender (`Tools/Blender/make_damage_morphs.py`)

Roda com `blender --background --python`, como o `export_fbx_batch.py`. Entrada:
um `.blend` ou `.fbx` do carro; saída: `.fbx` com os 5 alvos.

1. **Campo espacial suave, não ruído por vértice.** A deformação é uma função
   da posição no espaço do carro, aplicada igual a todas as cascas da malha.
   Assim peças separadas (para-choque, capô, para-lama) se movem juntas e **não
   abrem frestas** nem mostram faces de dentro, o problema das capturas do
   protótipo.
2. **Forma de cada zona** (valores iniciais, em fração do tamanho do carro):
   - Frente: recua até 18% do comprimento na ponta, caindo a zero a 35% do
     comprimento; capô sobe e enruga levemente no meio (dobra em "V").
   - Traseira: igual, espelhada, até 14%.
   - Laterais: afunda até 12% da largura no centro da porta, caindo a zero
     nas pontas do carro e perto do teto e do assoalho.
   - Teto: afunda até 20% da altura da cabine, colunas inclinam.
   - Em todas: ondulação de baixa frequência (amassado de chapa, 2 a 4 dobras)
     sobre a forma principal, com semente fixa para o resultado ser o mesmo a
     cada execução.
3. **Rodas, vidros e faróis** não recebem alvo nesta camada; o vidro quebra na
   camada 2.
4. Normais recalculadas por alvo (suaves, respeitando arestas vivas acima de
   ~40 graus). Exportar com os alvos e as normais deles.
5. Gera também uma imagem de conferência por zona (render do Blender, 1280x720)
   em `docs/img/dano-zonas/`, para a revisão olhar antes de importar.

## No Unreal

- Importar com os alvos de deformação ligados e as normais importadas (não
  recalculadas), por script Python do editor, versionado.
- `ADNVehicle` define o peso de cada alvo a partir de `ZoneDamage` da zona,
  com subida rápida (0,1 s, o metal não "cresce" devagar) e sem volta.
- O amassado por vértice do lote 10 continua, por cima, só para mossas
  locais pequenas (camada 5).
- `r.SkinCache` já está ligado na base de renderização (necessário para alvos
  de deformação com sombra e Lumen corretos).

## Regra pura (Core, com teste)

`FDNDamageMorph::Weights(ZoneDamage[], Anterior[], Dt, Config)`: peso por zona,
igual ao dano da zona com curva configurável (`MorphCurveExponent` em
`Damage.csv`, padrão 0,7: o amassado aparece cedo e satura devagar), subida
limitada por `MorphRisePerSecond` (padrão 10/s), nunca diminui. Entradas NaN ou
fora de [0, 1] tratadas como nos outros contratos.

## Aprovação

- Capturas na cena de aparência, nos 4 ângulos, com cada zona a 0,3, 0,6 e
  1,0, e a batida real na parede a 50 e 100 km/h.
- Nenhuma fresta nem face de dentro visível nas capturas.
- Custo medido no perfil alvo PS5 com 6 carros amassados.
- O Felipe olha e diz se parece batida de verdade.

## Carro

Fazer primeiro no carro do template, porque já está no projeto. Repetir no
carro do City Sample quando ele for escolhido. Se o carro do template, por ser
simples, não der resultado bom, dizer isso com captura em vez de forçar.
