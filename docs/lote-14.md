# Lote 14: o que o playtest 1 achou, a causa de cada um e o que mudou

Fonte: [`notas-playtest-1.md`](notas-playtest-1.md). Tudo abaixo foi reproduzido **jogando** (janela com a entrada de teclado entrando
pelo `PlayerController`, o mesmo caminho do teclado do Felipe; roteiro `-DNKeys` do `vehicle-probe.ps1`), com captura de tela, e não só
por cenário automático. Os números vêm de `Tools/Unreal/vehicle-probe.ps1` (comandos no fim).

## B1. Rodas de lado e "embaçadas"

- **Causa (de lado):** o Chaos entrega giro (pitch), esterçamento (yaw) e curso da suspensão no espaço do **componente**; o `FAnimNode_WheelController`
  do template faz `rotação nova = RotOffset x rotação do osso`, tudo no espaço do componente. O `Tick` do lote 12 aplicava o giro no espaço do
  **osso**. Como o osso `Phys_Wheel_*` não tem o eixo da roda em Y, a roda girava em torno do eixo errado (a roda ficava deitada). Ao abrir, parada,
  parecia certa; em movimento, tombava.
- **Correção:** `Core/DNWheelVisual.h` (puro, com teste): rotação relativa ao osso = `Osso^-1 x Giro x Osso`; o curso da suspensão é levado ao espaço do
  osso. Os testes provam que, com osso girado em qualquer orientação, o giro é uma rotação em torno do eixo Y do componente e o esterço em torno do Z.
- **Causa (borrão):** **desfoque de movimento do pós-processo** (o padrão da engine). Visível na captura em movimento: o chão e a roda aparecem riscados.
  A roda não tem troca de malha por LOD nem material especial.
- **Correção do borrão:** a câmera do carro desliga o desfoque (`MotionBlurAmount = 0` e `MotionBlurMax = 0` nas configurações de pós-processamento da
  câmera, em código; nenhum `.ini` foi tocado).
- **Prova:** capturas em movimento (acelerando a ~100 km/h, câmera lateral): rodas de frente, nítidas; com `D` apertado a roda da frente esterça.

## B2. Amassado "não aparece"

- **O que o log do playtest mostra:** as teclas T/Y/U **chegavam** e o dano rodava (`DNDent: batida de 10,0 m/s ... amassado novo ... 1 amassado(s) guardado(s)`).
  Ou seja, a lógica e o material estavam ligados no `-Play`; o problema era o que se via.
- **Causa 1:** a câmera de perseguição fica atrás do carro (a ~8 m, a ~1,9 m de altura): ela vê a traseira, o teto e o aerofólio, **nunca a frente nem o lado
  esquerdo**, que são onde as teclas batiam.
- **Causa 2:** o ponto da batida de T/Y/U era o meio da frente da caixa da carroceria (`+1,0` da extensão em X). A malha ali é **a grade, aberta, sem
  vértices**: o amassado desloca vértices, então não deformava nada. Medido: o centro da frente (0,7 a 1,0 da extensão) não deforma; a quina
  da frente (0,8 / -0,8) deforma.
- **Correção:** T/Y/U amassam a **quina da frente à esquerda**; G/H/J a porta esquerda; e a câmera **gira 3 s** para a frente-esquerda para o amassado
  aparecer, e depois volta (V limpa e também mostra).
- **Prova:** captura jogando (teclas reais T, Y, U): HUD `Amassados: 1 de 8` e a quina da frente visivelmente amassada; depois G: `2 de 8`.
- **Limite que continua:** uma batida **real** de frente cai no meio da frente (a caixa não é a superfície). O amassado de 55 cm de raio ainda pega os
  vértices ao redor, mas é menos marcante que na quina. Melhoria possível: juntar o ponto da batida ao vértice mais próximo da malha (ver "Anotado para depois").
- O `M_DNDentParent` existe e carrega no projeto principal (`Content/Detonauto/Vehicle`, criado por `create_dent_material.ps1`); `DNDent: carroceria
  'SM_SportsCar': 9724 vertices, 2 slot(s) de material` no log.

## B3. "Curva dura" nos presets 0 e 1

Medi o raio de curva em regime (entra na velocidade-alvo, segura 6 s com o esterço mantido, mede os 3 s finais) no **mesmo piso de atrito 1,0**, para o
carro do template (`BP_SportsCar_Pawn`, lido do Blueprint e rodado na sonda `DNSteerProbeGameMode`, só no Sandbox) e para o `Padrao` do projeto:

| Velocidade | Esterço | Raio do template | Raio do `Padrao` | Guinada do template | Guinada do `Padrao` |
|---|---|---|---|---|---|
| 40 km/h | total | 8,1 m | 6,0 m | 77 graus/s | 106 graus/s |
| 40 km/h | metade | 27,5 m | 24,1 m | 25 graus/s | 29 graus/s |
| 80 km/h | total | 26,2 m | 21,2 m | 47 graus/s | 57 graus/s |
| 80 km/h | metade | 42,2 m | 32,4 m | 30 graus/s | 39 graus/s |

- **O nosso carro vira MAIS fechado que o do template em toda a tabela (de 12% a 26% de raio a menos), e não mais aberto.** "Curva dura" não é falta de
  esterço. Aceleração lateral medida a 80 km/h com esterço total: 2,15 g no `Padrao` e 2,7 g no `arcade-1`: grip enorme (atrito de roda 3,0 e 4,0 sobre o
  piso 1,0).
- **O que o Blueprint do template tem e o código do protótipo não tinha** (lido do `BP_SportsCar_Pawn` por C++): curva de torque diferente
  `(1000,0.5) (1800,0.9) (3500,1,0) (6000,0,8) (7000,0,0)` (a do protótipo era `(0,0.6) (2000,0.9) (4500,1.0) (7000,0.7)`) e `MaxBrakeTorque` traseiro
  **1500** (o do protótipo era 4500). **Os dois foram alinhados ao template** neste lote (afetam aceleração e frenagem; o raio de curva não mudou).
  Todo o resto lido é igual: curva de esterço `(0,1) (20,0.8) (60,0.4) (120,0.3)`, taxas de entrada (esterço sobe 2,5/s e cai 5/s), massa, COM, tensor de
  inércia, rodas (raio, largura, atrito, mola, amortecimento, barra), diferencial traseiro, câmbio.
- **Causa da diferença de raio: não achada.** Com tudo igual no que consegui ler, o carro do template ainda sai mais aberto (subesterça mais). O que
  eu não consegui ler do Blueprint (propriedades que o Python da engine não expõe) pode ser a causa. **Não chutei.**
- **Proposta (para decisão, não aplicada):** o que muda a sensação com mais certeza, e que dá para medir, é (1) **reduzir o esterço total em alta
  velocidade** (curva de esterço mais fechada que `0,4` a 60 km/h e `0,3` a 120, hoje do template) para o carro não passar de ~1,2 g; (2) subir o tempo de
  subida do esterço (2,5/s para ~1,8/s) para o teclado, que é tudo ou nada, não "chicotear". Os dois são colunas novas de `Cars.csv` (por preset) e dão
  para medir com o `-Scenario Steer`. Esperar a resposta do Felipe sobre o que ele chama de "dura" (volante pesado, demora a virar, ou vira demais).

## J1. arcade-2 derrapava sozinho

- **Medido** (curva SEM freio de mão, acelerador no fundo, sem assistência): com a aderência traseira de 0,85, o `arcade-2` deslizava **22 a 23 graus**
  (60 km/h com esterço total; 100 km/h com 0,6) contra 4,5 a 5,3 do `arcade-1`. Era a aderência traseira baixa fora do freio de mão.
- **Correção:** `RearGripFactor` do `arcade-2` = 1,0 (igual ao `arcade-1`) e `HandbrakeRearGrip` 0,30 (produto 0,30, acima do mínimo 0,295). Agora a curva com o
  pé no fundo desliza 4,5 a 5,3 graus, e o `arcade-2` só derrapa de verdade com o Espaço.

## J2. Assistência de derrapagem (arcade-3 e arcade-2)

- **Regra pura:** `Core/DNDriftAssist.h` (`FDNDriftAssist::YawAcceleration`, `SlipAngleDeg`), com teste: acima do ângulo de início, uma aceleração de guinada
  gira o nariz para a direção da velocidade, `ganho x excesso`, com teto; zero abaixo de 10 km/h, sobe linear até 30 km/h; reduz se o carro já gira
  para o lado certo (a 120 graus/s ou mais não empurra). Parâmetros em `Cars.csv`: `DriftAssistStartDeg`, `DriftAssistGain`, `DriftAssistMax`
  (ganho e teto zero = desligada; o validador exige o par).
- **Valores:** `arcade-2` e `arcade-3`: início 25 graus, ganho 50 graus/s² por grau, teto 1500 graus/s². `Padrao` e `arcade-1`: desligada.
- **Medido** (freio de mão apertado 0,8 s numa curva; ângulo de deslize em toda a janela, de -180 a 180; recuperou = deslize < 10 graus e guinada < 40 graus/s):

| Preset | Entrada | Sem assistência | Com assistência 25/50/1500 |
|---|---|---|---|
| `arcade-2` | 55 km/h, esterço 0,5 | 21 graus, recupera em 0,69 s | 21,5 graus, 0,69 s (não age: abaixo do início) |
| `arcade-3` | 55 km/h, esterço 0,5 | 17 graus, 0,67 s | 18 graus, 0,70 s (não age) |
| `arcade-2` | 100 km/h, esterço total | **180 graus (roda)**, não recupera | **54 graus**, recupera em 0,43 s |
| `arcade-3` | 100 km/h, esterço total | **180 graus (roda)**, não recupera | **52 graus**, recupera em 0,44 s |
| `arcade-2` | 130 km/h, esterço total | roda | 54 graus, 0,47 s |
| `arcade-3` | 130 km/h, esterço total | roda | 52 graus, 0,48 s |

  Os números testados à parte: início 20 / ganho 100 / teto 2000 segura em 36 graus (recupera em 0,18 s, "grudado"); início 30 / ganho 40 / teto 1500
  deixa 64 a 67 graus. Escolhi 25/50/1500: derrapa até uns 50 graus antes de corrigir e **nunca** chega perto de 90.
- **Só no chão (revisão do lote 14):** a assistência só age com 3 rodas ou mais em contato (`FDNDriftAssist::IsGrounded`), para não girar o carro
  no ar ou capotado depois de uma batida. Medido de novo depois da mudança: `arcade-3` 52,2 graus e recupera em 0,44 s; `arcade-2` 54,3 graus e 0,43 s
  (iguais aos de antes). Mutação: 3 de 3 mortos.
- A assistência liga por padrão; `-DNNoAssist` desliga (medição de comparação). Quem sente é o Felipe: se parecer "grudado", subir o início ou baixar o teto.

## J3. Turbo "quase não dá diferença"

- **Medido** (`-Scenario Boost`: segura 60 km/h, depois 3 s com o acelerador a fundo; com e sem turbo): `Padrao` 133 para **211** km/h (**+59%**), `arcade-1` 147
  para **241** (**+64%**), `arcade-3` 138 para **234** (**+70%**), entrando a 62 km/h. A meta pedida era +40 a +60% de velocidade final: **já passa
  disso com a `NitroAccelMps2` atual (10 a 12 m/s²)**; não aumentei o número. Pelo teclado real (Shift pelo `PlayerController`): HUD `ATIVO`, barra 100 para 50 em 2,6 s.
- **Não consegui reproduzir "quase não dá diferença".** Hipóteses: a diferença é grande no número, mas a **tela não mostra**: não havia efeito visual. Mudança:
  o turbo abre o **campo de visão em 12 graus** (sobe e desce suave) enquanto está ativo. Se o Felipe ainda achar fraco, o número a mexer é `NitroAccelMps2` (até 40).
- A barra gasta 25 por segundo (4 s, mais 2 s do turbo prolongado do Core); no modo de teste ela enche de novo 2 s depois de esvaziar.

## J4. Câmeras

- **Dados:** `Data/Cameras.csv` (um modo por linha, na ordem da tecla **C**): `Id, Anchor (Chase|Car), DistanceCm, HeightCm, ForwardCm, SideCm, PitchDeg, Fov,
  Smoothing`. Validador (faixas e a linha obrigatória `alta-longe`), deriva (a linha `alta-longe` do CSV = padrão da struct `FDNCameraConfig`), `ToConfig`
  testado, subsistema `GetCameraModes()` (tabela inválida: só o modo padrão).

| Tecla C, na ordem | Âncora | Números | O que é |
|---|---|---|---|
| `alta-longe` (padrão) | perseguição | 810 cm atrás, 185 cm acima, FOV 95, suavização 2 | a dos presets 1, 2 e 3 do playtest (a que ele preferiu) |
| `alta-muito-longe` | perseguição | 1300 cm, 420 cm, inclinada 10 graus para baixo, FOV 90 | vê o carro e a pista de longe |
| `capo` | presa ao carro | à frente +100, altura 112, FOV 100 | câmera sobre o capô |
| `frente-para-choque` | presa ao carro | à frente +245, altura 45, FOV 100 | câmera no para-choque, baixa |
| `motorista` | presa ao carro | à frente -5, 35 cm à esquerda, altura 95, FOV 95 | olhos do motorista (volante à esquerda) |

- As colunas de câmera **saíram do `Cars.csv`** (distância, altura e FOV eram por preset e agora a câmera é independente do preset).
- Os modos presos ao carro acompanham a carroceria inteira (inclinam e rolam com ela).
- **O `SK_SportsCar` não tem interior.** A vista do motorista **enxerga através da carroceria** (casca de face única: de dentro, o capô some e aparece o
  chassi, o conjunto da suspensão e as rodas). Não inventei malha. Ver "Anotado para depois".

## Prova por mutação

Filtro `Detonauto.Carro+Detonauto.Dados+Detonauto.Camera+Detonauto.Subsistemas` (199 testes). Cada mutante: troca o texto, compila, roda, restaura.

- **Código (43 mutantes):** 41 mortos. 2 equivalentes, documentados: a assistência começar no ângulo exato (`>=` no lugar de `>`, o excesso é zero ali
  de qualquer jeito) e tirar a guarda de velocidade zero no ângulo de deslize (`atan2(0, 0)` já devolve 0).
- **Dados (14 mutantes em `Cars.csv` e `Cameras.csv`):** 14 mortos (deriva do `Padrao` e da `alta-longe`, ordem dos modos, faixas, presets do J1 e J2).
- **Subsistema (2 mutantes: sem o modo de reserva; validade nunca calculada):** 2 mortos. Na primeira rodada os dois derrubavam o runner (o teste
  indexava a lista sem conferir o tamanho); o teste agora confere o tamanho antes de indexar.
- Controle antes e depois, sem mutação: 199 de 199 aprovados.
- **Armadilha:** mutante de CSV não recompila; se rodar logo depois de um mutante de código, roda contra o binário mutante e o resultado não vale.
  A rodada válida compila o código limpo antes dos mutantes de CSV e no fim.

## Anotado para depois (o que não deu para fazer)

1. **Interior do carro** (volante e painel): o modelo do template não tem. Precisa de uma malha de interior (Meshy gratuito com crédito, ou criada) e de um
   material de face dupla; hoje a câmera `motorista` existe e funciona, mas mostra o chassi por dentro.
2. **Causa do raio de curva maior do template** (B3): não achada; ver proposta acima. Ler as propriedades do Blueprint que o Python não expõe pede C++ no Sandbox
   para cada candidato (o que já fiz para curva, taxas, rodas, motor, câmbio, corpo).
3. **Amassado real no meio da frente**: juntar o ponto da batida ao vértice mais próximo da malha da carroceria (ler o buffer de posições da LOD 0) ou
   usar uma malha de colisão simplificada como proxy para achar a superfície.
4. **Lote 13**: a medição do Crash (`Tools/Unreal/crash-measure.ps1`: batidas a 100 e 130 km/h contra 20 e 40 carros, e o custo em Game/Draw/GPU com
   40 carros em trilhos contra todos em Physics) e as mutações dos pontos de risco do tráfego **ainda não rodaram**: parei o 13 no que estava verde
   (commit `ac0cfc3`) para fazer o 14.
5. **Câmera `alta-muito-longe` a mais de 200 km/h**: numa captura a 200+ km/h logo depois de trocar de modo o carro não apareceu no quadro; a 72 km/h e parado aparece
   (verificado). Conferir jogando a velocidade máxima.
6. **Piso de teste**: o `WorldGridMaterial` aparece ruidoso em capturas pequenas (cosmético).

## Comandos usados (todos com `-Uproject Detonauto.uproject`)

```powershell
# roteiro de teclas REAIS + captura, no mapa de estrada (sem parede), sem tráfego
.\Tools\Unreal\vehicle-probe.ps1 -Uproject Detonauto.uproject -GameMode DNCrashTestGameMode -ExtraArgs '-DNTraffic=0' -Row arcade-3 -Keys "W@0.4-9;CAM90@5.5;SHOT@7.5;QUIT@9"
.\Tools\Unreal\vehicle-probe.ps1 -Uproject Detonauto.uproject -GameMode DNCrashTestGameMode -ExtraArgs '-DNTraffic=0' -Keys "T@1.5;Y@2.0;U@2.5;SHOT@3.6-1;QUIT@4.5"
# raio de curva em regime
.\Tools\Unreal\vehicle-probe.ps1 -Uproject Detonauto.uproject -GameMode DNCrashTestGameMode -ExtraArgs '-DNTraffic=0' -Row Padrao -Scenario Steer -WallKmh 80 -Steer 0.5
# freio de mão / curva com o pé no fundo / assistência ligada ou desligada
.\Tools\Unreal\vehicle-probe.ps1 -Uproject Detonauto.uproject -GameMode DNCrashTestGameMode -ExtraArgs '-DNTraffic=0','-DNNoAssist' -Row arcade-3 -Scenario Handbrake -HbKmh 100 -Steer 1.0
.\Tools\Unreal\vehicle-probe.ps1 -Uproject Detonauto.uproject -GameMode DNCrashTestGameMode -ExtraArgs '-DNTraffic=0' -Row arcade-2 -Scenario Turn -HbKmh 60 -Steer 1.0 -Throttle 1.0
# turbo: 3 s a fundo a partir de 60 km/h, com e sem
.\Tools\Unreal\vehicle-probe.ps1 -Uproject Detonauto.uproject -GameMode DNCrashTestGameMode -ExtraArgs '-DNTraffic=0' -Row arcade-1 -Scenario Boost -TurboOn
```
