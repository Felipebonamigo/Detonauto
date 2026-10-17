# Prova da decisão 2 do lote 20: por que o Crash dá 5 vezes mais pontos com o tráfego em Physics

17 out 2026. Contrato: `docs/regras-crash-cruzamento.md`, decisão 2. A hipótese do `estado-atual.md` dizia que a diferença de pontos entre o tráfego em `Rail`
e em `Physics` vinha da velocidade de aproximação (o `Physics` mantém ~29,7 m/s em todos os carros; o `Rail` acordado anda a ~7,7 m/s, porque segue o carro da frente),
que daria um Δv e um dano maiores por carro. **A medição não confirma isso.**

## Como foi medido

Cenário `C-130-40` (o jogador entra a 130 km/h contra 40 carros de tráfego), com as sementes 12345, 777 e 4242, em `Rail` e em `Physics`, no
`DNCrashTestGameMode` (a mesma estrada, as mesmas regras de dano e de pontos). Cada carro de tráfego registra no log a velocidade dele antes da batida, o Δv e, no fim da rodada,
os pontos e o dano total que rendeu (`DNTraffic: batida em ...` e `DNCrashCarro: ...`). O script é `Saved/Prova-lote20` (fora do Git); os números abaixo saem dos logs.

## Resultado

| | Rail (3 sementes) | Physics (3 sementes) |
|---|---|---|
| Pontos da rodada | 285, 1 383, 714 | 20 887, 3 836, 5 378 |
| Carros que renderam pontos | 2, 6, 2 | 29, 19, 25 |
| Pontos por carro (média da rodada) | 142, 231, 357 | 720, 202, 215 |
| Dano total médio por carro | 0,04 a 0,13 | 0,06 |
| Duração da rodada | 8,1 s, 12,3 s, 5,7 s | 30,0 s, 24,7 s, 24,6 s |
| Eventos de impacto | 3 a 273 | 22 795 a 59 307 |
| Velocidade do carro antes da 1ª batida (média dos que bateram) | 8,3 m/s | 13,5 m/s |
| Δv do carro na 1ª batida (média; máximo) | 9,7 m/s; 33,9 | 3,8 m/s; 32,5 |

(13 carros no total com batida registrada em Rail e 77 em Physics.)

## O que isso diz

- **Não é o Δv por carro.** O Δv médio na primeira batida é **maior** no `Rail` (9,7 m/s) do que no `Physics` (3,8 m/s). Os pontos por carro são parecidos (142 a 357 contra 202 a 720).
- **É a quantidade de carros atingidos e o tempo de rodada.** No `Physics` o jogador acerta 19 a 29 carros em 25 a 30 s; no `Rail`, 2 a 6, e a rodada acaba em 6 a 12 s por silêncio. No `Physics`
  os carros mantêm a velocidade e se empilham na frente do jogador (dezenas de milhares de eventos de impacto): é uma **reação em cadeia contínua**. No `Rail` os carros acordados se afastam
  ou nem chegam a ser acordados (3 a 13 acordaram de 40), e a rodada vira duas ou três batidas e um silêncio.
- A velocidade do carro antes da batida no `Physics` medida aqui (13,5 m/s) não é a de 29,7 m/s do relatório antigo: aquele valor era a velocidade imposta, e quem bate já a perdeu.

## Consequências para o contrato do lote 20

1. **A decisão 1 ("o tráfego do Crash é Rail com acordar") não se sustenta só nesta prova.** Com o `Rail` do jeito de hoje o jogador não passa de bronze (285 a 1 383 contra os limites de 3 000, 8 000 e 15 000
   do contrato) e a rodada acaba antes de começar. O que dá graça ao Crash é a cascata, e ela só existe quando os carros se amontoam na frente do jogador.
2. **O que ajustar não é o Δv, é a densidade e a cascata:** mais carros no mesmo trecho (os 12 a 20 do contrato), carros acordados que continuam andando na faixa (não se afastam), e
   a regra de fim de rodada (silêncio de 2 s) tolerar a cascata. Calibrar as medalhas depois de fixar isso.
3. **A hipótese do estado-atual fica refutada** (nota nesse documento): a diferença de 5 vezes vem da quantidade de carros atingidos, não do dano por carro.
4. Antes de fixar as medalhas: repetir esta medição em 80, 100, 120 e 140 km/h com o tráfego já ajustado.
