# Caminho até a fatia vertical

Escrito em 14 out 2026. Depois do dano por zona (lote 17) e do polimento
visual (16b), o que falta para um cruzamento jogável e bonito do começo ao fim.
Cada lote segue as regras de sempre: lógica no Core com teste, dados em CSV,
efeito visual aprovado por captura no `-Play` e na cena de aparência, custo
medido no perfil alvo PS5.

Ferramentas: as rotinas em `.claude/skills/` (`jogar-e-capturar`,
`fechar-lote`, `modelo-3d`, `verificar-no-pc`) e o Unreal MCP (skill
`unreal-mcp`) para montar materiais, Niagara e cena com o editor aberto. O que
for montado pelo MCP precisa sair reproduzível: o script Python versionado
continua sendo a fonte, ou o passo a passo fica registrado no documento do lote.

## Lote 18: sensação de velocidade e câmera de batida

O que mais vende um jogo de batida é a câmera.

- **Velocidade:** campo de visão que abre com a velocidade (curva em
  `Cameras.csv`), tremor leve acima de ~150 km/h, desfoque de movimento só da
  câmera e moderado. Estudar o desfoque por pixel da engine para não borrar as
  rodas do próprio carro (o Felipe não quer roda borrada); se não der, deixar
  desligado e dizer.
- **Câmera de batida:** batida do jogador acima de um limiar de variação de
  velocidade (`CrashCamMinDeltaVMs`, inicial 15 m/s) dispara câmera lenta (escala
  de tempo descendo a 0,25 em 0,15 s, segurando ~1,5 s, voltando em 0,4 s) e uma
  câmera cinematográfica que orbita o ponto da batida, sem atravessar paredes.
- **Controle do destroço:** durante a câmera lenta o jogador empurra o carro
  para os lados (força lateral limitada, `WreckSteerForce`), para mirar o tráfego.
  Nome de trabalho próprio; não usar o nome do Burnout.
- Regra pura no Core: `FDNCrashCam` (quando dispara, curva da escala de tempo,
  quando termina) e `FDNSpeedFov` (campo de visão por velocidade), com teste.
- Aprovação: vídeo curto ou sequência de capturas da batida na parede a 100 km/h
  e contra o tráfego, mais o custo.

## Lote 19: efeitos de batida (Niagara)

- Faíscas no impacto e quando a carroceria raspa no chão ou na parede.
- Cacos de vidro e de plástico (farol) no impacto forte.
- Fumaça de pneu na derrapagem (ligada ao ângulo de deslize da assistência).
- Fumaça do motor quando a zona da frente passa de 60% de dano; fogo acima de
  90%, se o custo couber.
- Pool de efeitos e limite de partículas ativas; parâmetros em
  `Data/Effects.csv`. Ponto de partida: os exemplos gratuitos da Epic (Content
  Examples, licença só Unreal), registrados em `licencas.csv`.
- Aprovação: captura de cada efeito e medição com 6 carros batendo ao mesmo
  tempo no perfil alvo PS5.

## Lote 20: o primeiro cruzamento

- Mapa `L_Cruzamento01` criado por script: cruzamento de duas avenidas, 4
  aproximações com 2 ou 3 faixas, semáforos simples (ciclo fixo), calçadas,
  mobiliário da cena de aparência, rampa ou reta de largada para o jogador
  chegar a 120 km/h.
- Tráfego nas duas avenidas, respeitando o semáforo (carro em trilha para na
  linha de retenção), densidade em `Crossings.csv`.
- Rodada de Crash completa: largada, batida, câmera de batida, pontos, fim,
  medalha, recorde salvo. Limites de medalha calibrados aqui, com 10 rodadas
  por velocidade de entrada.
- Hora do dia e direção do sol escolhidas para o carro do jogador não virar
  silhueta na aproximação.
- Aprovação: o Felipe joga 10 rodadas e responde o roteiro curto de sensação.
- Notas da revisão do lote 16b (cena de aparência), para o cruzamento:
  - **Asfalto:** faixas pintadas e linha de retenção (nenhuma captura mostra
    faixa); asfalto mais escuro, com variação de aspereza; marcas de pneu,
    manchas de óleo, remendos e bocas de lobo. Fontes: CC0 do Poly Haven ou do
    ambientCG, ou decalques próprios.
  - **Fachadas:** os pilares laranja chapados ao fundo parecem provisórios; no
    cruzamento, prédios do City Sample.
  - **Vitrines:** a 900 nits não estouram, mas a "sala" é uma caixa cinza lisa;
    usar interior falso com cubemap (função de material de interior da engine)
    ou textura CC0.
  - **Folhas:** brilho pontilhado (especular alto) nos jacarandás; conferir o
    modelo de sombreamento de folhagem de duas faces, com especular mais baixo
    e aspereza mais alta.

## Lote 21: som base

Som pesa tanto quanto a imagem na sensação de qualidade. Seguir a skill
`som-livre` (licenças, tratamento com ffmpeg, registro).

- **Motor:** camadas por rotação (marcha lenta, média, alta) misturadas pela
  rotação do Chaos, mais carga (acelerador) e um "assobio" de turbo quando o
  turbo liga. MetaSounds (aprovado na proposta de recursos).
- **Batidas em três faixas** pela variação de velocidade (leve, média, forte),
  com variação aleatória de altura e uma camada de vidro e de metal raspando.
- **Pneus:** chiado ligado ao ângulo de deslize.
- **Ambiente:** cidade ao fundo, baixo.
- Fonte: Sonniss GDC (sem crédito; o WAV original fica no PC, fora do git, e o
  asset importado só entra no repositório público com o sim do Felipe) e
  Freesound/Kenney CC0. Parâmetros em `Data/Audio.csv`.
- Aprovação: o Felipe ouve jogando; custo de CPU medido.
