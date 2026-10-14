# Cena de aparência (lookdev) e base de renderização

Lote 15, passos 1 e 2 de [`plano-visual.md`](plano-visual.md). Tudo daqui foi capturado **jogando**, pelo mesmo caminho do jogo
(`vehicle-probe.ps1`, janela, entrada pelo `PlayerController`), não em cenário sem tela.

## Antes e depois

Os mesmos ângulos nos dois mapas: a câmera de foto do carro (`CAM<graus>` do roteiro de teclas) em três-quartos pela frente-esquerda
(135), lateral esquerda (90) e frente (180), e a câmera de jogo (`alta-longe`) a 80 km/h. Preset `arcade-3`, 1920x1080.

- **Antes:** o mapa de teste (`/Engine/Maps/Entry` com a arena criada em C++) e a renderização sem `.ini`: GI nenhuma, reflexo só de
  tela, sombras comuns. A captura a 80 km/h do "antes" foi refeita depois da mudança do `.ini`, com os três métodos devolvidos aos valores
  antigos por console (`r.DynamicGlobalIlluminationMethod 0`, `r.ReflectionMethod 2`, `r.Shadow.Virtual.Enable 0`); as outras três
  são da renderização antiga de fato.
- **Depois:** `L_Lookdev` com a base nova e o perfil `DN_AlvoPS5`.

| Ângulo | Antes | Depois |
|---|---|---|
| Três-quartos | ![antes](img/lookdev/antes-tres-quartos.jpg) | ![depois](img/lookdev/depois-tres-quartos.jpg) |
| Lateral | ![antes](img/lookdev/antes-lateral.jpg) | ![depois](img/lookdev/depois-lateral.jpg) |
| Frente | ![antes](img/lookdev/antes-frente.jpg) | ![depois](img/lookdev/depois-frente.jpg) |
| 80 km/h | ![antes](img/lookdev/antes-80kmh.jpg) | ![depois](img/lookdev/depois-80kmh.jpg) |

## O que a cena tem

`/Game/Detonauto/Maps/L_Lookdev`, feita inteira por `Tools/Unreal/create_lookdev.py` (rodar com `create_lookdev.ps1`, editor fechado).
Rodar de novo refaz tudo nos mesmos pacotes.

- **Luz:** Sky Atmosphere; sol de fim de tarde a 15 graus do horizonte, no fim da rua e 2 graus para a direita, 100 000 lux antes da
  atmosfera (a atmosfera esquenta a cor e atenua); nuvens volumétricas (material de nuvem da engine); Sky Light em tempo real; neblina
  de altura leve.
- **Câmera:** exposição de câmera física no pós-processo: f/8, 1/180 s, ISO 100 (EV100 13,5). Com EV 13 a frente do carro estourava; com
  14 as sombras fechavam.
- **Rua:** 1,2 km ao longo de X; duas faixas de 3,5 m; faixa central amarela tracejada (3 m pintados, 6 m de intervalo); linhas de bordo
  brancas; meio-fio de 15 cm; calçadas de 3 m.
- **Prédios:** 112 blocos simples (4 a 12 andares, semente fixa), com vitrine no térreo e uma faixa de vidro por andar, quatro tons de
  reboco. **Os prédios do City Sample não estão neste PC:** os blocos ficam no lugar deles até a migração.
- **Texturas:** quatro conjuntos CC0 do Poly Haven em 2K (asfalto, calçada, concreto do meio-fio e reboco), projetados pelas coordenadas
  do mundo com o tamanho real de cada um (linhas em `licencas.csv`).
- **Largada:** PlayerStart com a tag `DNMapStart`, na faixa da direita, de frente para o sol. O `ADNVehicleTestGameMode` usa esse ponto e
  não cria a arena de teste. O `/Engine/Maps/Entry` também tem PlayerStart, por isso a tag.

## Como usar

```powershell
# refazer a cena (editor fechado)
.\Tools\Unreal\create_lookdev.ps1 -Uproject Detonauto.uproject
# jogar na cena, no perfil alvo PS5
.\Tools\Unreal\vehicle-probe.ps1 -Uproject Detonauto.uproject -Map /Game/Detonauto/Maps/L_Lookdev -AlvoPS5 -Play
# capturas nos ângulos fixos (1920x1080); para a de 80 km/h: -Keys "W@1-8;SHOT@3.4;QUIT@4.5"
.\Tools\Unreal\vehicle-probe.ps1 -Uproject Detonauto.uproject -Map /Game/Detonauto/Maps/L_Lookdev -ResX 1920 -ResY 1080 -AlvoPS5 -Keys "CAM135@4;SHOT@6;CAM90@6.5;SHOT@8.5;CAM180@9;SHOT@11;QUIT@12"
# medir (feche os outros Unreal e não mexa no PC enquanto mede)
.\Tools\Unreal\lookdev-measure.ps1 -Uproject Detonauto.uproject
```

Depois de mudar material ou configuração, a primeira abertura compila shaders por alguns minutos. As capturas e as medições só valem
depois disso.

## O que ficou de fora (anotado)

1. **Desfoque de movimento "só de câmera".** O pós-processo da cena deixa o desfoque desligado e a câmera do carro também (rodas nítidas,
   lote 14). O desfoque por pixel da engine mistura o movimento da câmera com o dos objetos; "só câmera" sem borrar as rodas precisa de
   estudo próprio. Fica para o passo 6 do plano visual (câmera de velocidade).
2. **Normal das faces verticais.** A projeção pelo mundo usa a base de tangente da malha, não a da projeção. Nos blocos isso não aparece;
   para arte final, as malhas terão UV próprio.
3. **Pintura do carro.** O carro ainda usa o material simples do template (laranja chapado). É o passo 4 do plano visual (pintura com
   verniz; `r.ClearCoatNormal` já está ligado).
4. **Prédios do City Sample, postes, árvores, sinalização.** Não há assets disso no PC; a cena é de blocos.

## Lote 16: carro e rua

Os mesmos 4 ângulos, perfil `DN_AlvoPS5`. "Antes" é o "depois" do lote 15.

| Ângulo | Antes (lote 15) | Depois (lote 16) |
|---|---|---|
| Três-quartos | ![antes](img/lookdev/depois-tres-quartos.jpg) | ![depois](img/lote16/depois-tres-quartos.jpg) |
| Lateral | ![antes](img/lookdev/depois-lateral.jpg) | ![depois](img/lote16/depois-lateral.jpg) |
| Frente | ![antes](img/lookdev/depois-frente.jpg) | ![depois](img/lote16/depois-frente.jpg) |
| 80 km/h | ![antes](img/lookdev/depois-80kmh.jpg) | ![depois](img/lote16/depois-80kmh.jpg) |

Rodas: antes ![antes](img/lote16/rodas-antes.jpg) e depois ![esquerda](img/lote16/rodas-depois-esquerda.jpg) ![direita](img/lote16/rodas-depois-direita.jpg).

- **Rodas de lado (bug do lote 14):** os ossos das rodas do SportsCar estão girados ±90 graus e a conta antiga fazia a malha herdar o
  osso. Agora a malha fica com o eixo do cubo no eixo lateral do carro (`Core/DNWheelVisual.h`, teste R2 visto falhar com a conta antiga).
- **Pintura:** `M_DNDentParent` virou a pintura automotiva: verniz (clear coat) liso por cima, base metálica com flocos por baixo (normal
  gerada pelo script), cor vermelho escuro metálico, e o amassado de antes. Feito por `Tools/Unreal/create_dent_material.ps1`.
- **Vidros:** o carro do template **não tem malha de vidro de janela** (o `SM_SportsCar_Glass` são os faróis); as janelas são o slot do
  chassi. O amassado pintava os dois slots com o laranja do protótipo; agora cada slot tem o seu material (pintura; chassi e janelas
  escuros e lisos, com reflexo) e os dois deformam juntos. Vidro translúcido de verdade pede um carro com vidro e interior (City Sample).
- **Pneus e rodas:** borracha escura e fosca, aro metálico com reflexo, nas malhas da roda.
- **Prédios:** janelas com salas falsas (interior mapping procedural, metade acesas), caixilhos, vitrine no térreo, fachada de concreto,
  pé-direito de 3 ou 3,5 m e 12 prédios baixos e recuados em 4 trechos.
- **Rua:** 282 objetos CC0 do Poly Haven: postes, lanternas presas nas fachadas, bancos, lixeiras, hidrantes e jacarandás nos recuos.
  O acervo do estúdio e o doc "Modelos 3D" não tinham mobiliário de rua; os modelos foram baixados com autorização do Felipe.
- **Custo** (`lookdev-measure.ps1`, sem outro Unreal aberto): parado 4,55 / 5,07 ms, andando 4,72 / 5,30 ms (Frame média / p95; GPU 4,3 e
  4,4 ms). Igual ao lote 15, dentro do ruído.

Anotado: os jacarandás aparecem com pouca folha (folhas mascaradas em 1K; pode ser o limiar da máscara ou o modelo); a placa sem marca
não foi feita; para a cidade final, os prédios e o mobiliário do City Sample (precisa ser baixado pelo Felipe no Fab) combinam entre si
e com o carro final, o que nenhuma mistura de fontes garante.
