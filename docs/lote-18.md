# Lote 18: câmera de velocidade e de batida

Contrato: [`regras-camera.md`](regras-camera.md). Feito na sessão local em 14 e 15 out 2026. As regras puras e os dados entraram antes
(05f79fa e d5b6a17, testes `Detonauto.Camera.Fx` e `Detonauto.Dados.CameraFx`); este documento cobre o lado do motor e a aprovação.

## O que existe

- **Campo de visão** (`ADNVehicle::UpdateSpeedCamera`): `FDNSpeedFov` com a base do modo, `SpeedFovScale` do modo e o turbo. A constante
  `TurboFovKickDegrees` saiu; o turbo soma o `TurboFovAddDeg` do CSV. O valor mostrado anda até o alvo a 30 graus/s, em tempo real.
- **Tremor**: `FDNSpeedShake` gira a lente (arfagem e guinada) sobre o braço; o braço continua mirando o carro. `-DNNoShake` ou
  `DNShake 0` no console zeram o multiplicador global (a opção de acessibilidade, que o menu vai guardar no save).
- **Desfoque nas bordas**: material de pós-processamento próprio, `M_DNEdgeBlur` (`/Game/Detonauto/Camera`), feito por
  `Tools/Unreal/create_edge_blur_material.ps1`. Desfoque radial: cada pixel tira a média de 12 amostras da cor da cena no caminho até
  o centro da tela, com um deslocamento por pixel para não aparecer degrau. Força `EdgeBlurMax · S(v)`, zero dentro de
  `EdgeBlurInnerRadius` (fração da meia diagonal). Fica antes do bloom (depois do TSR, em cor linear): o TSR não desfaz o rastro e luz
  forte borra como luz. Não usa o vetor de movimento, então as rodas e o carro ficam nítidos. Abaixo de `EdgeBlurStartKmh` o peso do
  material vai a 0 e a engine nem roda o passe. `-DNNoEdgeBlur` ou `DNEdgeBlur 0` desligam.
- **Câmera de batida** (`TryCrashCam`, `UpdateCrashCam`, `CalcCamera`):
  - dispara **no contato**, sem esperar a janela de 0,15 s do dano, e vale também com o dano desligado. Peça de rua nunca dispara,
    nem a que ficou sólida abaixo do limiar dela; carro de tráfego conta como tráfego, o resto como parede;
  - escala de tempo por `SetGlobalTimeDilation`, só quando muda; o carro devolve 1 ao sair do jogo (`EndPlay`);
  - órbita em volta do carro, em coordenadas do mundo (ele sai girando). Começa `CrashCamLeadDeg` (90) antes do lado da batida e gira
    para ele a `CrashCamOrbitDegPerS`, para mostrar o amassado de três quartos. Dos dois lados possíveis fica o que tem mais espaço até
    a esfera bater; empate, o mais perto da câmera do modo;
  - a mistura com a câmera do modo é em **coordenadas polares** em volta do carro (ângulo, distância e altura). Sem corte, e a câmera
    faz um arco em vez de uma reta que atravessaria o carro;
  - parede: esfera de `CrashCamProbeCm` do carro até a posição. Bateu, a câmera fica antes do contato na hora; quando libera, volta a
    1,5 do caminho por segundo real, para passar atrás de um poste não dar tranco.
- **Controle do destroço**: A e D empurram o carro (`FDNWreckSteer`) no `Entering` e no `Holding`. O lado é a direita da tela **da câmera
  do jogador no instante da batida**. A primeira versão usava a da órbita, que fica ao lado do carro: a direita dela corre ao longo da
  velocidade, e o lado da perpendicular ficava indefinido (esquerda e direita andavam para o mesmo lado).
- **HUD de depuração**: linha nova com FOV, força do desfoque, fase da câmera de batida, escala de tempo e o tempo real desde o
  disparo. Fica escrita em cada captura.

## Ferramentas de teste

No `vehicle-probe.ps1 -Map /Game/Detonauto/Maps/L_Lookdev -AlvoPS5 -ResX 1920 -ResY 1080 -Keys ... -ExtraArgs ...`, sempre com
`-UseFixedTimeStep -FPS=60` (a captura trava o quadro):

- `-DNCrashShots=0,0.15,0.6,1.2,1.65,2.05`: captura com o HUD nesses segundos **reais** depois do disparo (`dn-crash-t<ms>`).
- `-DNCrashSteer=-1|1`: segura A ou D pelo caminho do teclado enquanto o controle vale.
- `-DNTrafficAheadM=M`: carro de tráfego parado, atravessado na faixa, já com física.
- `-DNBlockSideM=S` (com `-DNWallAheadM`): bloco de 15 m de altura encostado na barreira, a S metros da faixa (negativo: esquerda).
- `-DNSpeedShots=60,150,220`: captura com o HUD quando o carro passa de cada velocidade (`dn-speed-<v>kmh`).
- `-DNEdgeBlurForce=S`: desfoque com força fixa, mesmo parado (medição de custo).
- `-DNNoEdgeBlur`, `-DNNoShake`, `-DNNoCrashCam`.

## Aprovação (os 8 itens do contrato)

1. **Parede a 100 km/h, sol por trás** (`-DNCruiseKmh=100 -DNWallAheadM=120 -DNSunYaw=180`): dispara com 34,6 m/s a 102 km/h. Fases
   no log em tempo real: segurando em 0,15 s, saindo em 1,65 s, espera em 2,05 s, parada em 4,05 s. Escala no quadro: 1,00 / 0,25 /
   0,25 / 0,25 / 0,25 / 1,00. Sequência: [`parede-sequencia.jpg`](img/camera-18/parede-sequencia.jpg). Em 0,15 s a câmera já está
   ao lado do carro, com a frente amassada contra a barreira; em 2,05 s volta para a perseguição.
2. **Tráfego a 100 km/h** (`-DNTrafficAheadM=120`): dispara com 18,2 m/s (tráfego).
   [`trafego-sequencia.jpg`](img/camera-18/trafego-sequencia.jpg). Em 0,87 s a esfera bateu num poste intacto da cena e a câmera
   chegou perto do carro aos poucos (84% → 74% do caminho), depois voltou. O carro de tráfego ainda é a caixa provisória.
3. **Mobiliário a 100 km/h** (`-DNPropLine=poste,arvore,banco,lixeira`): 4 de 4 quebradas, nenhuma linha `DNCrashCam` no log, cada
   batida diz "câmera de batida: não dispara", e a fileira deu APROVADO.
4. **Perto do prédio** (`-DNBlockSideM=-2.5`): o lado do bloco tinha 35% de espaço, o outro 87%; a órbita foi para o lado livre.
   [`predio-sequencia.jpg`](img/camera-18/predio-sequencia.jpg). A câmera nunca entra no bloco. Antes da escolha do lado, ela ficava
   espremida entre o carro e o bloco, a ~1 m da porta.
5. **Velocidade** na alta-longe: FOV 95,0 / 100,7 / 103,0 e desfoque 0 / 0,13 / 0,45 a 60 / 150 / 220 km/h.
   [`velocidade-com-desfoque.jpg`](img/camera-18/velocidade-com-desfoque.jpg),
   [`velocidade-sem-desfoque.jpg`](img/camera-18/velocidade-sem-desfoque.jpg) e, lado a lado a 220 km/h,
   [`220kmh-com-x-sem-desfoque.jpg`](img/camera-18/220kmh-com-x-sem-desfoque.jpg). Bordas borradas em raio (banco, prédios, asfalto
   perto da câmera), carro e rodas nítidos, HUD intacto.
6. **Controle do destroço**, a mesma batida na parede: sem tecla, o carro anda +0,25 m para a direita da tela; segurando A, −1,24 m;
   segurando D, +1,28 m. Usou 6,00 de 6,00 m/s nas duas. [`destroco-esquerda.jpg`](img/camera-18/destroco-esquerda.jpg) e
   [`destroco-direita.jpg`](img/camera-18/destroco-direita.jpg).
7. **Custo** no `DN_AlvoPS5`, 1920x1080: o passe `PostProcessMaterial 1920x1080 Material=M_DNEdgeBlur` medido por `profilegpu` com a
   força máxima do CSV (0,5) fixada parado: **0,038 ms de mediana, 0,041 ms no pior de 12 amostras**. O resto da câmera é uma consulta
   de esfera por quadro durante a câmera lenta. Medição do quadro inteiro (média e p95, com e sem) pendente: o TDFende ficou aberto em
   todas as tentativas e o `lookdev-measure` descartou (ver `docs/desempenho.md`).
8. **O Felipe joga** e diz se a velocidade e a batida "vendem": pendente.

Suíte: 677 de 677.

## O que mudou no contrato

- `CrashCamLeadDeg` (90, faixa [0, 180]) entrou em `Data/CameraFx.csv`: onde a órbita começa. Com a órbita começando atrás do carro,
  onde a câmera do modo está, ela girava ~50 graus em 2 s e nunca mostrava a frente amassada numa batida de frente.
- A direita da tela do controle do destroço é a da câmera do jogador no instante da batida (ver acima).
- A câmera de batida dispara no contato, sem esperar o agrupamento de 0,15 s do dano.

## Limites conhecidos

- O piloto automático das ferramentas (`-DNCruiseKmh`) só solta o acelerador quando a janela do dano fecha (0,15 s do jogo, 0,6 s reais
  na câmera lenta): nas capturas o carro ainda empurra a parede um pouco.
- A órbita gira no máximo `CrashCamOrbitDegPerS` × 2,05 s ≈ 51 graus. Batida pelo lado já começa perto do amassado; de frente, o
  começo a 90 graus mostra o lado com a frente.
