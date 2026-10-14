# Regras do tráfego simplificado

Contrato para a parte pura (`Source/Detonauto/Core/DNTraffic.h/.cpp`, sem UObject,
sem mundo, só `CoreMinimal`). Motivo: a medição mostrou ~0,13 a 0,19 ms de Game
thread por carro com física completa; 40 carros em movimento passam do orçamento
de 8 ms. Hipótese a validar no protótipo: o carro de tráfego anda "nos trilhos"
(sem física) e só vira físico quando algo o atinge ou chega perto.

## Níveis de simulação

| Nível | O que é | Custo |
|---|---|---|
| `Rail` | Anda em trilha com velocidade constante, sem física | baixo |
| `Physics` | Física completa (Chaos), sem volta para `Rail` | alto |
| `Despawn` | Devolvido ao pool | nenhum |

Transições (função `FDNTraffic::NextLevel`, pura):

1. `Rail -> Physics` quando ocorre **qualquer** um:
   - impacto registrado no veículo (força de impacto > 0, qualquer origem);
   - veículo `Physics` (jogador, rival ou outro tráfego) a **menos de 6,0 m**
     de distância (inclusive) entre centros;
   - jogador a menos de **25 m** na frente do veículo, com velocidade relativa
     de aproximação >= 20 km/h (acorda antes, para a física estar quente na batida).
2. `Physics` nunca volta a `Rail`.
3. `Rail` ou `Physics -> Despawn` quando a distância ao jogador é **maior que
   200 m** e o veículo está atrás do jogador (projeção no eixo de avanço < 0).
   Veículo `Physics` ainda em movimento (> 2,0 m/s) **não** some enquanto a
   rodada de Crash estiver ativa.

## Andar na trilha

`FDNTraffic::AdvanceOnRail(Distancia, Velocidade, Dt, ComprimentoTrilha, bLoop)`:
- avança `Distancia += Velocidade * Dt`; Dt <= 0 ou Velocidade < 0 não move;
- sem loop, satura em `ComprimentoTrilha` e informa `bChegouAoFim`;
- com loop, usa módulo (resultado em [0, Comprimento));
- `ComprimentoTrilha <= 0` ou valores não finitos: não move e informa erro.

## Densidade e pool

`FDNTraffic::PlanSpawns(Alvo, Ativos, MaxSpawnPorQuadro)`: quantos veículos
criar neste quadro = `clamp(Alvo - Ativos, 0, MaxSpawnPorQuadro)`. Alvo e
`MaxSpawnPorQuadro` (padrão 2) ficam em dados. Reaproveitar do pool antes de
criar novo; o pool tem tamanho máximo configurável.

## Dados

`Data/TrafficVehicles.csv` já existe. Parâmetros novos (raios, limites) vão
em `Data/TrafficSettings.csv` (uma linha, colunas com os nomes acima em
inglês: `WakeDistanceM`, `WakeAheadM`, `WakeAheadMinClosingKmh`,
`DespawnDistanceM`, `StillSpeedMs`, `MaxSpawnPerFrame`), com validação
(todos > 0, finitos, `WakeDistanceM < WakeAheadM < DespawnDistanceM`).

## Fora de escopo deste lote

Atores, spline real, IA de faixas, mapa. Só regras puras, dados e testes.
Quem mede se o modelo rende de verdade é o mapa de teste (6 carros + 40
tráfego) depois, no PC do Felipe.

## Decisões do lote 4 (14 out 2026)

Pontos que o contrato deixava abertos, fechados na implementação. Cada um tem teste
em `Core/Tests/DNTraffic.spec.cpp`; trocar a decisão é trocar o teste.

**Níveis (`NextLevel`)**

| Ponto | Decisão |
|---|---|
| Q1. "Jogador a menos de 25 m na frente" | Distância em linha reta entre centros **menor que** 25 m (estrito) **e** projeção de (jogador menos veículo) no eixo de avanço do veículo **maior que** 0 |
| Q2. Velocidade de aproximação | `NextLevel` recebe a velocidade de aproximação **pronta**, em km/h (positiva = se aproximando, calculada por quem chama). `>= 20` é inclusivo. Ele não calcula velocidade relativa de vetores |
| Q3. Ordem | **Primeiro acorda** (Rail para Physics), **depois** avalia o despawn sobre o nível resultante. Assim um Rail atingido longe, com a rodada de Crash ativa e em movimento, vira Physics e fica; sem rodada ativa, some |
| Q4. Proteção na rodada de Crash | Só **Physics** com velocidade **maior que** 2,0 m/s (estrito; exatamente 2,0 pode sumir) fica protegido. **Rail nunca é protegido.** A velocidade é a escalar atual do veículo; vale o módulo, e NaN conta como em movimento |
| Q5. Bordas | Veículo Physics a **<= 6,0 m** (inclusivo); jogador a **< 25 m**; aproximação **>= 20 km/h**; despawn a **> 200 m**; projeção do veículo no eixo do jogador **< 0** para "atrás"; projeção do jogador no eixo do veículo **> 0** para "na frente" |
| Entradas NaN | Em qualquer comparação NaN vale "não": não acorda e não some. A única exceção é a velocidade NaN, que conta como em movimento (na dúvida, o veículo não some durante a rodada) |
| Despawn e Physics | `Despawn` é final. `Physics` nunca volta a `Rail` |

**Andar na trilha (`AdvanceOnRail`)**

| Ponto | Decisão |
|---|---|
| Q6a. `bChegouAoFim` | Só quando **houve avanço** (Dt > 0 e velocidade > 0) e a distância resultante é **>=** o comprimento, inclusive chegar exatamente nele e já estar no fim e tentar andar. Nos casos "não move" é falso. Depois do arredondamento para float, igualar o comprimento também conta como fim |
| Q6b. Distância inicial | **Sem loop:** fora de `[0, Comprimento]` é **erro** (`bErro`, não move, devolve a distância recebida). **Com loop:** aceita qualquer distância finita, inclusive negativa, e normaliza por módulo. O resultado fica **sempre em `[0, Comprimento)`**, mesmo no arredondamento (um negativo minúsculo não devolve o comprimento). Com loop `bChegouAoFim` é sempre falso |
| Q6c. Velocidade zero | Move zero, sem erro (não é "negativa") |
| Erros | Valores não finitos (distância, velocidade, Dt ou comprimento) ou `ComprimentoTrilha <= 0`: não move, informa erro e devolve a distância recebida |

**Densidade e dados**

| Ponto | Decisão |
|---|---|
| `PlanSpawns` | `clamp(Alvo - Ativos, 0, MaxSpawnPorQuadro)` calculado em **int64**, sem estourar. Alvo, ativos ou máximo negativos valem 0 |
| Q7. `Data/TrafficSettings.csv` | **Exatamente uma linha** (id `padrao`); zero ou mais de uma é erro de validação. `MaxSpawnPerFrame` é inteiro maior que zero. Os outros cinco são decimais finitos maiores que zero, com `WakeDistanceM < WakeAheadM < DespawnDistanceM` (estrito). Valores iniciais: 6,0; 25,0; 20,0; 200,0; 2,0; 2 |
| Conversão | `FDNTrafficSettingsRow::ToSettings()` entrega os mesmos números no tipo puro `FDNTrafficSettings`, que é o que as regras de `Core/` recebem |

## Lote 13b: correções da revisão independente (14 out 2026)

A revisão do código do lote 13 achou cinco defeitos no tráfego. Decisões do par (sessão da nuvem) e o que mudou. Testes em
`Core/Tests/DNTraffic.spec.cpp` (S1 a S6) e `Core/Tests/DNTrafficWorld.spec.cpp` (G1 a G3, "Sem jogador").

| Defeito | Causa | Correção |
|---|---|---|
| Carros em trilho atravessavam uns aos outros | Corpos cinemáticos não colidem entre si, e cada carro tinha uma velocidade sorteada | Cada carro em trilho segue o da frente na mesma faixa (`FDNTrafficGeometry::LeaderAhead` + `FDNTraffic::FollowSpeed`): folga mínima de 8 m entre para-choques, volta à própria velocidade a partir de 20 m, para atrás de um carro parado. O jogador também conta como carro da frente. No fim da trilha os de trás param em fila, com folga |
| Ao acordar, o carro virava uma caixa que freava até parar em ~12 m | Só a velocidade era copiada; o atrito do cubo com o piso (média de 0,7 e 1,0) freava a 0,85 g | Enquanto "dirige" (até a primeira batida com dano, `AddedZoneDamage > 0` ou destruído), o carro acordado usa um material físico de atrito `RollingFriction` (0,02) combinado pelo **mínimo** com o piso; depois da batida, volta ao atrito normal |
| Ônibus e caminhão não acordavam antes de uma batida de traseira | A distância entre centros (7,8 m numa traseira de ônibus) passava dos 6 m | Acordar mede a **folga entre as caixas** (`FDNTrafficGeometry::NearestGapM`): na linha entre os centros, a distância menos o quanto cada caixa avança nessa direção. `WakeDistanceM` continua 6 m, agora de folga |
| Sem jogador, o tráfego sumia e as vagas se perdiam | Sem jogador, a posição dele virava 1e9 e todo carro ficava "atrás e longe" | `FDNTrafficGeometryInput::bHasPlayer`: sem jogador a distância é NaN e nada acorda nem some por causa dele (impacto e vizinho físico ainda acordam) |
| A rodada nunca silenciava | (1) o fim da rodada olhava todos os carros Physics, inclusive os que só andam; (2) na medição "todos Physics", a velocidade imposta a cada quadro vencia o contato e o carro empurrava o destroço como um trator | O silêncio olha só os **destroços** (`MaxWreckSpeedMs`: Physics que já levou batida com dano) e o jogador; o "manter velocidade" da medição acaba no primeiro contato que não é o piso. A rodada começa só na primeira batida **no jogador** (antes, um contato tráfego com tráfego ligava o relógio) |

Medido (`crash-probe.ps1 -Kmh 130 -Traffic 40`): Rail: menor folga em trilho 8,0 m; primeiro impacto com o jogador a 2,3 s; a rodada
silencia em 12,3 s; 19 carros acordaram e 12 seguiam rolando a 12,5 m/s no fim. Physics (só medição): a rodada silencia em 24,5 s (antes
batia no teto de 30 s); os destroços ainda se mexem até ~20 s porque os carros sem motorista continuam chegando à pilha.

Dados novos em `Data/TrafficSettings.csv`: `FollowMinGapM` (8,0), `FollowCatchUpGapM` (20,0) e `RollingFriction` (0,02). Validação:
maiores que zero e finitos, `FollowMinGapM < FollowCatchUpGapM`, `RollingFriction <= 1`.

Fica para depois: os carros acordados que dirigem não seguem o da frente (só o atrito baixo); o espaçamento no nascimento continua o de
`PlanSlots` (30 m entre centros, 19 m ou mais entre para-choques mesmo com ônibus); a cadeia de batidas da medição "todos Physics" não
tem motorista.
