# Crash no cruzamento (lote 20)

Contrato do segundo modo da fatia jogável (`docs/fatia-jogavel.md`). Substitui a
descrição curta do lote 20 em `docs/fatia-vertical.md`. O jogador entra num cruzamento
cheio de tráfego a uns 120 km/h e causa o maior estrago possível. Reaproveita o carro,
o dano, as peças, os efeitos, a câmera de batida, a rodada de Crash e o tráfego em
trilho que já existem. O que falta é o **mapa**, o **tráfego de verdade** e a
**calibração**.

## Decisões que fecham as pendências

1. **O modo do tráfego do Crash é o `Rail` com acordar E cascata** (revisado em 17 out depois da
   prova em `docs/prova-crash-rail-physics.md`, que refutou a hipótese da velocidade). A diferença
   de 5 vezes não vem do Δv por carro (no `Rail` ele é até maior: 9,7 contra 3,8 m/s), vem da
   **quantidade de carros atingidos e da duração da rodada**: no `Physics` 19 a 29 carros
   rendem pontos numa cascata de 25 a 30 s; no `Rail` só 2 a 6, e a rodada acaba em 6 a 12 s por
   silêncio. Com o `Rail` de hoje o jogador não passa de bronze. Ajustes, nesta ordem:
   - **densidade:** os 12 a 20 carros no trecho do cruzamento, não 40 espalhados;
   - **carro acordado continua andando na faixa** em vez de se afastar, para a cascata se manter;
   - **silêncio do fim de rodada** tolera a cascata (só vale depois que nada mais está batendo
     em nada por `QuietSecondsToEnd`, contando carros acordados em movimento).
   O `Physics` de todos continua só ferramenta de custo.
2. **Medalhas só depois de medir de novo** em 80, 100, 120 e 140 km/h com os ajustes acima
   (script `Tools/Unreal/crash-prova-lote20.ps1`). Os 3.000/8.000/15.000 abaixo são provisórios.
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
