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

- O turbo liga quando o jogador segura o botão e a barra tem mais de 0.
- Consome 25 unidades por segundo. A barra cheia dura 4 s.
- Desliga ao soltar o botão ou quando a barra chega a 0.

### Turbo prolongado

Se o jogador liga o turbo com a barra em 100 e segura **sem soltar** até a
barra chegar a 0, ele ganha 50 unidades, uma vez por uso. Soltar o botão em
qualquer momento cancela o prêmio.

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
Limites devem ser estritamente crescentes. Uma configuração que não respeite
isso é inválida e o teste de dados a recusa.

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

Ordem de avaliação, a primeira que valer define o tipo:

1. **Vertical:** o jogador está pelo menos 100 cm acima do rival e caindo.
2. **Por trás:** o ângulo entre `A` e `V` é menor que 30 graus e o jogador está
   atrás do rival.
3. **Em T:** o ângulo entre `A` e `V` está entre 60 e 120 graus e o contato
   está no lado do rival.
4. **Lateral:** o contato está no lado do rival e o ângulo entre `A` e `V` é
   menor que 60 graus.
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
