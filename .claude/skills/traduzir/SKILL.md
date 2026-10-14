---
name: traduzir
description: 'Escreve e confere o texto do Detonauto nos idiomas do jogo (PT, EN e ES), com um glossário único dos termos, e acha texto que ficou fora da tradução. Use ao criar ou mudar qualquer texto que o jogador lê (menu, HUD, tutorial, loja), quando o Felipe pedir "traduz", "põe em inglês/espanhol" ou "tem texto sem tradução".'
---
# Traduzir

Idiomas: português, inglês e espanhol no Acesso Antecipado; mais 4 idiomas na 1.0 (`docs/design.md`).

## Como o texto funciona neste jogo

Ainda não há texto traduzível. O HUD de hoje é de depuração (`FString` em `GEngine->AddOnScreenDebugMessage`,
`DNVehicle.cpp` e `DNCrashTestGameMode.cpp`), e os nomes vêm dos CSV de `Data/` como `FString`. O plano é
`Data/Strings.csv` na Sprint 22 (`docs/arquitetura.md`). Quando o primeiro texto de jogador entrar:

- texto de interface é `FText`, nunca `FString`;
- a fonte fica em texto, em `Data/Strings.csv`, importado como String Table (colunas `Key,SourceString,Comment`), como
  manda a regra 2 do CLAUDE.md;
- as traduções saem pelo Localization Dashboard (gather, depois `Content/Localization/Game/<idioma>/Game.po`, depois
  compile). O `.po` é texto: a nuvem escreve, o PC compila.

O idioma-fonte (PT ou EN) é decisão do Felipe: pergunte antes do primeiro texto.

## Passos

1. Escreva o texto já em todos os idiomas do jogo, no mesmo commit. Texto só em PT não entra.
2. Use os termos do glossário: `docs/glossario.csv` (colunas `termo_pt`, `en`, `es`, `nota`; crie na primeira tradução). Termo novo de jogo (golpe, modo, peça, recurso) entra no glossário
   antes de aparecer na tela.
3. Confira: no PC, rode o gather e veja no Dashboard que nenhum texto ficou sem tradução; na nuvem,
   `grep -rn 'AddOnScreenDebugMessage' Source/` acha texto fixo que deveria virar `FText`.
4. Texto longo (tutorial, loja): releia no idioma de destino como alguém daquele país leria. Tradução literal de
   gíria gaúcha ou de futebol não funciona; ache o equivalente.
5. Veja na tela: o inglês e o espanhol costumam ser 20 a 30 % mais longos que o português. Tire captura nos três idiomas (skill `jogar-e-capturar`).

## Armadilhas

- Na Steam, "Espanhol - Espanha" e "Espanhol - América Latina" são idiomas separados; o mesmo vale para "Português -
  Brasil" e "Português - Portugal". Declare só os que o jogo tem de verdade.
- Número, data e moeda mudam com o idioma (1.000,5 em PT, 1,000.5 em EN).
- Nome próprio de lugar real, marca e nome de pessoa não se traduzem; e o jogo não tem marca real (CLAUDE.md).
- Nome de modo herdado do Burnout não se traduz nem se usa: o nome final é próprio (CLAUDE.md).
