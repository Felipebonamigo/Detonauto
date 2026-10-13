# Menu de mapa: cidades do Brasil e do mundo (conceito)

Ideia do Felipe, 13 out 2026, a partir de uma tela de seleção de região de um
jogo de corrida clássico: o jogador escolhe onde jogar girando um globo, e a
cidade escolhida mostra um recorte da realidade, com pontos turísticos.

Este documento é conceito. Nada aqui está decidido além do que está marcado como
decisão. Os números são suposições a validar.

## O que a tela de referência faz

Vista numa captura de tela da referência (só como inspiração de fluxo, sem
copiar visual, nome nem texto):

- Um globo 3D com a região escolhida em destaque e o resto escurecido.
- Uma lista curta de regiões ao lado, com a atual marcada.
- Dois contadores com selo de conclusão: progresso na região e progresso total.
- Quatro ações nos botões: voltar, detalhes do jogador, trocar e selecionar.

O que aproveitar é o **fluxo**: globo, região, cidade, cruzamento, com
progresso visível em cada nível. A identidade visual do Detonauto é própria.

## Fluxo proposto

1. **Globo.** Aparece o planeta. O Brasil começa em destaque, e as outras
   regiões aparecem apagadas, como "em breve".
2. **Região.** Ao escolher o Brasil, o globo gira e dá zoom. As cidades
   aparecem como pontos.
3. **Cidade.** Um painel mostra a cidade: uma imagem ou cena, um texto curto,
   a lista de pontos turísticos e o progresso (cruzamentos feitos e medalhas).
4. **Cruzamentos.** A lista de cruzamentos daquela cidade, com a medalha de
   cada um.
5. **Jogar.**

Contadores em cada nível, no formato feito/total.

## Dados em tabelas, como o resto do jogo

| Tabela | Conteúdo |
|---|---|
| `Data/Regions.csv` | Id, nome, país ou grupo, ordem, desbloqueio |
| `Data/Cities.csv` | Id, nome, região, latitude, longitude, distrito do jogo, texto curto, desbloqueio |
| `Data/Landmarks.csv` | Id, cidade, nome, tipo, texto curto, fonte do texto, necessita revisão |
| `Data/Crossings.csv` | A tabela de cruzamentos, com a coluna `Cidade` |

**Ids.** Dois ids que só diferem em maiúsculas e minúsculas são o mesmo id (como o
`FName` da engine), e o validador os recusa como duplicados. Um id só pode ter
letras sem acento, números, hífen e sublinhado, para ser seguro como nome de
ativo e como chave.

Latitude e longitude servem para o globo girar até a cidade e para colocar o
ponto. A lógica de progresso (feito sobre total, medalhas) é regra pura em
`Core/`, com testes, como as demais.

## O globo, com ferramentas gratuitas

- Uma esfera com textura da Terra. A imagem Blue Marble da NASA e os dados do
  Natural Earth (domínio público, confirmar na página de cada arquivo) cobrem a
  textura e os contornos de países.
- Os pontos das cidades são widgets presos à esfera. A conversão de latitude e
  longitude em posição é uma conta simples, feita em `Core/` e testada.
- Câmera que gira até a cidade escolhida com uma suavização.
- Nada de imagem de satélite do Google ou da Apple: são proprietárias.

## "A realidade da cidade", e o risco jurídico

**Pontos turísticos.** O plano do jogo hoje tem **um distrito urbano**, e dois na
versão 1.0. Recriar uma cidade real com fidelidade está fora do que uma pessoa
faz. A proposta realista é cada cidade ser um **distrito inspirado** nela:

- ruas e quarteirões estilizados, com a identidade visual da cidade (cores,
  placas em português, tipos de carro e de tráfego);
- **um a três elementos reconhecíveis** da cidade, como silhueta ao fundo ou
  monumento em ponto de referência, e não a cidade inteira;
- no menu, a lista de pontos turísticos com um texto curto e, se possível, uma
  imagem.

**Cuidados antes de lançar, a validar com um advogado:**

1. **Obras em espaço público.** A lei brasileira (Lei 9.610/98, art. 48) permite
   representar livremente, em pintura, desenho, fotografia e audiovisual, obras
   situadas permanentemente em logradouros públicos. **A regra varia por país**,
   e o jogo vende no mundo todo. Arquitetura moderna de autor ainda vivo ou
   morto há menos de 70 anos pode ter proteção em alguns lugares. Prefira
   monumentos antigos e formas genéricas.
2. **Marcas.** Nenhuma marca real em fachadas, placas ou outdoors. As marcas
   fictícias do jogo (desenhadas em SVG) cumprem esse papel.
3. **Destruir patrimônio.** Nenhum ponto turístico é destruído no jogo. Eles
   ficam de fundo ou fora da zona de colisão, para não gerar problema de
   imagem nem de reação do público.
4. **Dados de mapa.** O OpenStreetMap é livre, com a obrigação de crédito
   (© colaboradores do OpenStreetMap) e regras próprias sobre bases derivadas.
   Uso em massa dos dados exige ler a licença antes.
5. **Textos sobre a cidade.** Precisam ser **verdadeiros**. Se forem escritos
   com ajuda de IA, entram na declaração de conteúdo gerado por IA da Steam, e
   cada fato é conferido em fonte confiável antes de entrar. A coluna
   `necessita revisão` de `Landmarks.csv` existe para isso.

## Escopo e calendário (proposta)

Esta camada tem dois custos muito diferentes:

| Parte | Custo | Quando |
|---|---|---|
| Menu do globo, com progresso e dados em tabela | Médio: umas 2 a 3 semanas | Sprints de interface, perto do Acesso Antecipado |
| **A arte de cada cidade** | **Alto**: é o distrito inteiro | Uma cidade por atualização |

Proposta: no Acesso Antecipado, o globo mostra o Brasil e uma primeira cidade
jogável. As demais aparecem como "em breve", o que também ajuda a juntar lista
de desejos. Cada cidade nova vira uma atualização, o que se encaixa no plano de
atualizações a cada três meses.

Isso muda o plano atual de "2 distritos na 1.0" para "N cidades por
atualização", e precisa ser refletido no cronograma geral se for aprovado.

## Decisões em aberto, do Felipe

1. **Qual cidade primeiro.** Sugestão: Rio de Janeiro, a mais reconhecível no
   mundo, com monumentos antigos de fácil referência, como os Arcos da Lapa. A
   segunda, São Paulo, tem a vantagem de muitos cruzamentos densos, bons para o
   modo Crash.
2. **Cidade real ou inspirada.** Recomendação: inspirada, com poucos elementos
   reconhecíveis, por custo e por risco jurídico.
3. **Quando abrir o mundo.** Depois de duas ou três cidades brasileiras.
4. **Texto das cidades:** escrito por você, por mim com revisão sua, ou só
   dados verificados (nome, ano, fatos simples).

## Regras de progresso (decididas em 14 out 2026)

- Um cruzamento está **feito** quando tem medalha de bronze ou melhor.
- Os contadores mostram **cruzamentos feitos sobre o total**, por cidade, por
  região e no geral.
- Cada nível mostra também quantas medalhas de ouro tem.
- Uma cidade com `Playable = false` conta no mapa, mas não entra no total
  jogável e não pode ser selecionada. Aparece como "em breve".
- O mapa usa convenção de eixos do Unreal (Z para cima, X para a frente). A
  direção de uma cidade no globo é o vetor unitário
  `(cos lat · cos lon, cos lat · sin lon, sin lat)`.

## Decisão do Felipe sobre o que usar (14 out 2026)

- **Só o que é público**, nos termos da lei brasileira (Lei 9.610/98, art. 48):
  monumentos, praças, vias e obras situadas em logradouro público.
- **Nenhuma marca real**: nem logotipo, nem letreiro, nem fachada com marca. As
  marcas fictícias do jogo ocupam esse lugar.
- A **consulta a um advogado passou a ser opcional**. O Felipe aceita o risco
  com essas duas regras. Se uma loja ou um país reclamar de algum elemento, ele
  sai do jogo por atualização.

**Ressalva registrada, sem mudar a decisão:** a liberdade do art. 48 vale no
Brasil. Em outros países ela pode ser menor, principalmente para arquitetura
moderna de autor. A mitigação é a de antes: preferir monumentos antigos e
formas genéricas, não destruir nenhum ponto turístico e manter cada elemento
reconhecível fácil de remover. Quem lê isto no futuro deve saber que o risco
foi aceito, e não eliminado.

