# Lote 17c: mobiliário destrutível

Contrato: [`regras-mobiliario.md`](regras-mobiliario.md). Pedido do Felipe em 14 out 2026: postes, árvores e o resto do mobiliário
(tudo menos prédios) caem quando o carro bate, e o carro segue perdendo só um pouco de velocidade. Feito na sessão local.

## O que existe

- **Regra pura** `Core/DNBreakable` (teste `Detonauto.Mobiliario.Regra`, B1 a B9): limiar, perda de velocidade por momento arcade com
  teto e frontalidade, lançamento e tombo da peça, sono, política da batida (B7: peça não abre rodada de Crash nem chama a câmera de
  batida) e, novo neste lote, **varrer** (B9). Dados em `Data/Breakables.csv` (por tipo) e `Data/BreakableSettings.csv` (uma linha),
  com validação e teste de deriva (`Detonauto.Dados.Mobiliario`).
- **Peça** `Crash/ADNBreakableProp`: `Base` (o que fica: flange do poste, toco da árvore) e `Body` (o que quebra). Inteira, não tem
  tick nem simulação. Na quebra, o carro sai com a velocidade de antes do contato menos a perda da regra, e a peça é lançada e tomba.
  Pode ser reiniciada (`ResetProp`), para servir de pool quando a cidade trocar instâncias por atores.
- **Orçamento** `Crash/UDNBreakableSubsystem`: no máximo 16 peças simulando (`FDNPartPool::Plan`); passando disso, a mais antiga congela
  onde está. Destroço acordado pelo varrer volta ao orçamento sem contar como quebra nova.
- **Destroço** é um tipo de colisão próprio, `DNDebris` (`ECC_GameTraceChannel1`, `Config/DefaultEngine.ini`): bate no chão, nos
  prédios, no tráfego e nas outras peças, mas **nunca segura o carro, nem o chassi nem as rodas** (teste `Detonauto.Mobiliario.Colisao`,
  C1 a C4). O carro varre o destroço que alcança: ele sai da frente a 1,1 vez a velocidade do carro, com um pulo de 1,5 m/s, e o carro
  não muda (`ADNVehicle::PlowDebris`, uma consulta de sobreposição por quadro na caixa da carroceria).
- **Cena:** `create_lookdev.py` põe todo o mobiliário da `L_Lookdev` como `ADNBreakableProp`. O poste e o jacarandá entram cortados por
  `Tools/Blender/split_prop.py` (poste a 10 cm, jacarandá a 40 cm na escala da cena): `SM_dn_street_light_base` (96 vértices) e
  `_top` (428); `SM_jacaranda_tree_stump` (15 mil) e `_top` (3 milhões, o modelo original). As linhas de licença estão em
  `docs/licencas.csv` (CC0 permite modificar; a modificação está anotada).

## Ferramentas de teste

No `vehicle-probe.ps1 -Map /Game/Detonauto/Maps/L_Lookdev -Keys ... -ExtraArgs ...`:

- `-DNPropLine=poste,arvore,banco,lixeira` põe cópias das peças do mapa na faixa do carro, a partir de `-DNPropStartM` (padrão 60 m),
  a cada `-DNPropSpacingM` (padrão 45 m). No QUIT, o log traz o veredito:
  `DNPropLine: resumo N de M quebrada(s) | menor velocidade depois da 1a quebra ... | maior desvio ... | APROVADO`. Aprova quando todas
  quebram, o carro nunca cai abaixo de 60% do cruzeiro e nunca sai mais de 1,5 m da faixa ("bater e seguir").
- `-DNCruiseKmh=N`: piloto automático a N km/h com manutenção de faixa (log `DNCruise` a cada 0,5 s).
- `-DNPropShots`: capturas antes (a 12 m), no contato e 0,8 s depois de cada quebra (`dn-prop-<n>-<tipo>-...`).
- `lookdev-measure.ps1` imprime também `Exclusive/GameThread/Physics` e `Exclusive/AllWorkers/Physics`.

## O que mudou no contrato (medido)

1. **Sai `IgnoreCarS` (0,3 s); o destroço nunca mais segura o carro.** Na fileira de 16 peças o carro parava ou saía da rua. Dois
   motivos, achados com log de contato em cada fronteira:
   - o traço da suspensão das rodas da Chaos corre no canal WorldDynamic. As rodas subiam no banco que voava à frente, sem nenhum contato
     no chassi, e o carro saía lançado de 60 a 107 km/h girando. Agora as rodas ignoram `DNDebris`, mas continuam vendo o chão, os
     prédios e o tráfego (`PhysicsBody`);
   - a 100 km/h, a lixeira lançada a 35 m/s voltava a segurar o chassi depois de 0,3 s, caía à frente, o carro a alcançava e ela ficava
     presa no para-choque contra a árvore seguinte: de 101 para 6 km/h.
2. **Entra o varrer (B9),** com `PlowFactor` 1,1 e `PlowUpMs` 1,5 em `BreakableSettings.csv`. Sem ele, o carro atravessava o banco
   caído quando o alcançava ([`varrer-sem-e-com.jpg`](img/mobiliario-17c/varrer-sem-e-com.jpg): à esquerda, o banco cruzando o capô;
   à direita, o banco jogado na calçada).
3. **A base e o toco são destroço desde o início:** as rodas passam por cima do toco de 40 cm da árvore sem pular.

## Provas

- Suíte 659/659. Testes novos vistos falhar antes da correção: C1, C3 e C4 (tipo 1 em vez de 14, respostas 2 em vez de 0); os três de
  B9 (o do teto de 35 m/s passava com o corpo vazio e foi trocado por um valor exato); os de faixa e cópia de `PlowFactor` e `PlowUpMs`
  e a carga do CSV real sem as colunas.
- O veredito da fileira reprovou antes da correção (100 km/h: menor velocidade 1 km/h) e aprovou depois:

| Fileira | Quebradas | Menor velocidade | Maior desvio | Veredito |
|---|---|---|---|---|
| 16 peças a 60 km/h, a cada 12 m | 16 de 16 | 52 km/h | 0,09 m | APROVADO |
| 8 peças a 100 km/h, a cada 30 m (3 árvores) | 8 de 8 | 78 km/h | 0,14 m | APROVADO |
| poste, árvore, banco, lixeira a 60 km/h | 4 de 4 | 49 km/h | 0,10 m | APROVADO |
| poste, árvore, banco, lixeira a 100 km/h | 4 de 4 | 78 km/h | 0,20 m | APROVADO |

- **Velocidade medida no motor contra a regra** (carro arcade-3, 1.350 kg; a regra dá a perda e a velocidade que sobra a 60 e a
  100 km/h; medido = antes → depois no log `DNProp`):

| Tipo | Perda da regra | A 60 km/h, medido | Perda medida | A 100 km/h, medido | Perda medida |
|---|---|---|---|---|---|
| poste (250 kg) | 15,6% | 63,1 → 53,3 | 15,5% | 99,9 → 84,3 | 15,6% |
| árvore (400 kg) | 22,9% | 63,1 → 48,7 | 22,8% | 101,0 → 77,9 | 22,9% |
| hidrante (120 kg) | 8,2% | 62,2 → 57,2 | 8,0% | 99,8 → 91,6 | 8,2% |
| banco (60 kg) | 4,3% | 63,1 → 60,4 | 4,3% | 100,9 → 96,6 | 4,3% |
| lixeira (25 kg) | 1,8% | 63,1 → 62,0 | 1,7% | 101,4 → 99,5 | 1,9% |

  A diferença de até 0,2 ponto vem da frontalidade (o carro nunca bate exatamente no centro da peça).
- **A 10 km/h contra o poste** (13,3 km/h no contato, abaixo do limiar de 20): o carro para (3,5 km/h logo depois, 0 em seguida) e o
  poste fica de pé.
- **Capturas** no jogo da cena de aparência (roteiro `-Keys` com o piloto), antes, no contato e 0,8 s depois:
  [`batidas-60kmh.jpg`](img/mobiliario-17c/batidas-60kmh.jpg) e [`batidas-100kmh.jpg`](img/mobiliario-17c/batidas-100kmh.jpg).
  Nada atravessa o chão nem some à vista.
- **Custo** (perfil DN_AlvoPS5, 1920x1080, RTX 4070 Ti, janela de 8 a 18 s, duas execuções de cada): [`desempenho.md`](desempenho.md),
  seção do lote 17c. O quadro não muda (4,8 ms); a linha do jogo sobe 0,18 ms na média e 0,55 ms no p95, e a física nos trabalhadores
  sobe 0,11 ms.

## Ficou para depois

- **O Felipe jogar e dizer se parece destruição** (item 5 da aprovação do contrato).
- A medição pôs as 16 peças em fila na faixa do carro, e não em zigue-zague pela calçada como o contrato pedia: o piloto do teste segue
  reto. Com o carro passando reto, as 16 caem; o custo de simular é o mesmo.
- A árvore a 60 km/h é lançada a 8,8 m/s, mais devagar que o carro: ela fica para trás tombada sobre a rua, e o carro passa sob a copa.
  O varrer empurra o centro de massa, mas a copa é grande. Fica aceitável na captura; um tombo para o lado é ajuste do lote 19.
- Placa e paraciclo existem no CSV, mas ainda não têm modelo na cena.
- Folhas, faíscas e o jato do hidrante ficam para o lote 19; o som de impacto, para o lote 21.
