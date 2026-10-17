# Crash no cruzamento (lote 20)

Contrato do segundo modo da fatia jogável (`docs/fatia-jogavel.md`). Substitui a
descrição curta do lote 20 em `docs/fatia-vertical.md`. O jogador entra num cruzamento
cheio de tráfego a uns 120 km/h e causa o maior estrago possível. Reaproveita o carro,
o dano, as peças, os efeitos, a câmera de batida, a rodada de Crash e o tráfego em
trilho que já existem. O que falta é o **mapa**, o **tráfego de verdade** e a
**calibração**.

## Decisões que fecham as pendências

1. **O modo do tráfego do Crash é o `Rail` com acordar.** O `Physics` de todos vira só
   ferramenta de medição de custo. Motivo (hipótese do estado-atual, a provar no
   primeiro passo): o `Physics` mantém 29,7 m/s em todos os carros até o primeiro
   contato; o `Rail` acordado anda a 7,7 m/s porque segue o carro da frente. O
   tráfego real de cruzamento anda devagar e para no semáforo.
2. **Prova antes de fixar as medalhas:** log por carro com o Δv do primeiro impacto e
   os pontos, por modo, no `C-130-40`. Se a diferença de 5 vezes vier só da
   velocidade de aproximação, está explicado e o `Rail` vale. Se vier de outro
   lugar, corrigir antes de seguir.
3. **130 km/h rende menos que 100** é um defeito de design arcade. O carro é
   desviado no primeiro impacto. Duas alavancas, testar nesta ordem:
   - o jogador pesa mais que o tráfego nas batidas do Crash (razão de massa em
     `Data/Crash.csv`, valor inicial: o carro do jogador conta como 1,3× na conta do
     Δv do tráfego, não a massa física);
   - o controle do destroço (lote 18) já ajuda a mirar depois do primeiro impacto.
   Meta: a pontuação média cresce com a velocidade de entrada, de 80 a 140 km/h.
4. **Repetibilidade:** passo fixo da física (`-UseFixedTimeStep`) no modo Crash
   para a medalha ser justa. Medir o custo e o efeito na direção.

## O mapa `L_Cruzamento01`

- Montado por script, como o percurso: ruas a partir de linhas centrais em
  `Data/CoursePoints.csv` (reaproveita `ADNCourseActor`). Duas avenidas de 2 faixas
  por sentido se cruzando em ângulo reto, com 120 m de aproximação em cada lado, a
  largada a 150 m do cruzamento.
- **Fachada** com o mesmo kit do percurso 1, mas **com cantos** (as quinas do kit CH)
  nos quatro lados do cruzamento, para não parecer corredor.
- **Semáforos** de ciclo fixo (12 s verde, 3 s amarelo, 15 s vermelho para a outra
  avenida), de peça de rua que quebra. O trilho do tráfego **para na linha de
  retenção** no vermelho.
- **Mobiliário** destrutível a cada ~22 m, como no percurso, mais o que já temos
  (poste, árvore, banco, lixeira, hidrante).
- **Hora do dia:** sol baixo vindo do lado, para o carro do jogador não virar
  silhueta na aproximação.

## Tráfego

- 8 modelos de tráfego, `Data/TrafficVehicles.csv`. Até o download do City Sample
  Vehicles, o carro "caixa" continua; **não aprovar a aparência do tráfego** nesse caso.
- Densidade e velocidade por avenida em `Data/Crossings.csv`: 12 a 20 carros ao mesmo
  tempo no cruzamento, velocidades de 30 a 50 km/h, com um ônibus a cada 6 carros.
- Orçamento: o do lote 13 (`desempenho.md`): 0,02 ms de Game por carro em trilho;
  meta de 6 carros destruídos e 20 acordados no `DN_AlvoPS5`, p95 abaixo de 16,6 ms.

## Rodada, pontos e medalhas

- Rodada, multiplicador e cadeia: como em `regras-de-jogo.md` e `Crash.csv`. A
  pontuação continua por dano (valor × aumento de dano).
- **Medalhas:** `Crossings.csv` ganha as linhas `cruzamento-01` com limites iniciais de
  bronze 3.000, prata 8.000 e ouro 15.000. Calibrar com **10 rodadas por velocidade
  de entrada** (80, 100, 120 e 140 km/h) e com a skill `statistical-analysis` para
  dizer quantas rodadas bastam. Ouro tem que ser possível, mas exigir mirar.
- **Tela de resultado** do Crash: pontos, multiplicador máximo, maior cadeia,
  medalha, recorde. Mesmo estilo da tela de resultado da corrida (fonte nítida).
- **Reinício** com R: cruzamento, tráfego, mobiliário e carro como no começo.

## O que precisa existir antes de chamar de pronto

1. Mapa criado por script e jogável do começo ao fim, com a tela de resultado.
2. Capturas, sol por trás e 1920: aproximação, o impacto no meio do cruzamento, a
   câmera de batida e a tela de resultado.
3. O log da prova da decisão 2, e a pontuação por velocidade de entrada (decisão 3).
4. Custo no `DN_AlvoPS5`.
5. O Felipe joga 10 rodadas e responde "dá vontade de mais uma?", "a pontuação
   faz sentido?", "a medalha é justa?".

Divertido e bonito: este modo é a promessa do jogo. Se jogar não dá vontade de "mais
uma" com caixas no lugar do tráfego, o problema é de design (ritmo, pontuação,
cadeia), não de arte; resolver antes do download do City Sample Vehicles.
