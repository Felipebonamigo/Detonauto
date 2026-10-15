# Peças que soltam e vidros que quebram

Contrato do lote 18b. É a camada 2 do dano (`docs/plano-visual.md`). Pedido do
Felipe em 15 out 2026: "pode sair pedaços do carro também: paralama, portas,
vidros, aerofólio, etc."

A regra de **quando** uma peça solta já existe no Core (`docs/regras-dano.md`,
regra 4, e `FDNPartPool`). Este lote faz três coisas:

1. amplia a lista de peças;
2. cria um estágio intermediário, a peça **pendurada**;
3. faz tudo **aparecer**: malhas separadas, física, vidro trincado e estourado,
   farol apagado.

**Referência: Wreckfest** (o Felipe: "tipo o que acontece no Wreckfest"). O que
faz o dano de lá convencer, e este lote copia a ideia, não o asset:

- a peça não some de uma vez: primeiro **pendura**;
- o para-choque **arrasta no chão** (com faísca, no lote 19);
- a porta abre e balança;
- o capô levanta;
- o que cai **fica na pista** como obstáculo até o fim da rodada;
- o vidro estoura em cacos;
- no carro destruído, roda pode sair.

Começa no carro do template, que já está no projeto. O mesmo processo (script
do Blender e CSV) vai ser repetido no carro do City Sample quando ele for
escolhido, então nada pode ser feito à mão só para este carro.

## Peças e estados

Cada peça passa por **Presa → Pendurada → Solta**, sem volta. Peças sem
dobradiça pulam de Presa para Solta.

| Peça | Zona | Zona secundária | Massa (kg) | Pendura (dano / Δv) | Solta (dano / Δv) | Dobradiça |
|---|---|---|---|---|---|---|
| para-choque dianteiro | Front | – | 8 | 0,30 / 8 | 0,60 / 12 | lateral, 60° (até tocar o chão) |
| para-choque traseiro | Rear | – | 8 | 0,30 / 8 | 0,60 / 12 | lateral, 60° (até tocar o chão) |
| capô | Front | Roof | 12 | 0,35 / 9 | 0,75 / 16 | borda do para-brisa, 55° |
| tampa traseira | Rear | – | 10 | 0,35 / 9 | 0,75 / 16 | borda dianteira, 55° |
| porta esquerda | Left | – | 20 | 0,35 / 9 | 0,80 / 16 | borda dianteira, 70° |
| porta direita | Right | – | 20 | 0,35 / 9 | 0,80 / 16 | borda dianteira, 70° |
| para-lama dianteiro esquerdo | Front | Left | 5 | – | 0,70 / 14 | – |
| para-lama dianteiro direito | Front | Right | 5 | – | 0,70 / 14 | – |
| para-lama traseiro esquerdo | Rear | Left | 5 | – | 0,70 / 14 | – |
| para-lama traseiro direito | Rear | Right | 5 | – | 0,70 / 14 | – |
| aerofólio | Rear | Roof | 6 | – | 0,50 / 10 | – |

- **Dados:** a tabela fica em `Data/CarParts.csv`, uma linha por peça e por carro
  (coluna `CarId`), com validador e teste de deriva como os outros CSVs.
- **Dano usado:** o dano da peça é o **maior** entre a zona e a zona secundária.
- **Δv:** conta quando o impacto acerta uma das duas zonas da peça. Os limiares
  são inclusivos, como no `regras-dano.md`.
- **Enum:** as peças novas entram **no fim** do `EDNDamagePart`, para não mudar a
  ordem das que já existem nem os testes delas. O `GlassFront` sai da lista de
  peças, porque vidro tem regra própria (abaixo). Registre a decisão e ajuste os
  testes que o usam.
- **Um mesmo impacto** pode pendurar e soltar a mesma peça. Nesse caso ela vai
  direto para Solta.
- **Retrovisor:** se a malha do carro tiver, ele entra como peça sem dobradiça
  da zona lateral, com 1 kg e soltando em 0,40 / 8. O do template provavelmente
  não tem.

## Vidros e faróis (`FDNGlass`, regra pura)

| Vidro | Zona | Trinca de | Estoura em | Quando estoura |
|---|---|---|---|---|
| para-brisa | Front, secundária Roof | 0,40 | **nunca** | trinca até 1,0 e afunda junto do teto: vidro laminado não some |
| laterais esquerdos | Left | 0,30 | 0,60 ou Δv ≥ 12 na zona | somem e soltam cacos |
| laterais direitos | Right | 0,30 | 0,60 ou Δv ≥ 12 na zona | somem e soltam cacos |
| traseiro | Rear, secundária Roof | 0,30 | 0,65 ou Δv ≥ 12 | some e solta cacos |
| farol, de cada lado | Front | 0,30 | 0,60 | a lente trinca e a luz apaga |
| lanterna, de cada lado | Rear | 0,30 | 0,60 | a lente trinca e a luz apaga |

- **Trinca:** a força vai de 0 no início da trinca até 1 em 1,0 de dano. No
  material é uma máscara de trinca (Voronoi e linhas radiais, ou textura CC0),
  centrada no ponto de impacto mais próximo e espalhando com o dano.
- **Cacos:** um Niagara curto (cubos pequenos de vidro temperado, ~0,8 s),
  com orçamento próprio e pool. É o mesmo efeito que o lote 19 vai reaproveitar.

## Como aparece

### Malhas (Blender, por script)

`Tools/Blender/split_car_parts.py` lê o carro e um mapa por carro,
`Tools/Blender/parts/<CarId>.json`.

1. Separa as peças:
   - **Peças que já são cascas separadas** (provável no para-choque, capô,
     para-lama e aerofólio do template) são escolhidas pelo centro da caixa e
     pelo mapa.
   - **Peças coladas na carroceria** (provável na porta) são cortadas por uma
     caixa alinhada às linhas de painel e fechadas.
2. Cada peça sai como **malha com os mesmos 5 alvos de deformação**, calculados
   pelo **mesmo campo espacial** do `make_damage_morphs.py`. Presa, ela amassa
   igual à carroceria, sem fresta, porque o campo é função da posição.
3. Onde a peça sai fica um **vão**, uma malha escura simples que só aparece
   quando a peça sai:
   - porta: interior da porta;
   - capô: cofre do motor, um bloco escuro com volumes de motor;
   - para-choque: travessa.
   Nunca se vê através do carro.
4. Os vidros saem como malhas próprias, com o material de vidro e a máscara de
   trinca.
5. O script gera uma imagem de conferência por peça em `docs/img/pecas/`.

### No motor

- **Presa:** componente preso ao chassi. O peso dos alvos de deformação é
  copiado da carroceria a cada mudança.
- **Pendurada:** a peça vira corpo físico ligado ao chassi por uma restrição de
  física (constraint), com o giro limitado ao ângulo da dobradiça. Ela balança
  com o movimento do carro.
  - Colisão de destroço (`DNDebris`), então nunca segura o carro.
  - O para-choque pendurado **arrasta no chão**: uma ponta continua presa e a
    outra encosta no asfalto. O ponto de contato fica exposto num evento
    (`OnPartScrape`), para o lote 19 ligar faíscas e o lote 21 o som de raspar.
  - A massa pequena e um amortecimento angular (padrão 2) evitam que ela tire o
    carro do rumo.
- **Solta:** a restrição quebra e a peça sai com a velocidade do ponto do carro
  onde estava, mais um empurrão na direção da batida (`PartLaunchMs`, padrão 4).
  - O peso dos alvos fica congelado: a peça voa já amassada.
  - Mesma colisão de destroço, varrer e dormir do mobiliário
    (`regras-mobiliario.md`, regras 8 a 10).
  - Orçamento: o `FDNPartPool` com `MaxActive` 24, contando as peças penduradas
    e as soltas.
- **Reiniciar a rodada** devolve tudo: peças presas, vãos escondidos, vidros
  inteiros, faróis acesos.
- **Tráfego:** só o carro do jogador ganha peças neste lote. O tráfego ganha
  quando tiver carros preparados (City Sample, lote 20 ou depois).

### Roda solta (como no Wreckfest; opcional, só se sobrar)

Com o carro **destruído** (`bDestroyed`), a roda visual do canto mais batido
pode sair voando. A roda física do Chaos continua, escondida, para o destroço
não mudar de comportamento. Só entra se o resto estiver aprovado.

## Custo

- Peça presa: zero de física, só a malha a mais.
- Medir no `DN_AlvoPS5` (quadro e física, média e p95):
  - o carro inteiro, para ver o custo das malhas separadas;
  - 6 carros com todas as peças penduradas;
  - 24 peças soltas ao mesmo tempo.

## Aprovação

Todas as capturas com o sol por trás e em 1920.

1. **Imagem de conferência** do Blender de cada peça e do carro remontado sem
   fresta, nos estados 0 e 1,0.
2. **Batida a 100 km/h na parede, no `-Play`, com a câmera de batida do lote
   18:**
   - para-choque e capô saindo;
   - aerofólio voando, se a traseira bater;
   - farol apagado;
   - para-brisa trincado.
3. **Para-choque arrastando:** depois de pendurar, o carro anda a 60 km/h e o
   para-choque raspa no chão. Captura de lado e log do `OnPartScrape`.
4. **Batida lateral** (tráfego ou parede em T):
   - porta pendurada balançando;
   - vidro lateral estourando com cacos;
   - numa segunda batida, a porta saindo.
5. **Carro andando com o capô pendurado** a 80 km/h, de lado, sem atravessar a
   carroceria nem o para-brisa.
6. **Nenhum vão transparente:** captura de perto, sem a porta e sem o capô.
7. **Custo** acima.
8. **O Felipe joga** e diz se os pedaços saindo "vendem" a batida.

## Decisões da implementação (15 out 2026)

Detalhes, capturas e números em `docs/lote-18b.md`.

- **P5, empurrão da peça solta:** vai para fora do carro, pelo lado da peça, sem a parte que entraria no obstáculo. A peça no eixo da
  batida (para-choque, capô, tampa) sai de lado. Isso substitui "na direção da batida": na direção da batida a peça ia contra a parede,
  e para longe dela atravessava o próprio carro (o destroço ignora o carro).
- **A câmera atravessa a peça** pendurada ou solta. A esfera da câmera de batida parava nela, ao lado do carro, e entrava no carro.
- **Dobradiça:** a restrição de física deixa girar de um lado só. Capô e tampa têm uma mola no meio da faixa e abrem 27 graus; sem ela,
  a gravidade fechava. A peça pendurada não colide com destroço e só raspa contra o chão estático.
- **Cacos** por `ADNGlassShards` (cubos instanciados, sem física e sem Niagara) até o lote 19.
- **Farol e lanterna:** a lente do template não tem luz. "A luz apaga" virou lente fosca com a trinca, e depois estouro com cacos.
- **Trinca procedural** no material do amassado: radiais, anéis em polígono, teia no centro. O centro fica recuado para dentro do vidro,
  longe da borda escondida sob o capô.
- **Vão:** casca escura feita da malha grossa, sem bloco de motor.
- **Quem tem peças:** todo `ADNVehicle`. O tráfego (caixa provisória) segue com a regra 4 do `regras-dano.md` até ter carro preparado.
- **Retrovisor:** o template não tem. **Roda solta:** não entrou.
