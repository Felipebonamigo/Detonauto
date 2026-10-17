# Percurso 1: rua de cidade do primeiro jogo (marco 1a)

Desenho do primeiro percurso da corrida ponto a ponto (`docs/fatia-jogavel.md`). Regras puras: `Core/DNCourse.h` (comprimento, progresso, ponto no progresso) e
`Core/DNRace.h` (chegada, ordem, posição).

## O que o City Sample dá e não dá (inspeção de 17/10/2026)

- O `Small_City_LVL` é World Partition: 78 atores no nível-base (52 HLOD, tráfego, multidão, ZoneGraph, 1 `PlayerStart`). As ruas **não** são atores com spline que
  dê para ler: o asfalto vem em ladrilhos gerados no Houdini (`Road/Kit_City_Road/SM_ROAD_<largura>_...`) e o trânsito vem de lanes do ZoneGraph/Mass.
  Copiar um trecho do mapa pronto traria o Mass inteiro (tráfego, multidão, ZoneGraph) e passaria dos 3 GB.
- O que é aproveitável como **kit**: prédios modulares `Building/CH/A..J` (parede, quina interna e externa, coluna), calçada (`Road/Kit_Sidewalk_A`, meio-fio
  `Kit_Small_Curb_A`), materiais de asfalto (`Road/Material/M_Asphalt_Master_Inst*`), mobiliário em `Prop/`, e carros em `Vehicle/`.

## Decisão (precisa do ok do Felipe: muda o jeito de montar a pista)

**Nossa própria rua, montada a partir de uma linha central, com o kit do City Sample como acabamento.** Nada do Mass/ZoneGraph do City Sample entra.

1. `Data/Courses.csv`: pontos da linha central (X, Y, Z em cm), largura da pista, nome. Um arquivo de dados, como o resto do jogo.
2. Ator C++ `ADNCourse` lê a linha, usa `FDNCourse`, e gera: malha da pista (asfalto do City Sample), meio-fio e calçada, largada/chegada, pontos de verificação
   a cada N metros, e o trilho do tráfego (reaproveita o `Rail` do `DNTrafficManager`).
3. Prédios: instâncias (`ISM`) das peças `CH/*` ao longo da calçada, com sorteio fixo por semente (mesmo percurso sempre igual).
4. Mobiliário (poste, hidrante, árvore) em instâncias, do `Prop/`, só o que o percurso usa.

Ordem: (a1) pista cinza gerada da linha, só o jogador, com checkpoints e progresso (jogável, feio); (a2) asfalto e calçada do City Sample; (a3) prédios e mobiliário;
captura aprovada pelo Felipe a cada passo. Orçamento total do City Sample no repositório: 3 GB.

## Riscos

- Montar prédios modulares ao longo de curvas dá encaixe feio; começamos com ruas de quarteirão reto e curvas de 90 graus em cruzamentos.
- Texturas de prédio do City Sample são 4k: reduzir para 2k na migração (regra do lote 20).

## Andamento

- **a1 feito (17 out):** pista cinza gerada da linha de `Data/CoursePoints.csv` (percurso1, 1976 m), largada, 7 pontos de verificação e chegada, progresso e chegada pelo Core, piloto automático que completa em 80,9 s. Sinal do erro de rumo: positivo = lado +Y, que no Unreal é a direita (o mesmo do esterço).
- **a2 e a3:** asfalto e calçada do City Sample, depois prédios e mobiliário; esperam o ok do Felipe sobre o rumo (seção Decisão).
- **a2 e a3 feitos (17 out), a primeira versão:** `ADNCourseActor` põe o asfalto (`M_Asphalt_Master_Inst`), o meio-fio e a calçada reta do City Sample e fachadas do kit CH/C L10_A dos dois lados (peças de um andar empilhadas em 4 a 10 andares, ordem sorteada por semente fixa), com um miolo cinza atrás para as janelas não mostrarem o chão. Céu e névoa novos. Falta: quinas e colunas (só entram com cruzamento), roof, mobiliário (poste, hidrante, árvore), texturas em 2k, e os vãos entre fachadas na parte externa das curvas.
- **1b feito (17 out), a primeira versão dos rivais:** 3 rivais (`Data/Rivals.csv`: Veloz 120/55, Firme 110/50, Cauteloso 100/46 km/h na reta/curva) saem da grade na frente do jogador, que larga em último, atrás de uma contagem de 3 s. Cada um dirige pelo `FDNRivalDriver`, a mesma regra do piloto automático do jogador. Corrida completa medida: Veloz 69,6 s, Firme 74,6 s, Cauteloso 80,8 s, jogador automático 83,0 s. A grade fica num trecho de 40 m de asfalto, calçada e prédios antes da largada. Falta: rivais que desviam do tráfego e do jogador, batem de volta e usam turbo; cores diferentes por rival; takedown (marco 1c).
- **1c feito (17 out), takedown e turbo na corrida:** o toque do jogador num rival abre uma janela de 1,5 s (`FDNTakedown`: velocidade relativa >= 20 km/h, rival "batido" quando o dano total dele sobe 0,05, isto é, uma zona com ~25%, ou ele fica destruído, e o jogador não destruído); vale o takedown, o turbo do jogador enche (`TakedownSetsBarTo`) e o rival fica destroçado 3 s e volta à pista, inteiro, no ponto da linha onde estava (`FDNRaceTakedown`, W1 a W4). Provado com o modo de teste `-DNRaceHunt` (o jogador mira no rival mais perto): "TAKEDOWN 1: Cauteloso, por trás, 71 km/h relativos, 0,19 s depois do toque; turbo 100", e o rival voltou 3 s depois. Quando o jogador também fica destruído no choque, não vale (regra do contrato): o log diz "bateu mas o takedown não vale". Falta: o jogador destruído hoje não volta (sem tela de fim), rival que bate de volta e usa turbo, pontuação do takedown, tela de resultado (marco 1d).
