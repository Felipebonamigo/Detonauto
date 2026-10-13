# Regras de jogo do Detonauto (valores iniciais)

Especificação das regras que não dependem de física nem de gráficos: turbo,
pontuação do modo Crash, medalhas e classificação do takedown. Servem de
contrato para o código em `Source/Detonauto/Core/` e para os testes
automáticos.

**Todos os números são valores iniciais.** Foram escolhidos para dar uma
partida testável e vão mudar quando o Felipe jogar. Por isso ficam em uma
struct de configuração, nunca espalhados pelo código, e depois migram para
`Data/*.csv`. Unidades: segundos, unidades de turbo, centímetros e graus,
como no Unreal.

**Nomes.** "Takedown" é nome de trabalho. Nenhum nome da lista proibida no
`CLAUDE.md` aparece no código. A barra extra do turbo chama-se "turbo
prolongado".

## 1. Turbo

A barra vai de 0 a 100 unidades e começa em 0.

### Ganho (barra desligada)

| Fonte | Condição | Ganho |
|---|---|---|
| Contramão | Em faixa contrária e velocidade acima de 80 km/h | 12 por segundo |
| Passada raspando | Distância lateral abaixo de 150 cm de um veículo de tráfego em movimento, velocidade acima de 60 km/h, uma vez por veículo | 6 por evento |
| Derrapada | Ângulo de deslizamento entre 15 e 70 graus, velocidade acima de 60 km/h | 8 por segundo |
| Salto | Depois de 0,3 s no ar | 10 por segundo, no máximo 20 por salto |
| Takedown | Quando acontece | A barra vai a 100 |

**Cadeia de passadas raspando:** passadas dentro de 2,0 s da anterior
multiplicam o ganho do evento por 1,0, 1,25, 1,5, 1,75 e 2,0, nessa ordem.
Passadas separadas por mais de 2,0 s reiniciam a cadeia.

O ganho nunca passa de 100. Com o turbo ligado, só o takedown dá ganho.

### Uso

- **O turbo liga no aperto do botão**, isto é, no quadro em que o botão passa de
  solto para apertado, e só se a barra tiver mais de 0 nesse quadro. Segurar o
  botão com a barra vazia não liga nada, mesmo que a barra comece a encher.
- Consome 25 unidades por segundo. A barra cheia dura 4 s.
- Desliga ao soltar o botão ou quando a barra chega a 0.
- Por consequência, depois de a barra zerar com o botão ainda apertado, o
  turbo só volta a ligar com um novo aperto (soltar e apertar de novo). Isso
  evita um turbo que religa sozinho quando o takedown recarrega a barra ou
  quando os ganhos enchem a barra com o botão segurado.

### Turbo prolongado

Se o jogador liga o turbo com a barra em 100 e segura **sem soltar** até a
barra chegar a 0, ele ganha 50 unidades, uma vez por uso, e o turbo continua
ligado no mesmo uso. Soltar o botão em qualquer momento cancela o prêmio. Um
"uso" vai do aperto que liga o turbo até ele desligar.

## 2. Pontuação do modo Crash

- Cada veículo destruído tem um **valor em dinheiro**, vindo da tabela de
  tráfego.
- **Pontos de um veículo = valor × multiplicador no instante da destruição.**
- **Multiplicador:** começa em 1 e sobe 1 para cada multiplicador coletado no
  chão. Máximo 8.
- **Cadeia:** um veículo destruído dentro de 1,5 s do anterior soma 10% ao
  valor, acumulando por elo, até +100%. Passar de 1,5 s reinicia a cadeia.
- **Fim da rodada:** quando nenhum veículo passa de 2 m/s por 3 s seguidos, ou
  aos 30 s depois do primeiro impacto, o que vier primeiro.
- A pontuação satura em `MAX_int32`, sem dar a volta (regra do `FDNScore`).
- Valores zero ou negativos são ignorados.

## 3. Medalhas

Cada cruzamento define três limites crescentes: bronze, prata e ouro. A medalha
é a mais alta cujo limite a pontuação alcançou. Abaixo do bronze, nenhuma.
Limites devem ser **maiores que zero e estritamente crescentes**. Uma
configuração que não respeite isso é inválida e o teste de dados a recusa.
Alcançar um limite significa pontuação maior ou igual a ele.

## 4. Takedown

Um takedown acontece quando **todas** as condições valem:

1. O jogador toca num rival e a velocidade relativa no contato é de pelo menos
   20 km/h.
2. O rival bate em outro carro, em tráfego ou na parede em até 1,5 s depois do
   contato.
3. O jogador não destruiu a si mesmo no mesmo intervalo.

### Tipo do takedown

Entradas: direção da frente do jogador `A`, direção da frente do rival `V`,
normal do contato, altura relativa e se o jogador está caindo.

**Convenção de contato.** A normal do contato é um vetor unitário no espaço
do mundo que **aponta do jogador para o rival**. Com `V` a frente do rival:

| Zona de contato no rival | Condição |
|---|---|
| Traseira | `dot(normal, V)` maior ou igual a cos 45° (0,7071) |
| Frontal | `dot(normal, V)` menor ou igual a -cos 45° |
| Lateral | qualquer outro valor |

O ângulo entre `A` e `V` é `acos(dot(A, V))`, de 0 a 180 graus, com os dois
vetores normalizados.

Ordem de avaliação, a primeira que valer define o tipo:

1. **Vertical:** o jogador está pelo menos 100 cm acima do rival e caindo.
2. **Por trás:** o ângulo entre `A` e `V` é menor que 30 graus e a zona do
   contato é traseira.
3. **Em T:** o ângulo entre `A` e `V` está entre 60 e 120 graus (inclusive) e a
   zona do contato é lateral.
4. **Lateral:** a zona do contato é lateral e o ângulo entre `A` e `V` é menor
   que 60 graus.
5. **Geral:** qualquer outro caso.

Cada tipo pode ter uma recompensa diferente no futuro. Por enquanto todos
recarregam a barra a 100.

## O que NÃO está especificado aqui

- Dano à carroceria, peças soltas e explosão (depende do teste técnico de
  dano, Sprints 12 e 13).
- Sensação da direção e câmera.
- Inteligência artificial dos rivais.
- O valor de cada veículo, que sai de `Data/TrafficVehicles.csv`.

## Como cada regra é testada

Cada regra vira funções puras em C++, sem Actors nem física, com o tempo e as
entradas passados como parâmetro. Um teste de Automation por regra, rodando
sem janela pelo `Tools/Unreal/test.ps1`. A sensação final é decisão do Felipe
jogando.

## Decisões de implementação (13 out 2026)

Ambiguidades levantadas na implementação e como foram fechadas. Cada uma vira um
comentário no código e um teste que fica fácil de trocar se a decisão mudar.

**Turbo**

| Ponto | Decisão |
|---|---|
| T1. Fontes de ganho no mesmo quadro | Somam |
| T2. "Uma vez por veículo" | Por veículo, enquanto o estado não for reiniciado. O estado guarda os ids já contados e tem um reinício explícito |
| T3. Depois da 5ª passada da cadeia | O multiplicador fica em 2,0 |
| T4. Intervalo de exatamente 2,0 s | Continua a cadeia. Só "mais de 2,0 s" reinicia |
| T5. "Entre 15 e 70 graus" | Inclusivo nos dois extremos. "Acima de" é estritamente maior |
| T6. Religar com o botão apertado | Exige novo aperto. **Revisão:** a regra virou mais simples, e o turbo só liga no aperto (borda de subida do botão), como descrito na seção de uso |
| T7. Turbo prolongado | A barra vai a 50 e o turbo continua no mesmo uso, com um prêmio por uso |
| O1. Ganho e consumo no mesmo quadro | Resolvido pela regra do aperto: ganhar com a barra vazia e o botão segurado não liga o turbo, então o ganho não é gasto no mesmo quadro |
| O2. Sinal do ângulo de deslizamento | Vale o valor absoluto: derrapar para os dois lados ganha |
| O3. Passada que não cumpre as condições | Não conta como elo da cadeia nem marca o veículo como contado |
| O4. Teto de 20 por salto | Conta o ganho possível do salto, mesmo que a barra já estivesse cheia ou o turbo ligado |

**Crash**

| Ponto | Decisão |
|---|---|
| C1. Ordem da conta | Valor com o bônus da cadeia, depois vezes o multiplicador. O 1º veículo da cadeia não tem bônus e o 11º já está em +100% |
| C2. Arredondamento | Aritmética inteira de 64 bits, sem ponto flutuante. O bônus da cadeia é `valor × (100 + 10 × elos) / 100`, truncado para baixo, e só depois multiplicado |
| C3. Fim da rodada | Só conta depois do primeiro impacto. "Parado" é velocidade menor ou igual a 2,0 m/s. 3,0 s e 30,0 s exatos encerram |
| C4. Depois do fim | Pontos e multiplicadores são ignorados |
| C5. Valor menor ou igual a zero | Ignorado: não vira elo da cadeia nem conta como primeiro impacto |

**Medalhas**

| Ponto | Decisão |
|---|---|
| M1. Limites | Estritamente crescentes **e maiores que zero**. Alcançar significa maior ou igual |

**Takedown**

| Ponto | Decisão |
|---|---|
| K1. Zona de contato | Convenção e limites na seção do takedown acima |
| K2. Limites | "Entre 60 e 120", "pelo menos 100 cm" e "pelo menos 20 km/h" são inclusivos |
| K3. Condição 2 e 3 | "Em até 1,5 s" é inclusivo e conta do instante do contato |

**Escolhas da implementação, aceitas em 14 out 2026**

| Ponto | Decisão |
|---|---|
| O5. Barra no quadro do aperto | Vale a barra depois dos ganhos e do takedown daquele quadro |
| D1. Limites de medalha inválidos | `Evaluate` devolve nenhuma medalha, em vez de adivinhar |
| D2. Pontos de uma destruição | `RegisterDestruction` devolve os pontos efetivamente somados. Se saturar, a diferença |
| D3. Velocidade inválida (NaN) no fim da rodada | Conta como em movimento, para não encerrar a rodada por engano |
| D4. Multiplicador fora de 1 a 8 | É limitado ao intervalo |
| E1. Vetor inválido (zero ou NaN) no takedown | A zona é lateral e a classificação é geral. O tipo vertical não depende dos vetores e continua valendo |
| E2. Vetores sem comprimento 1 | São normalizados. Os que já têm comprimento 1 passam intactos, para o limite ficar exato |
| E3. Condição 3 do takedown | O jogador só é desqualificado se se destruir de 0 a 1,5 s depois do contato. Antes, depois ou NaN não desqualificam |
| E4. Altura NaN | Nunca vira vertical |


## Ajuste em dados: Turbo.csv, Crash.csv e Takedown.csv (lote 5)

Os números destas regras moram em `Data/Turbo.csv`, `Data/Crash.csv` e `Data/Takedown.csv` (uma linha cada, id
`padrao`, colunas com os nomes dos campos de `FDNTurboConfig`, `FDNCrashConfig` e `FDNTakedownConfig`). O teste de
deriva garante que o CSV e o padrão da struct pura são iguais. Ainda não são lidos pelo jogo. Regras de validação
(`FDNDataValidation`), todas com valores finitos e exatamente uma linha:

| Regra | Validação |
|---|---|
| V1. Turbo, limites e janelas | `MaxBar`, `NearMissMaxLateralCm`, `NearMissChainWindowSeconds` e `ConsumptionPerSecond` maiores que zero |
| V2. Turbo, velocidades e ganhos | Velocidades mínimas, ganhos, `AirTimeThresholdSeconds`, `AirMaxGainPerJump` e `ExtendedBonus` maiores ou iguais a zero |
| V3. Turbo, derrapada | `0 <= DriftMinAngleDeg < DriftMaxAngleDeg <= 90` (acima de 90 o carro anda de ré, não derrapa) |
| V4. Turbo, takedown | `TakedownSetsBarTo` em `(0, MaxBar]` |
| V5. Turbo, cadeia de quase batidas | `NearMissChainMultipliers` não vazio, cada valor finito e maior que zero. Não exige ordem crescente nem primeiro valor 1,0 |
| V6. Crash, multiplicador e bônus | `MaxMultiplier >= 1`; `ChainBonusPercentPerLink` e `ChainBonusMaxPercent` maiores ou iguais a zero |
| V7. Crash, tempos | `ChainWindowSeconds`, `QuietSecondsToEnd` e `MaxSecondsAfterFirstImpact` maiores que zero; `QuietSpeedMetersPerSecond >= 0` |
| V8. Crash, fim da rodada | `QuietSecondsToEnd <= MaxSecondsAfterFirstImpact` (senão o silêncio nunca encerra a rodada) |
| V9. Takedown, velocidade, janela e altura | `MinRelativeSpeedKmh >= 0`; `RivalCrashWindowSeconds` e `VerticalMinHeightCm` maiores que zero |
| V10. Takedown, ângulos | `BehindMaxAngleDegrees` e `SideMaxAngleDegrees` em `(0, 180]`; `0 <= TBoneMinAngleDegrees < TBoneMaxAngleDegrees <= 180` |
| V11. Takedown, lateral e T | `SideMaxAngleDegrees <= TBoneMinAngleDegrees`: sem sobreposição; um vão entre as duas é permitido |
| V12. Takedown, zonas | `ZoneHalfAngleDegrees` em `(0, 90)`, estrito |
