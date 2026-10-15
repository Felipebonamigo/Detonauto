# Lote 17b: desgaste da pintura e vincos duros

Pedido na revisão do lote 17 ([`lote-17.md`](lote-17.md)): o carro amassado parecia "plástico derretido". Faltavam riscos, tinta
arrancada, primer, metal nu e fuligem, e os amassados precisavam de vincos duros de chapa dobrada. Contrato:
[`regras-dano-visual.md`](regras-dano-visual.md). Feito na sessão local em 14 e 15 out 2026.

## O que existe

- **Desgaste da pintura** (`Tools/Unreal/create_dent_material.py`): lascas (primer e, no miolo, metal nu), riscos compridos e
  fuligem, por ruído 3D procedural, sem textura. Onde aparece: o peso de cada zona (o mesmo dos alvos de deformação, mandado pelo
  `ADNVehicle` com a caixa da carroceria) vezes a cor de vértice que o Blender grava (quanto cada zona amassa ali, puxada para as
  cristas dos vincos). O chassi fica sem lascas. Parâmetros em `Data/Damage.csv`: `WearChipCoverage` 0,2, `WearChipSizeCm` 6,
  `WearScratch` 0,6 e `WearSoot` 0,5 (com 0,5 e 14 cm o carro parecia camuflado).
- **Vincos** (`Tools/Blender/make_damage_morphs.py`): subdivisão simples antes dos alvos (6,5 mil para 37,8 mil vértices; normais e
  UVs interpolados) e vincos com crista viva (perfil 1 − |sen|) na frente, na traseira, nas portas e no teto.
- **Barreira de teste** na cena de aparência: `-DNWallAheadM=M` põe uma barreira de concreto atravessando a rua a M metros da largada.

## Fechamento (15 out 2026)

### A frente em 1,0 mais destruída

A batida real a 100 km/h deixava a frente em 1,0, mas o nariz ficava arredondado, os faróis inteiros e o capô quase liso. Mudou só a
frente (a traseira ficou igual):

- **Recuo em rampa:** a ponta recua 0,75 do balanço (era 0,55). Com a queda suave de antes, o recuo não passa de ~0,66 sem a malha
  dobrar sobre si (determinante 1 − 1,5 × recuo). A frente usa agora uma rampa quase linear (`crush_ramp`, inclinação de no máximo 1).
  Mais que isso o nariz passaria das rodas: o balanço é só 15,6% do comprimento.
- **Capô em tenda** mais alto (`HOOD_RISE` 0,08 para 0,14) e **vincos da frente** 1,6 vez mais fortes que os da traseira.
- Conferência de dobra: `DN_Front` com deslocamento máximo de 64,7 cm (eram 42,5 no 17) e menor determinante 0,17. As outras zonas:
  `DN_Rear` 38,6 cm e 0,18; `DN_Left` 26,8 cm e 0,54; `DN_Right` 28,9 cm e 0,51; `DN_Roof` 13,9 cm e 0,69.
- Antes e depois, em 1,0: [`frente-antes-depois.jpg`](img/dano-zonas-17b/frente-antes-depois.jpg). O para-choque recua até as rodas,
  que despontam nas quinas, e o farol e o capô amassam junto.

### Capturas

Perfil alvo PS5, 1920x1080, carro parado na largada da `L_Lookdev`, as 5 zonas por `DNZone`, câmera de foto nos 4 quartos.
- Os quartos da frente (135 e 225 graus) estão com o sol original.
- Os de trás (315 e 45 graus) estão com o sol girado 180 graus (`-DNSunYaw=180`), para o sol ficar sempre por trás da câmera.

Capturas:
- Níveis intacto, 0,3, 0,6 e 1,0: [`lookdev-niveis.jpg`](img/dano-zonas-17b/lookdev-niveis.jpg).
- 1,0 de perto: [`lookdev-1.0-frente-esquerda.jpg`](img/dano-zonas-17b/lookdev-1.0-frente-esquerda.jpg) e
  [`lookdev-1.0-traseira-direita.jpg`](img/dano-zonas-17b/lookdev-1.0-traseira-direita.jpg).
- Lado a lado com o "plástico derretido" do 17, mesma vista:
  [`17-x-17b-frente-esquerda.jpg`](img/dano-zonas-17b/17-x-17b-frente-esquerda.jpg).
- **Batida real a 100 km/h na barreira, no jogo** (`-DNCruiseKmh=100 -DNWallAheadM=120`): 101,7 km/h antes do contato, variação de
  36,9 m/s, frente em 1,0, o carro volta a ~30 km/h. Capturas: [`parede-100kmh-frente.jpg`](img/dano-zonas-17b/parede-100kmh-frente.jpg),
  [`parede-100kmh-frente-esq.jpg`](img/dano-zonas-17b/parede-100kmh-frente-esq.jpg) e
  [`parede-100kmh-lado-esq.jpg`](img/dano-zonas-17b/parede-100kmh-lado-esq.jpg). O ricochete varia de uma execução para outra, e às
  vezes o carro para na sombra dos prédios.

Ferramentas novas para as capturas:
- A câmera de foto (`CAM<graus>` no roteiro, teclas T/Y/U) usa FOV de 60 graus. Antes valia o FOV de 95 do modo de jogo, aplicado a
  cada quadro, e o carro saía pequeno no quadro.
- `-DNSunYaw=G` gira o sol só naquela execução.
- O piloto `-DNCruiseKmh` solta o carro na primeira batida real; daí em diante, mandam as teclas do roteiro.

### Lascas sem confete (revisão do fechamento)

As lascas pareciam confete: retângulos claros e pequenos espalhados por igual no capô, no para-lama e na asa. Achado por eliminação
no motor: zerando só a cobertura das lascas os retângulos ficavam; zerando também os riscos, sumiam. Eram os **riscos**. O ruído deles
é comprido em X (120 cm) e curto em Y e Z (3 cm); nas faces viradas para a frente, que são planos Y-Z, virava quadradinho de 3 cm.

O que mudou no material (`create_dent_material.py`, sem dado novo):
- os riscos usam a normal local e somem nas faces de frente e de trás: raspão corre ao longo do carro;
- as lascas só nascem onde o dano local passa de 0,08 (zona × cor de vértice das cristas, mais a ponta da frente ou da traseira
  com o dano da zona), em grupos de tamanhos variados, e ficam mais densas perto da batida (a máscara dos amassados);
- a borda da lasca vem de ruído com distorção de domínio;
- a cor fica em camadas: primer cinza-claro fosco (albedo 0,5, aspereza 0,7) e, no miolo das lascas grandes, metal nu
  cinza-aço (metálico 1, aspereza 0,35). O risco mostra primer em vez de clarear a tinta;
- a máscara do teto fica só sobre a cabine: a asa traseira só ganha lasca com a traseira batida (na batida real, só de frente, a
  asa fica limpa);
- a posição que o material lê é a da chapa já amassada. A frente em 1,0 recua ~60 cm, então a faixa da ponta vai até 25% do
  comprimento.

Antes e depois: [`lascas-antes-depois.jpg`](img/dano-zonas-17b/lascas-antes-depois.jpg) e
[`lascas-de-perto.jpg`](img/dano-zonas-17b/lascas-de-perto.jpg). As capturas `lookdev-1.0-frente-esquerda.jpg` e
`parede-100kmh-frente-esq.jpg` foram refeitas. Na frente as lascas ficam esparsas; nas dobras da porta e na asa (com a traseira
batida) elas aparecem em grupos. Os riscos ainda quase não se leem a 3 m.

### Custo: pendente

A medição com 37,8 mil vértices (6 carros amassados contra 6 intactos, como no lote 17) não fechou: outras sessões (TDFende e
AgeOfEarth) abriram a Unreal no mesmo PC durante as rodadas, e uma delas deu 47,9 ms de quadro com a GPU dividida. O
`lookdev-measure.ps1` agora descarta o cenário se houver outro Unreal aberto antes ou depois dele. A medição fica para uma janela com o
PC livre. Expectativa: no lote 17, com 7 mil vértices, os alvos de deformação ficaram dentro do ruído; o skin cache com 5,4 vezes mais
vértices deve continuar abaixo de 1 ms.

## Ficou para depois

- O custo com 37,8 mil vértices, com o PC livre (acima).
- O Felipe jogar e dizer se agora parece chapa amassada.
- A lasca de tinta, de perto, ainda parece pontilhado claro uniforme. Variar o tamanho e juntar lascas nas cristas é ajuste para
  quando o carro final (City Sample) entrar.
