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
