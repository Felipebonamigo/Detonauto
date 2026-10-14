---
name: conferir-fato
description: 'Confere em fonte confiável o texto do Detonauto que fala do mundo real (cidades e pontos turísticos), anota a fonte e só então libera o texto. Use antes de escrever ou mudar `Data/Landmarks.csv`, `Data/Cities.csv` ou qualquer texto sobre cidade, quando houver texto marcado para revisão, ou quando o Felipe perguntar "isso está certo?".'
---
# Conferir um fato

Decisão do Felipe (14/10/2026, CLAUDE.md e `docs/menu-mapa.md`): só o que é público (monumento, praça, via, obra em
logradouro público), nenhuma marca real, nenhum ponto turístico destruído, e texto sobre cidade ou ponto turístico só
entra depois de conferido em fonte confiável.

## Onde está o texto

- `Data/Landmarks.csv`: `Id,CityId,Name,Kind,ShortText,SourceNote,NeedsReview`. Hoje sem linhas.
- O validador (`Source/Detonauto/Core/Data/DNDataValidation.cpp`) recusa `ShortText` com `NeedsReview=true` e
  `ShortText` sem `SourceNote`.
- O teste "nenhum texto de ponto turistico entra sem revisao" (`Core/Tests/DNDataFiles.spec.cpp`) ainda exige
  `NeedsReview=true` e texto vazio em todos. O primeiro texto conferido muda esse teste para "texto só com `SourceNote` e
  `NeedsReview=false`": é mudança de regra, combine com o Felipe antes.
- `Data/Cities.csv` (nome e coordenadas, também fixados por teste) e `Data/Regions.csv`.

## Fontes que valem

- Prefeitura, governo do estado, IPHAN e órgãos oficiais de patrimônio, para monumento e obra pública.
- Lei 9.610/98, art. 48 (obra em logradouro público pode ser representada), para saber o que pode entrar.
- Coordenadas: OpenStreetMap (crédito obrigatório) e Natural Earth.
Não valem: busca de IA, resumo de blog, Wikipédia sozinha (serve para achar a fonte primária), página sem autor nem data.

## Passos

1. Liste o que falta conferir: linhas de `Data/Landmarks.csv` com `NeedsReview=true` e texto proposto à espera, e todo texto novo sobre cidade.
2. Para cada fato (data, medida, autor, regra, nome), abra a página da fonte com o WebFetch. Trecho de resultado de busca
   não é fonte. Para pesquisa longa, use a skill `deep-research`.
3. Se a fonte confirma: anote a fonte em `SourceNote` (URL e data de acesso), escreva o `ShortText` com as próprias palavras e ponha
   `NeedsReview=false`.
4. Se a fonte desmente ou não acha: reescreva o texto com o que a fonte diz, ou tire o fato. Não deixe "aproximadamente" no lugar de conferir.
5. Texto que nasceu de IA continua sendo de IA depois de conferido: ele entra na declaração (skill `declaracao-ia`).
6. Commit com a lista do que foi conferido e as fontes.

## Armadilhas

- Ponto turístico com dono privado (estádio, prédio) ou com marca na fachada não entra, mesmo conferido.
- Imagem de satélite do Google ou da Apple não serve de referência (`docs/menu-mapa.md`).
