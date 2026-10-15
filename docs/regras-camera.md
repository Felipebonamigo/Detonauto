# Câmera: velocidade e batida

Contrato do lote 18. Resumo em `docs/fatia-vertical.md`. O que mais vende um
jogo de batida é a câmera: ela tem que **sentir** a velocidade e **mostrar** a
batida.

Os cinco modos de câmera já existem (`Data/Cameras.csv`). Este lote acrescenta
três coisas por cima deles:

1. a sensação de velocidade, com campo de visão, tremor e desfoque nas bordas;
2. a câmera de batida, com câmera lenta;
3. o controle do destroço durante a câmera lenta.

## Dados

Valores globais em `Data/CameraFx.csv`, uma linha de configuração como em
`TrafficSettings.csv`. O `TurboFovKickDegrees`, hoje uma constante em
`DNVehicle.h`, passa para lá como `TurboFovAddDeg` (regra 2 do `CLAUDE.md`).

| Valor | Padrão | O que é |
|---|---|---|
| `SpeedFovStartKmh` | 60 | abaixo disso o campo de visão é o do modo |
| `SpeedFovFullKmh` | 200 | a partir daqui soma o máximo |
| `SpeedFovMaxAddDeg` | 8 | quanto a velocidade abre o campo de visão |
| `TurboFovAddDeg` | 12 | o que o turbo soma, como hoje |
| `MaxFovDeg` | 115 | teto do total, para não virar olho de peixe |
| `FovBlendDegPerS` | 30 | quanto o campo de visão muda por segundo, no máximo |
| `ShakeStartKmh` | 150 | início do tremor |
| `ShakeFullKmh` | 250 | tremor máximo |
| `ShakeMaxDeg` | 0,35 | amplitude máxima, em graus de giro da câmera |
| `ShakeHz` | 9 | frequência do ruído do tremor |
| `EdgeBlurStartKmh` | 100 | início do desfoque nas bordas |
| `EdgeBlurFullKmh` | 250 | desfoque máximo |
| `EdgeBlurMax` | 0,5 | força máxima (0 a 1) |
| `EdgeBlurInnerRadius` | 0,4 | raio sem desfoque, em fração da meia diagonal da tela |
| `CrashCamMinDeltaVMs` | 15 | variação de velocidade que dispara a câmera de batida |
| `CrashCamTimeScale` | 0,25 | escala de tempo na câmera lenta |
| `CrashCamEnterS` | 0,15 | tempo para descer a escala até 0,25 |
| `CrashCamHoldS` | 1,5 | tempo segurando |
| `CrashCamExitS` | 0,4 | tempo para voltar a 1 |
| `CrashCamCooldownS` | 2,0 | espera depois de terminar, antes de poder disparar de novo |
| `CrashCamOrbitRadiusM` | 7 | distância da câmera ao carro |
| `CrashCamOrbitHeightM` | 2,5 | altura da câmera |
| `CrashCamOrbitDegPerS` | 25 | velocidade da órbita |
| `CrashCamProbeCm` | 20 | raio da esfera que impede a câmera de entrar em parede |
| `CrashCamLeadDeg` | 90 | a órbita começa tantos graus antes do lado da batida (entrou no fim do lote 18, aprovado por captura) |
| `WreckSteerAccelMs2` | 6 | aceleração lateral do controle do destroço |
| `WreckSteerMaxDeltaVMs` | 6 | quanto o controle pode mudar a velocidade, no total, por batida |

`Cameras.csv` ganha duas colunas por modo: `SpeedFovScale` e `ShakeScale`.

| Modo | `SpeedFovScale` | `ShakeScale` |
|---|---|---|
| alta-longe | 1,0 | 1,0 |
| alta-muito-longe | 1,0 | 0,7 |
| capo | 1,0 | 1,2 |
| frente-para-choque | 1,0 | 1,2 |
| motorista | 0,5 | 0,8 |

Dentro do carro, o campo de visão abrindo demais enjoa; por isso o motorista
tem `SpeedFovScale` menor.

**Todo tempo deste contrato é tempo real**, sem a escala de tempo. Use o delta
não dilatado. Senão a câmera lenta prolonga a si mesma.

## Regras puras (Core, com teste)

### `FDNSpeedFov`

O campo de visão é `Base + SpeedFovMaxAddDeg · S(v) · SpeedFovScale +
TurboFovAddDeg · TurboBlend`, limitado a `MaxFovDeg`.

- `S(v)` é uma `smoothstep` de 0 a 1 entre `SpeedFovStartKmh` e `SpeedFovFullKmh`.
- O valor mostrado anda até o alvo a no máximo `FovBlendDegPerS`. Assim a
  freada da batida não dá um tranco na lente.
- Velocidade NaN ou negativa conta como 0.

### `FDNSpeedShake`

Giro de arfagem e guinada da câmera a partir de um ruído suave e determinístico
(Perlin 1D com semente fixa, eixos defasados), em `ShakeHz`.

- A amplitude é `ShakeMaxDeg · S(v) · ShakeScale`, com `S` entre
  `ShakeStartKmh` e `ShakeFullKmh`.
- Abaixo do início, o tremor é exatamente zero.
- Fica pronto para o menu ter uma opção de desligar (acessibilidade): um
  multiplicador global de 0 ou 1, guardado no save quando o menu existir.

### `FDNCrashCam`

Máquina de estados: `Idle → Entering → Holding → Exiting → Cooldown → Idle`.

- **Dispara** com batida real do jogador de variação de velocidade maior ou
  igual a `CrashCamMinDeltaVMs`, só se a fonte contar
  (`FDNImpactPolicy::TriggersCrashCam`, então mobiliário nunca dispara).
  Disparo durante a câmera lenta ou na espera é ignorado.
- **Escala de tempo:**
  - `Entering`: desce de 1 a `CrashCamTimeScale` em `CrashCamEnterS`, com
    suavização na saída.
  - `Holding`: segura por `CrashCamHoldS`.
  - `Exiting`: volta a 1 em `CrashCamExitS`.
- Também expõe o peso de mistura da câmera de batida (0 a 1): sobe no
  `Entering` e desce no `Exiting`.
- O teste confere os valores nos instantes de borda e no meio de cada fase, o
  limiar exato e a espera.
- **Escolhas da implementação (aprovadas na revisão):**
  - `Entering` desce com `1 − (1 − u)²` (saída suave): no meio da fase, escala
    0,4375 e peso 0,75;
  - `Exiting` volta com `smoothstep`: no meio, escala 0,625 e peso 0,5;
  - a sobra de um passo de tempo passa para a fase seguinte (um passo grande
    atravessa várias fases).

### `FDNWreckSteer`

Durante a câmera de batida (`Entering` e `Holding`), a entrada esquerda e
direita do jogador vira uma aceleração horizontal de até `WreckSteerAccelMs2`.

- A direção é perpendicular à velocidade horizontal do carro, no sentido da tela.
- Aplique como aceleração, sem depender da massa.
- O total de variação de velocidade por batida é limitado a
  `WreckSteerMaxDeltaVMs`.
- Fora da câmera de batida, nada.
- **Escolha da implementação (aprovada):** a direção é a perpendicular à
  velocidade horizontal que fica do lado direito da tela (o sinal da entrada
  escolhe o lado). Com o carro parado, vale a direita da tela.
- Nome de trabalho: "controle do destroço". Nunca use o nome do Burnout.

## Câmera de batida no motor

- **Começo:** parte da posição da câmera atual (sem corte) e mistura para uma
  órbita em volta do **carro**, não do ponto da batida, porque o carro sai
  girando:
  - raio `CrashCamOrbitRadiusM`;
  - altura `CrashCamOrbitHeightM`;
  - `CrashCamOrbitDegPerS` em tempo real, girando para o lado de onde veio o
    impacto, para mostrar a parte amassada.
- **Parede:** traço de esfera (`CrashCamProbeCm`) do carro até a posição
  desejada; se bater, a câmera vem para antes do contato. Nunca dentro de
  prédio, poste ou destroço.
- **Fim:** volta para o modo de câmera do jogador, misturando em `CrashCamExitS`.
- **Som:** o som com a escala de tempo é do lote 21; aqui só a imagem.
- **Escolhas da implementação (lote 18, medidas em [`lote-18.md`](lote-18.md)):**
  - dispara no contato, sem esperar a janela de 0,15 s do dano, e vale com o dano desligado;
  - a órbita começa `CrashCamLeadDeg` antes do lado da batida, no lado com mais espaço livre até a esfera bater (empate: o mais perto
    da câmera do modo). Começando onde a câmera do modo estava, numa batida de frente ela nunca mostrava o amassado;
  - a mistura é em coordenadas polares em volta do carro (ângulo, distância, altura): a câmera faz um arco e nunca atravessa o carro;
  - a esfera encurta o caminho na hora e devolve 1,5 do caminho por segundo real (sem tranco atrás de poste);
  - o controle do destroço usa a direita da tela da câmera do jogador **no instante da batida**. A da órbita fica ao lado do carro e
    corre ao longo da velocidade, e o lado da perpendicular ficava indefinido.

## Desfoque

O Felipe não quer roda borrada. Hoje o desfoque de movimento está zerado no
`DNVehicle.cpp`. A ordem é:

1. **Desfoque nas bordas** por um material de pós-processamento próprio:
   desfoque radial a partir do centro da tela.
   - Força `EdgeBlurMax · S(v)`.
   - Zero dentro de `EdgeBlurInnerRadius`, onde está o carro, com transição
     suave até a borda.
   - Por não usar o vetor de movimento da engine, nunca borra as rodas.
2. O desfoque por objeto da engine continua desligado. Se a 5.8 tiver uma opção
   por componente que impeça as rodas de escrever velocidade, ela pode ser
   avaliada depois, com captura. Confira o nome exato na engine antes de usar e
   não invente.

## Aprovação

1. **Batida a 100 km/h na parede da `L_Lookdev`, no `-Play`, sol por trás:**
   sequência em tempo real nos instantes 0; 0,15; 0,6; 1,2; 1,65 e 2,05 s, com
   a escala de tempo de cada quadro escrita na captura.
2. **Batida contra o tráfego** a 100 km/h, na mesma sequência.
3. **Mobiliário a 100 km/h:** nenhuma câmera lenta (log).
4. **Câmera perto do prédio:** a mesma batida com o carro encostado no prédio.
   A câmera não entra no prédio.
5. **Velocidade:** quadros parados a 60, 150 e 220 km/h na alta-longe, com e
   sem o desfoque nas bordas. As rodas nítidas a 220 km/h.
6. **Controle do destroço:** a mesma batida com esquerda segurada e com direita
   segurada. Desvio lateral medido em log, dentro do teto.
7. **Custo** no `DN_AlvoPS5`, sobretudo o do material de desfoque, com média e
   p95.
8. **O Felipe joga** e diz se a velocidade e a batida "vendem".
