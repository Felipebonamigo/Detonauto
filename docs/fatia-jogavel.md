# Fatia jogável: o primeiro jogo completo

Decisão do Felipe, 16 out 2026: antes de refinar carros, ter **rodando no jogo** um carro completo, uma pista completa, um desafio de
explosão e um modo estilo destruição, "e por aí vai". Referência de trabalho (nunca nomes, textos ou telas): a corrida com **takedowns**
em ruas de cidade e o modo de explosão em cruzamento dos Burnout clássicos; o modo de destruição do Wreckfest e o arremesso do FlatOut.
A regra de ouro do estúdio vale: **divertido e bonito, com realismo e gráficos atuais**.

Este documento é o **desenho para aprovar**. Substitui o lote 20 isolado do `fatia-vertical.md`: o cruzamento vira um dos quatro modos.

## Os modos (por prioridade)

| # | Modo | O que é | O que já existe |
|---|---|---|---|
| 1 | **Corrida takedown** (principal) | Corrida ponto a ponto em ruas de cidade, contra 3 rivais, com tráfego. Derrubar rival (takedown) enche o turbo; chegar em primeiro ganha. | carro, turbo, takedown (regra pura), tráfego em trilho, dano, câmeras |
| 2 | **Crash** (explosão) | Cruzamento com tráfego: o carro entra a ~120 km/h e causa o maior estrago; pontos, multiplicador e medalha. | rodada, pontuação por dano, medalhas, câmera de batida |
| 3 | **Derby de destruição** | Arena fechada, vários carros batendo até sobrar um. Estilo Wreckfest. | dano, peças, efeitos |
| 4 | **Arremesso do piloto** (stunt) | O piloto é lançado do carro na batida e mira em alvos. Estilo FlatOut. | nada (piloto e boneco de física são novos) |

## O que falta, por área

- **Carro completo:** o carro do jogador já tem dano, peças, vidros e efeitos. Faltam o **interior visível** (vidros hoje são
  fumês opacos), **som** (lote 21) e o **piloto** (modo 4).
- **Pista de cidade:** hoje há uma rua reta de 1,2 km (`L_Lookdev`, blocos no lugar de prédios). Falta um **percurso**: curvas, cruzamentos,
  largada e chegada, pontos de verificação, trilho para a IA e o tráfego. Os prédios e o asfalto finais dependem do City Sample.
- **Rivais de corrida (a maior peça nova):** IA que segue a pista, pega curva, desvia do tráfego, bate de volta e usa turbo. É o que mais
  pesa em tempo e em risco.
- **Regras de corrida:** posição, voltas ou chegada, tempo, takedown contra rival com pontuação, tela de resultado. Pelo `FDNTakedown` já
  existente, mais regras puras novas com teste.
- **Menu e HUD:** escolher modo e pista, HUD de corrida (posição, tempo, turbo, takedowns), tela de fim. Hoje só há o HUD de depuração.
- **Som e música:** lote 21 (rotina `som-livre` e `gerar-som`).
- **Tráfego:** hoje são caixas. Pelo menos um modelo de carro de tráfego decente (o Cuiabano está pausado, em andamento no git) e a
  decisão `Rail` contra `Physics` (hipótese em `estado-atual.md`).

## Ordem proposta

1. **Corrida takedown, primeiro percurso** (peça mais longa). Marcos: (a) percurso com curvas e pontos de verificação, só o jogador;
   (b) rivais que correm o percurso; (c) takedown e turbo na corrida; (d) HUD e tela de resultado.
2. **Crash no cruzamento** (o lote 20): reaproveita o tráfego e a rodada que já existem.
3. **Menu mínimo** ligando os modos 1 e 2.
4. **Derby** (arena e rivais que batem).
5. **Arremesso do piloto.**
6. **Som** correndo em paralelo desde o item 1.

A cada marco: teste das regras puras antes do código, captura aprovada no `-Play`, custo medido no perfil alvo PS5 e uma linha no `estado-atual.md`
sobre o que mudou **na diversão e no visual**.

## Riscos e limites

- **A IA de corrida** pode tomar semanas de ajuste: começa simples (segue o trilho, vários níveis de agressividade em CSV) e só cresce se
  jogar der vontade de "mais uma".
- **Realismo e gráficos de hoje pedem os assets finais** (prédios, asfalto, carros do City Sample). O download no Fab é do Felipe; até
  lá a fatia roda com blocos e o carro do template, provisórios.
- **Quatro modos** é muito. Se o tempo apertar, a fatia fecha com os modos 1 e 2 e o resto fica em sequência.

## O que preciso de você para começar

1. **Aprovar esta ordem** (ou trocar a ordem dos modos).
2. **O percurso da corrida:** ponto a ponto (largada a chegada) ou **circuito de voltas** pelas ruas? Sugestão: ponto a ponto, mais
   curto e mais fácil de ler.
3. **Quando baixar o City Sample no Fab** (prédios, mobiliário, veículos), para os gráficos finais.

## City Sample: onde está e quanto entra (17/10/2026)

- Cópia de trabalho do City Sample em `D:\CitySample` (fora do repositório, 104 GB; o build do editor dele passou com `-NoUBA -MaxParallelActions=4`,
  porque o acelerador estourou a memória virtual, erro 1455).
- **Orçamento decidido pelo Felipe: até 3 GB de City Sample no repositório (LFS)**; hoje o LFS do jogo tem ~0,4 GB. Passou de 3 GB: parar e perguntar.
- Migrar só o que o trecho de corrida usa (kit de rua e calçada, prédios, mobiliário, árvores, 3 carros de tráfego), texturas em 2k onde der;
  cada grupo migrado ganha linha em `docs/licencas.csv`.
