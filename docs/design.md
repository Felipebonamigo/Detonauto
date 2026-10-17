# Detonauto: documento de design

Versão inicial, outubro de 2026. Atualizar ao fim de cada checkpoint.

## Visão

Detonauto é um jogo de corrida arcade em que dirigir perigosamente é a única
forma de ganhar turbo, derrubar o rival é recompensa e a batida é espetáculo.
A referência de sensação é Burnout 3: Detona, com gráficos realistas e
destruição como diferencial. O modo Crash é o coração do jogo.

## Pilares

1. **Turbo ganho por risco.** A barra só enche na contramão, passando raspando
   no tráfego, derrapando e no ar.
2. **Detona como objetivo.** Jogar o rival na parede ou no tráfego recarrega
   o turbo, dispara câmera lenta e mostra um replay curto.
3. **A batida é espetáculo.** Depois de bater, o tempo desacelera, o jogador
   controla os destroços e pode explodir o carro para atingir o que estiver em
   volta.

Toda funcionalidade nova precisa reforçar pelo menos um pilar.

## Plataformas e metas técnicas

- PC na Steam primeiro, em Acesso Antecipado. PS5 depois da versão 1.0.
- 60 fps num PC com placa de vídeo no nível do PS5.
- Teste técnico do protótipo: 6 carros com dano e 40 carros de tráfego a 60 fps.
- Steam Deck jogável.

## Mecânicas

### Direção

Arcade, não simulação. Aderência alta, derrapagem controlada e sensação forte
de velocidade com campo de visão, desfoque e tremor de câmera. Um leve ímã
ajuda a encostar no rival sem exigir precisão milimétrica. Base no Chaos
Vehicles com uma camada própria de controle por cima.

### Turbo

| Ação | Efeito |
|---|---|
| Contramão | carga contínua |
| Passar raspando no tráfego | carga por evento |
| Derrapar | carga contínua |
| Salto | carga por tempo no ar |
| Detona | barra cheia |

Usar a barra inteira sem soltar concede um bônus de barra extra.

### Detona

Acontece quando o rival bate por causa do jogador. Tipos previstos: lateral,
por trás, em T, por cima e com os destroços depois da própria batida. Cada
detona dá turbo, câmera dedicada e entra na contagem do evento.

### Batida

Batidas acima de um limite de força ativam câmera lenta e uma câmera
cinematográfica. Durante a câmera lenta, o jogador empurra o carro no ar para
atingir rivais ou tráfego. A explosão sob comando aplica um impulso radial com
dano pela distância e pode causar explosões em cadeia.

### Dano

Deformação da carroceria no ponto de impacto e peças que se soltam, como capô,
portas, para-choques e vidros. As peças soltas viram corpos físicos com tempo
de vida e reaproveitamento. A técnica exata será decidida pelos números do
teste técnico no protótipo.

### Tráfego

Carros em trilhas com reaproveitamento, semáforos simples e reação a batidas.
Cada veículo tem um valor em dinheiro para a pontuação do modo Crash. Não
precisa de IA complexa: precisa parecer vivo e explodir bem.

## Modos

| Modo | Entra em | Resumo |
|---|---|---|
| Crash | Acesso Antecipado | Cruzamento curto, uma largada, pontuação pelo valor destruído, multiplicadores no chão e medalhas de bronze, prata e ouro |
| Road Rage | Atualização 2 | Fazer o máximo de detonas antes que o carro aguente o limite de dano |
| Corrida | Atualização 3 | Rotas pelo distrito com rivais e tráfego. Detona dá turbo, bater custa segundos |

Os nomes Road Rage e os termos da batida são de trabalho, herdados do Burnout.
Os nomes finais serão próprios.

### Modo de arremesso (ideia do Felipe, 14 out 2026)

Nome de trabalho: **Arremesso**. Não usar o nome de nenhum jogo existente,
nem as provas, os personagens ou o visual deles. A mecânica em si (lançar o
piloto do carro e pontuar pelo voo) é genérica.

- O jogador acelera numa pista curta e, numa linha de largada, o carro para ou
  desacelera e o **piloto é lançado** pelo para-brisa, com a força definida
  pela velocidade e pelo ângulo no momento da largada.
- O piloto é um boneco de física (ragdoll) que voa, quica e rola. Pontua pelo
  resultado: distância, altura ou acerto num alvo.
- Provas de exemplo, todas nossas e com nomes próprios: salto em distância,
  salto em altura, alvos circulares, aros de fogo, pinos. Cada prova é um
  "cruzamento" curto, com medalhas de bronze, prata e ouro como no Crash.
- Reaproveita o que já existe: carro, tráfego, pontuação, medalhas, progresso
  e dados em CSV. Novo: boneco de física, câmera de voo, alvos.

Fica **depois** do Acesso Antecipado (entra como atualização, junto ou depois
do Road Rage). Não entra no cronograma atual até o Crash estar jogável. Custo
técnico: um boneco de física por rodada é barato perto do tráfego, mas precisa
de personagem (o manequim gratuito da Epic serve para protótipo; conferir a
licença antes de qualquer uso final), esqueleto e animação de lançamento.
Regra pura prevista (Core): pontuação por distância, altura e alvo, com
limites de medalha em CSV, no mesmo padrão dos outros contratos.

## Conteúdo por lançamento

| Lançamento | Data prevista | Conteúdo |
|---|---|---|
| Acesso Antecipado | nov 2028 | 20 cruzamentos, 6 carros, 1 distrito, rankings da Steam, português, inglês e espanhol |
| Versão 1.0 | nov 2029 | 30 cruzamentos, Road Rage, corrida, 2 distritos, 10 carros, mais 4 idiomas |
| PS5 | jul 2030 | Versão 1.0 com troféus e DualSense completo |

### Ideias inspiradas no Wreckfest (Felipe, 14 out 2026)

Só mecânicas gerais. Nada de nomes, veículos, pistas ou visual do jogo de
referência: tudo fictício e feito por nós.

| Ideia | O que muda no Detonauto | Quando | Custo e risco |
|---|---|---|---|
| Dano que afeta o carro | O dano deixa de ser só visual: roda torta puxa a direção, motor danificado perde força, capô amassado atrapalha a visão. Usa as zonas e peças de `docs/regras-dano.md` | Primeiro o visual (já no plano); a consequência na direção na Versão 1.0 | Médio. Regra pura testável; a sensação precisa de playtest |
| Arena de destruição | Modo de arena fechada com vários carros até sobrar um, aproveitando dano e tráfego em trilha | Junto do Road Rage (Atualização 2) | Alto. Pede IA de rivais e muitos carros com física; medir antes |
| Veículos improvisados | Carros velhos e estranhos, vans, ônibus, até um trator, todos fictícios. A tabela `TrafficVehicles.csv` já tem classes variadas | Alguns já no Acesso Antecipado como tráfego; jogáveis depois | Baixo a médio. Arte e ajuste de física por veículo |
| Pistas sujas e ovais | Terra, lama e pista de bate-bate, com carros pesados | Versão 1.0, depois do segundo distrito | Alto. Exige tipos de superfície na física e pista própria |

**Regra de escopo:** nada disso entra antes de o Crash estar jogável e do
checkpoint G1 (teste de dano a 60 fps). O plano atual já é grande para uma
pessoa; cada ideia nova só entra tirando outra ou empurrando uma data.

Fora do plano: online em tempo real, tela dividida e carros licenciados.

## Carros

Carros fictícios em 4 classes: compacto, muscle, cupê e super. A base são os
13 veículos gratuitos do City Sample, modificados no Blender com carroceria,
grade, faróis e marca próprios. As marcas fictícias são desenhadas em SVG.

## Mundo

Um distrito urbano denso, montado com os prédios do City Sample personalizados
e materiais do Poly Haven e do ambientCG. O mesmo distrito serve a todos os
cruzamentos. Um segundo distrito entra na versão 1.0.

## Áudio

Motores e impactos dos pacotes GDC da Sonniss e de sons CC0 ou CC BY do
Freesound. Música de bandas independentes, com acordo por escrito.

## Checkpoints

| Código | Data | Critério principal |
|---|---|---|
| G1 | fim de ago 2027 | 10 pessoas de fora querem jogar de novo, e o teste de dano roda a 60 fps |
| G2 | fim de fev 2028 | 5 cruzamentos e 2 carros em qualidade final, trailer pronto |
| AA | nov 2028 | 20 cruzamentos, 6 carros e lista de desejos na casa de 7 mil |
| 1.0 | nov 2029 | Todo o conteúdo da versão 1.0 e avaliações positivas |
| PS5 | jul 2030 | Certificação da Sony aprovada |

## Perguntas em aberto

- Nomes finais dos modos e dos termos da batida.
- Técnica de dano, decidida no teste técnico das Sprints 12 e 13.
- Hora do dia e paleta, decididas na bíblia de arte da Sprint 22.
- Bandas da trilha sonora.
- Resultado da busca de Detonauto no INPI, nas classes 9 e 41.
