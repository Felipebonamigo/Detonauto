# Mobiliário destrutível

Contrato do lote 17c. Pedido do Felipe em 14 out 2026: postes, árvores e
mobiliário (tudo menos prédios) caem ou quebram quando o carro bate, e o carro
segue perdendo só um pouco de velocidade. "O foco é em destruição."

## O que quebra e o que não quebra

- **Quebra:** poste de luz, árvore de calçada, banco, lixeira, hidrante, placa
  de rua e paraciclo. Mais tipos entram só com uma linha em `Data/Breakables.csv`.
- **Não quebra:** prédios, muros, meio-fio, barreiras de concreto e a barreira
  de teste. **Pontos turísticos e monumentos nunca quebram** (regra do
  `CLAUDE.md`): nenhum asset de `Data/Landmarks.csv` pode ser `ADNBreakableProp`.
- **Semáforo:** fica fora deste lote. O lote 20 decide como o ciclo do
  cruzamento continua com o semáforo derrubado.

## Regra pura `FDNBreakable` (Core, com teste)

Entradas: velocidade do carro **antes** do contato (lida no passo anterior, não
a que sobra depois que a física já freou), massa do carro (do preset em
`Cars.csv`), direção do carro até a peça e a linha do tipo.

1. **Limiar.** A peça quebra se a velocidade horizontal do carro for maior ou
   igual a `MinBreakSpeedKmh` do tipo. Abaixo disso, a peça é sólida e o carro
   bate como num obstáculo. Por exemplo, a 10 km/h contra um poste o carro para e
   o poste fica de pé.
2. **Perda de velocidade do carro,** por conservação de momento arcade:
   `Perda = 1 − M / (M + k · m)`, com `M` a massa do carro, `m` a do tipo e
   `k = ArcadeMassFactor` (global, padrão 1). A perda é limitada a
   `MaxSpeedLossPct` (padrão 25%) e multiplicada pela frontalidade
   `f = |v̂ · d̂|`, onde `d̂` é a direção horizontal do centro do carro ao centro da
   peça. Um raspão de porta perde quase nada. Velocidade depois do contato:
   `v · (1 − Perda · f)`. Só o módulo muda: a direção e o giro do carro ficam
   como antes do contato, sem guinada.
3. **Lançamento da peça:** velocidade inicial `v · LaunchFactor` na direção do
   carro, mais `LaunchUpMs` para cima, tudo limitado a `MaxLaunchMs`. Os tipos
   com `TopplesAtBase` recebem também um giro em torno da base, num eixo
   horizontal perpendicular ao carro: eles tombam em vez de voar.
4. **Dano no carro:** passa pelo `FDNDamage` de sempre, com a variação de
   velocidade `v − v'` na zona do contato, sem parâmetro novo. A batida em
   mobiliário **nunca** dispara a câmera de batida do lote 18 nem conta como a
   batida do jogador na rodada de Crash. Isso precisa ter teste.
5. **Pontos:** a coluna `Points` existe, com valores pequenos. A calibração é no
   lote 20, para o mobiliário temperar a rodada sem premiar quem desvia do
   tráfego.
6. Peça quebrada não volta inteira durante a rodada. Ao reiniciar a rodada, ela
   volta ao lugar.
7. Entradas NaN, negativas ou massa zero são tratadas como nos outros contratos
   (validação na carga do CSV e valor seguro na regra).

Exemplos com o carro de 1.500 kg, `k = 1` e batida de frente:

| Tipo | Massa (kg) | Perda | A 100 km/h, segue a |
|---|---|---|---|
| lixeira | 25 | 1,6% | 98 km/h |
| placa | 30 | 2,0% | 98 km/h |
| banco | 60 | 3,8% | 96 km/h |
| hidrante | 120 | 7,4% | 93 km/h |
| poste | 250 | 14,3% | 86 km/h |
| árvore | 400 | 21,1% | 79 km/h |

## `Data/Breakables.csv` (valores iniciais)

| Id | MassKg | MinBreakSpeedKmh | LaunchFactor | LaunchUpMs | TopplesAtBase | BreakHeightCm | Points |
|---|---|---|---|---|---|---|---|
| poste | 250 | 20 | 0,7 | 2 | sim | 10 | 50 |
| arvore | 400 | 25 | 0,5 | 1 | sim | 40 | 50 |
| banco | 60 | 8 | 1,1 | 3 | não | 0 | 20 |
| lixeira | 25 | 5 | 1,3 | 4 | não | 0 | 10 |
| hidrante | 120 | 10 | 0,9 | 2 | não | 0 | 30 |
| placa | 30 | 8 | 1,2 | 3 | sim | 0 | 20 |
| paraciclo | 40 | 8 | 1,0 | 2 | não | 0 | 10 |

`BreakHeightCm` é a altura onde a peça parte. O que fica abaixo (toco ou base) não
sai do lugar.

Os valores globais ficam numa linha de configuração, como em
`TrafficSettings.csv`:

| Valor | Padrão |
|---|---|
| `ArcadeMassFactor` | 1,0 |
| `MaxSpeedLossPct` | 25 |
| `MaxLaunchMs` | 35 |
| `MaxActiveProps` | 16 |
| `SleepSpeedMs` | 0,2 |
| `SleepAfterS` | 1,0 |
| `IgnoreCarS` | 0,3 |

## Física e custo

- **Peça intacta não tem custo:** sem tick e sem simulação. Só a peça solta
  simula.
- **Dormir:** abaixo de `SleepSpeedMs` por `SleepAfterS`, a peça para de
  simular e fica estática onde caiu. O destroço continua na cena até a rodada
  reiniciar.
- **Limite:** no máximo `MaxActiveProps` peças simulando ao mesmo tempo. Use a
  mesma regra do `FDNPartPool::Plan`, com um orçamento próprio, separado das 24
  peças do carro. Passando do limite, a peça mais antiga é congelada onde está.
  **Nada some à vista do jogador.**
- **Colisão:** a peça solta ignora o carro que a derrubou por `IgnoreCarS`, para
  não frear duas vezes nem prender embaixo do carro. Ela colide com o chão, com o
  tráfego e com as outras peças. Ligue CCD só enquanto ela estiver rápida, para
  não atravessar o chão.

## Escala: o desenho tem que servir à cidade

Na `L_Lookdev`, um ator por peça é aceitável. Na cidade (lote 20 em diante, com
o City Sample) haverá milhares de peças, e um ator por peça não cabe no custo. O
`ADNBreakableProp` precisa poder **sair de um pool e ser reiniciado**, para que
depois um gerenciador troque a instância (ISM/HISM com Nanite) atingida por um
ator físico do pool no instante da batida. Esta troca não é para agora, mas
nada neste lote pode depender de a peça já existir como ator desde o início do
mapa.

## Visual

- **Poste:** parte na base e fica o flange. O `make_street_light.py` gera a
  base como malha separada. Se a luz estiver acesa, ela apaga quando o poste cai.
- **Árvore:** o tronco parte a 40 cm e o toco fica. Tronco e copa caem juntos,
  girando. O corte do jacarandá do Poly Haven é feito por script do Blender: CC0
  permite modificar, e a modificação é anotada em `licencas.csv`. Folhas e
  galhos soltos ficam para o lote 19 (Niagara).
- **Hidrante:** o jato de água fica para o lote 19.
- **Banco, lixeira, placa e paraciclo:** voam inteiros.
- Som de impacto por material (metal, madeira, plástico) fica para o lote 21.

## Aprovação

1. **Batidas no `-Play` na `L_Lookdev`:** sequência de capturas (antes, no
   contato e depois) contra o poste, a árvore, o banco e a lixeira, a 60 e a
   100 km/h. O documento do lote traz a tabela de velocidade antes e depois de
   cada batida, comparada com a da regra.
2. **A 10 km/h contra o poste:** o carro para e o poste fica.
3. **Custo:** 16 peças derrubadas em sequência, num zigue-zague pela calçada,
   medidas no perfil `DN_AlvoPS5` (quadro e física, média e p95) e comparadas
   com a rua intacta.
4. **Nada atravessa o chão nem some à vista** nas capturas.
5. **O Felipe joga** e diz se parece destruição.
