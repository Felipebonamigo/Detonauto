# Efeitos de batida (Niagara)

Contrato do lote 19. Resumo em `docs/fatia-vertical.md`. Os efeitos são a camada
4 do dano (`docs/plano-visual.md`). Todo efeito **roda com a escala de tempo do
mundo**, então fica em câmera lenta junto com a câmera de batida do lote 18.
Isso é proposital: é onde o efeito mais aparece.

## Onde ficam os números

Em `Data/Effects.csv`, uma linha por efeito, com validador e teste de deriva.
Os sistemas Niagara expõem **parâmetros de usuário**, e o jogo os preenche a
partir do CSV ao disparar. Ajustar um efeito é mudar o CSV, não o asset.

## Regras puras (Core, com teste)

`FDNEffects` decide **se** e **quanto**. O Niagara só desenha.

| Efeito | Dispara quando | Quantidade / intensidade |
|---|---|---|
| Faísca de impacto | batida com Δv ≥ `SparkMinDeltaVMs` (4) em superfície dura (muro, poste, carro) | `clamp(SparkPerMs · Δv, 10, 120)` partículas, velocidade de 4 a 12 m/s em cone pela tangente do contato, vida de 0,3 a 0,8 s |
| Faísca de raspão | `OnPartScrape` ou carroceria raspando em parede ou chão, com velocidade relativa ≥ 3 m/s | `SparkScrapeRatePerMs · v` por segundo, enquanto durar |
| Cacos de vidro | vidro estoura (`FDNGlass`) | 60 a 150 cubos de 1 a 2 cm e 20 brilhos, vida de 1,5 s. Substitui o `ADNGlassShards` |
| Lascas de plástico | batida com Δv ≥ 12 | 10 a 30 pedaços escuros pequenos |
| Fumaça de pneu | ângulo de deslizamento ≥ `TireSmokeSlipDeg` (8°) com mais de 20 km/h, ou roda patinando | taxa de 0 a 1 proporcional ao deslizamento, por roda, cinza-claro, vida de 2 a 4 s, crescendo |
| Fumaça do motor | dano da frente ≥ 0,6 (clara) e ≥ 0,8 (escura e grossa) | contínua, saindo do capô ou do vão do motor |
| Fogo | dano da frente ≥ 0,9, ou carro destruído com a frente ≥ 0,7 | chamas pequenas no vão do motor, com uma luz pontual sem sombra e tremulante |
| Mobiliário | peça de rua quebra | poste: pó de concreto na base; árvore: 30 a 40 folhas caindo e um pouco de pó; hidrante: jato de água (opcional) |

- **Superfície:** dura (concreto, metal, carro) gera faísca. Mato e terra, no
  futuro, geram pó em vez de faísca.
- **Quantidade:** um dano maior na mesma zona não dispara outra vez a fumaça do
  motor; a intensidade só sobe.
- **Reiniciar** (tecla R e reinício da rodada) apaga tudo: fumaça, fogo e cacos
  no chão.

## Orçamento

O que mais custa é a **fumaça na frente da câmera** (sobreposição de
transparência), não a quantidade de partículas.

- **Pool por tipo**, reaproveitado, com teto de sistemas ativos em
  `Effects.csv`. Valores iniciais:
  - faísca: 8;
  - cacos: 4;
  - fumaça de pneu: 4 carros;
  - motor e fogo: 6 carros.
- **Teto de partículas:**
  - faísca e cacos em GPU, até 2.000 no total;
  - fumaça em CPU, até 400 sprites no total.
- **Fumaça:** com iluminação barata (sem reflexo de Lumen na transparência) e
  partículas grandes e poucas em vez de muitas pequenas.
- **Distância:** a fumaça some suave a menos de 1,5 m da câmera, para não
  encher a tela quando a câmera de batida passa por dentro.
- **Meta no `DN_AlvoPS5`:** 6 carros batendo ao mesmo tempo, com fumaça de pneu
  e de motor, somam no máximo **+1,5 ms de GPU no p95**.

## Assets e licença

- **Ponto de partida:** os modelos de Niagara da própria engine (DirectionalBurst,
  Fountain, UpwardMeshBurst, versões Lightweight) e as texturas de fumaça e de
  faísca do conteúdo gratuito da Epic (Starter Content e Content Examples,
  licença só para Unreal). Tudo com linha em `licencas.csv`.
- **Montagem:** pelo **Unreal MCP**, ligado desde 14 out com o editor aberto, e
  com o passo a passo de cada sistema anotado em `docs/lote-19.md`. O que der
  para fazer por Python (criar a partir do modelo, ligar parâmetros de usuário)
  fica em script versionado.

## Aprovação

Capturas com o sol por trás, em 1920 e sem HUD, na câmera de batida.

1. Batida a 100 km/h no muro: faíscas, lascas de plástico e cacos do farol.
2. Para-choque arrastando a 60 km/h: rastro de faíscas.
3. Vidro lateral estourando: cacos (Niagara no lugar do `ADNGlassShards`).
4. Derrapagem com freio de mão a 80 km/h: fumaça de pneu.
5. Frente em 0,6, em 0,8 e em 0,95: fumaça clara, escura e fogo.
6. Árvore e poste quebrando: folhas e pó.
7. Custo: 6 carros batendo ao mesmo tempo, com fumaça, e a câmera de batida
   dentro da fumaça, no `DN_AlvoPS5` (GPU, média e p95).
8. O Felipe joga e diz se a batida ganhou peso.
