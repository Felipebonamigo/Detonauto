---
name: declaracao-ia
description: 'Junta tudo o que o Detonauto tem de conteúdo gerado por IA (modelos do Meshy, texto, som, imagem) a partir dos registros do jogo e escreve a declaração de IA da Steam em PT e EN, separando o que foi feito antes do que é gerado durante o jogo. Use antes de publicar a página ou um build, quando entrar asset de IA novo, ou quando o Felipe perguntar "o que declaro de IA?".'
---
# Declaração de conteúdo de IA na Steam

A Steam pede, no questionário de conteúdo do Steamworks, o que o jogo usa de IA, em dois grupos:

- **Gerado antes** (pré-gerado): arte, modelo, som, texto ou código feitos com IA durante o desenvolvimento. A Valve
  pede a garantia de que não infringe direito de terceiros.
- **Gerado durante o jogo** (ao vivo): conteúdo criado por IA enquanto a pessoa joga. Exige dizer as proteções contra
  conteúdo ilegal. O Detonauto não gera nada ao vivo.

O texto aparece na página da loja. Código escrito com o Claude não entra, só o que o jogador vê ou ouve
(regra do estúdio, CLAUDE.md).

## Onde está o que é de IA neste jogo

- `docs/licencas.csv`, coluna `gerado_por_ia` (`sim` ou `não`). Hoje as 9 linhas dizem `não`.
- Texto de cidade ou ponto turístico que nasceu de IA (`Data/Landmarks.csv`; regra em `docs/menu-mapa.md`).
- Crédito "3D models generated with Meshy (meshy.ai)." na coluna `texto_do_credito`.

## Passos

1. Liste tudo o que está marcado como IA nos registros acima. Confira também os créditos ("generated with Meshy") e
   qualquer texto de interface, loja ou lugar que veio de IA sem reescrita humana.
2. Procure o que deveria estar marcado e não está: modelo com origem Meshy sem a marca, texto com "gerado" no
   histórico do commit. Corrija o registro antes da declaração.
3. Escreva a declaração em `docs/declaracao-ia.md`, em PT e EN, assim:
   - o que é (modelos 3D de props e personagens, texto de descrição, efeitos sonoros...), em linguagem de jogador;
   - de onde veio (ex.: "gerados com o Meshy e revisados à mão"; a licença de cada um está nos créditos do jogo);
   - o que **não** é IA, quando ajuda (música, vozes, código de jogo não entram na declaração).
4. Mostre o texto ao Felipe. É ele quem preenche o questionário no Steamworks.
5. Quando entrar asset de IA novo, atualize o arquivo no mesmo commit do asset.

## Armadilhas

- O Meshy grava metadado que identifica conteúdo de IA no GLB; os termos dele proíbem remover. Não limpe esse metadado ao reduzir o modelo.
- Uma declaração que esconde IA pode tirar o jogo da loja. Na dúvida, declare.
- Música gerada por IA é proibida no Detonauto (CLAUDE.md): ela nem chega à declaração.
