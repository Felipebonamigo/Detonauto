# Plano visual: realismo dentro de 60 fps

Escrito em 14 out 2026, depois do primeiro playtest no projeto principal. O
Felipe quer o jogo **visualmente bonito, com realismo nos gráficos**. Até aqui o
projeto investiu em regras, dados e testes; o visual ficou para depois. Este
documento corrige isso: define a base de renderização, a direção de arte e como
cada efeito é verificado.

## O que o playtest e as capturas mostraram

| Observação | Causa provável | Consequência |
|---|---|---|
| Tela do Felipe: céu preto, piso cinza ruidoso, carro chapado | O mapa de teste é o `/Engine/Maps/Entry` com piso e luz criados por código, sem céu, sem atmosfera, sem pós-processo | Nada do que se vê hoje representa o jogo |
| `Config/DefaultEngine.ini` não tem nenhuma linha de renderização | O projeto foi criado mínimo; os modelos da Epic normalmente ligam Lumen, sombras virtuais e campos de distância no próprio `DefaultEngine.ini` | **Provavelmente** o jogo roda sem Lumen e sem sombras virtuais. Conferir no editor e fixar em texto |
| Amassado por deslocamento de vértices: frente "rasgada", faces de dentro escuras, superfície sem relevo de luz | A malha do carro do template é esparsa e feita de cascas separadas; o deslocamento não recalcula a normal | A técnica serve para mossas pequenas, **não** para a deformação estilo Wreckfest |
| Carro do template com pintura lisa e aparência de plástico | Material simples de exemplo | O carro final precisa de material de pintura de verdade |

## Pilares do realismo

1. **Luz.** Lumen (iluminação global e reflexos), sombras virtuais, Sky
   Atmosphere, nuvens volumétricas, exposição física. Para a cidade à noite,
   avaliar MegaLights quando a cidade existir.
2. **Materiais.** Pintura automotiva com verniz (camada de clear coat e flocos),
   pneus e borracha, vidro com quebra, asfalto com variação, poças e umidade.
   Texturas do Poly Haven e do ambientCG (CC0).
3. **Dano crível.** Ver a seção própria abaixo. É o coração do jogo.
4. **Câmera e velocidade.** Campo de visão que abre com a velocidade, tremor
   leve, desfoque de movimento só na câmera e moderado (o Felipe não quer roda
   borrada; desfoque por objeto desligado nas rodas), câmera lenta na batida.
5. **Efeitos.** Faíscas, cacos de vidro, fumaça de pneu, fumaça e fogo de motor,
   destroços com física e tempo de vida. Niagara.
6. **Som.** Motor e impacto pesam tanto quanto a imagem na sensação de
   qualidade. Sonniss GDC e Freesound CC0/CC BY.

## Base de renderização (primeiro passo, só texto)

Fixar em `Config/DefaultEngine.ini`, com comentário em português, e medir:

- Iluminação global e reflexos por Lumen; sombras virtuais; campos de distância
  de malha; TSR como anti-aliasing e escala de resolução.
- Um perfil de escalabilidade **"alvo PS5"** em texto (resolução interna, Lumen e
  sombras em qualidade de console) para medir sempre no mesmo nível, mesmo com a
  RTX 4070 Ti do Felipe sendo mais forte que o alvo.
- Conferir no editor 5.8 o nome exato de cada variável antes de gravar; não
  inventar nomes. Registrar o que mudou em `docs/desempenho.md`.

## Cena de aparência (lookdev)

Um mapa de teste **bonito e fixo**, criado por script (Python do editor, texto
versionado), usado para todas as capturas de avaliação:

- Céu e sol com Sky Atmosphere e nuvens volumétricas, hora fixa (fim de tarde,
  até a bíblia de arte decidir).
- Um trecho de rua: asfalto, calçada, meio-fio, faixa pintada, 2 ou 3 prédios do
  City Sample ao fundo.
- Pontos de câmera fixos para capturas comparáveis: carro parado de três-quartos,
  lateral, frente, e em movimento a 80 km/h.

Sem essa cena, nenhuma decisão visual (pintura, dano, câmera, efeito) tem como
ser julgada.

## Dano: técnica revisada

O deslocamento de vértices sozinho não entrega o que o Felipe pediu. Proposta
em camadas, da mais barata e mais visível para a mais cara:

| Camada | O que faz | Como | Custo |
|---|---|---|---|
| 1. Deformação grande por zona | Frente amassada, traseira afundada, lateral e teto | Alvos de deformação (morph targets) por zona, gerados por script no Blender a partir da malha do carro, com normais corretas; o peso de cada um vem de `ZoneDamage` do `FDNDamage` | Baixo em tempo de jogo; trabalho de preparo por carro |
| 2. Peças que soltam | Capô, portas, para-choques, vidro | Malhas separadas com física e pool, como `docs/regras-dano.md` já prevê | Médio; limite de 24 peças |
| 3. Marcas na superfície | Riscos, tinta arrancada, sujeira, metal exposto | Máscara de dano no material da pintura e decalques no ponto de impacto | Baixo |
| 4. Efeitos no impacto | Faíscas, cacos, fumaça | Niagara | Baixo a médio |
| 5. Mossas locais | Pequenas mossas extras | O deslocamento de vértices atual, só em malha com densidade suficiente | Baixo |

Alternativa a avaliar depois da camada 1: deformação da malha na CPU **só no
instante da batida** (não a cada quadro), com normais recalculadas, numa
carroceria refeita com densidade uniforme. É o mais parecido com o Wreckfest e
o mais caro de preparar.

Tudo depende de **um carro preparado para dano**: carroceria fechada, densidade
uniforme, peças separadas. Os veículos do City Sample são a base prevista; antes
de escolher, conferir em cada um a densidade da malha, se as peças vêm separadas
e se há interior (necessário para a câmera do motorista).

## Como cada efeito visual é aprovado

1. **Captura na cena de aparência**, nos mesmos ângulos, antes e depois.
2. **Captura no modo em que o Felipe joga** (`-Play`, com janela e entrada real),
   não só em cenário automático. O lote 14 mostrou que teste verde não garante
   que o efeito aparece.
3. A sessão da nuvem olha as capturas antes de aprovar.
4. Medição de custo no perfil "alvo PS5".
5. O Felipe dá a palavra final jogando.

## Ordem proposta

1. Base de renderização em texto e medição (sem arte nova).
2. Cena de aparência por script.
3. Escolha do carro do City Sample para a fatia vertical e inspeção da malha.
4. Pintura automotiva e pneus no carro escolhido.
5. Dano: camada 1 (morph por zona) com um carro, depois camadas 2 a 4.
6. Câmera de velocidade e câmera lenta na batida.

Os passos 3 e 5 dependem do **Blender instalado** no PC do Felipe.
