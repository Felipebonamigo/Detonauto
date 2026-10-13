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
