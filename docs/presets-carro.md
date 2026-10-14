# Presets do carro de teste (lote 8, afinação arcade)

> **Desde o lote 12 o carro vive no projeto principal** (`Source/Detonauto/Vehicle`) e os presets em `Data/Cars.csv`; veja
> [`carro.md`](carro.md) para jogar e medir lá (`Tools/Unreal/vehicle-probe.ps1`). Este documento continua valendo para o
> **significado de cada coluna**, o que cada preset muda e o roteiro de teste; os comandos abaixo (`TestCars.csv`, `car-probe.ps1`,
> `DNTestCarGameMode`) são os do projeto descartável, mantido como referência. No projeto principal o nitro de teste virou o turbo do Core.

Resposta ao que o Felipe sentiu no carro do template ([notas da Sprint 2](notas-sprint-2.md)): duro, pesado, parece
simulador, não derrapa (freio de mão "trava"), câmera perto demais, sem nitro. Este lote **não mexe no Core nem no
projeto principal**: tudo vive no projeto descartável `Sandbox/VeiculoTeste` (fora do Git), com cópia de referência em
[`Tools/Unreal/veiculo-teste/`](../Tools/Unreal/veiculo-teste/README.md). Dano e deformação não entram aqui.

**Quem escreveu isto não consegue sentir o carro.** O que está marcado como "medido" saiu de execuções sem intervenção
(números abaixo, com o comando para repetir). O que está marcado como "esperado" é hipótese a confirmar jogando: se o
Felipe sentir outra coisa, o que vale é o que ele sentiu, e o ajuste é trocar números em `Data/TestCars.csv`.

## Como usar

1. Feche o editor do Unreal e compile: `Tools/Unreal/build.ps1 -Uproject Sandbox\VeiculoTeste\VeiculoTeste.uproject`
   (com o editor aberto a DLL fica travada e o build falha).
2. Abra o `VeiculoTeste.uproject`, mapa `VehicleBasic`, World Settings, **GameMode Override = `DNTestCarGameMode`**, Alt+P.
3. Teclas durante o jogo:

| Tecla | Efeito |
|---|---|
| W, S, A, D | Acelerar, frear/ré, esterçar (como antes) |
| Espaço | Freio de mão |
| **Shift** | **Nitro de teste** (segure) |
| **1, 2, 3** | Troca para o preset `arcade-1`, `arcade-2`, `arcade-3` **andando** |
| **0** | Volta para `Padrao` (o carro do template, para comparar) |

4. O HUD de debug no canto superior esquerdo mostra o **nome do preset**, a **velocidade em km/h** e o **estado do nitro**
   (`pronto`, `ATIVO`, `recarregando` ou `vazio`, com a barra).
5. Para mudar um número, edite `Data\TestCars.csv`, salve e **jogue de novo** (o CSV é lido ao trocar de preset e ao nascer
   o carro; não precisa recompilar). Linha com número inválido é recusada no log e o carro fica como estava.

Ao trocar de preset andando, o jogo recria o estado físico do veículo: a velocidade se mantém, mas o câmbio volta ao ponto
morto e engata a 1ª de novo em uns 0,2 s (medido).

## O que cada coluna do CSV faz

| Coluna | Efeito | Parâmetro do Chaos 5.8 |
|---|---|---|
| `MassKg` | Massa do chassi | `UChaosVehicleMovementComponent::Mass` |
| `MaxTorque` | Força do motor | `EngineSetup.MaxTorque` |
| `LateralGrip` | Aderência lateral, nas 4 rodas (1 = padrão) | `SetWheelSlipGraphMultiplier` (`LateralSlipGraphMultiplier`) |
| `SuspensionStiffness` | Rigidez da mola | `UChaosVehicleWheel::SpringRate` |
| `SteeringGain` | Ganho do volante (1 = padrão) | aplicado na entrada, antes do Chaos |
| `CenterOfMassDropCm` | Quanto o centro de massa desce | `UPrimitiveComponent::SetCenterOfMass` (`COMNudge`) |
| `SuspensionDampingRatio` | Amortecimento (0 a 1) | `SuspensionDampingRatio` |
| `SuspensionTravelCm` | Curso da suspensão, para cima e para baixo | `SuspensionMaxRaise` e `SuspensionMaxDrop` |
| `RollbarScaling` | Barra estabilizadora (menor = a carroceria rola mais) | `RollbarScaling` |
| `RearGripFactor` | Aderência do eixo traseiro em relação ao padrão | `FrictionForceMultiplier` da roda traseira e `SetWheelSlipGraphMultiplier` |
| `HandbrakeRearGrip` | Quanto da aderência traseira sobra com o freio de mão apertado (1 = nada muda) | rampa no carro, via `SetWheelFrictionMultiplier` e `SetWheelSlipGraphMultiplier` |
| `HandbrakeRampInSeconds` / `HandbrakeRampOutSeconds` | Tempo para a perda de aderência chegar ao máximo / voltar ao normal | rampa no carro (`FDNHandbrakeRamp`) |
| `HandbrakeTorque` | Força de frenagem do freio de mão | `MaxHandBrakeTorque` da roda traseira |
| `CameraDistanceCm`, `CameraHeightCm`, `CameraFov` | Câmera: distância, altura e campo de visão | `TargetArmLength`, `SocketOffset.Z`, `FieldOfView` |
| `NitroAccelMps2` | Aceleração extra com Shift (0 = sem nitro) | `AddForce` para frente |
| `NitroDrainPerSecond` / `NitroRegenPerSecond` | Gasto e recarga da barra (0 a 100) por segundo | barra de teste (`FDNTestNitro`) |

Os valores padrão das colunas novas são os do carro do template: uma linha com só as cinco colunas antigas continua dando
o mesmo carro de antes (`Padrao`, `Arcade` e `Pesado` seguem no arquivo).

## Os presets

| | `Padrao` (template) | `arcade-1` leve | `arcade-2` derrapador | `arcade-3` meio-termo |
|---|---|---|---|---|
| Massa (kg) | 1500 | 1200 | 1200 | 1350 |
| Torque do motor | 750 | 850 | 850 | 800 |
| Centro de massa desce (cm) | 0 | 25 | 25 | 15 |
| Mola (SpringRate) | 250 | 170 | 170 | 210 |
| Amortecimento | 0,50 | 0,55 | 0,55 | 0,50 |
| Curso da suspensão (cm) | 10 | 18 | 18 | 14 |
| Barra estabilizadora | 0,15 | 0,05 | 0,05 | 0,10 |
| Aderência lateral / volante | 1,0 / 1,0 | 1,0 / 1,0 | 1,0 / 1,0 | 1,0 / 1,0 |
| Aderência do eixo traseiro | x1,00 | x1,00 | x0,85 | x0,92 |
| Aderência traseira com freio de mão | x1,00 | x1,00 | x0,35 | x0,35 |
| Rampa do freio de mão (entra / volta, s) | 0,10 / 0,50 | 0,10 / 0,50 | 0,12 / 0,70 | 0,12 / 0,70 |
| Torque do freio de mão | 6000 | 6000 | 3000 | 3000 |
| Câmera: distância / altura (cm) / FOV | 650 / 150 / 90 | 810 / 185 / 95 | 810 / 185 / 95 | 810 / 185 / 95 |
| Nitro: aceleração extra (m/s²) | 10 | 12 | 12 | 12 |

**A resposta de direção foi preservada:** o ganho do volante e a aderência lateral são os do `Padrao` em todos os presets,
e nenhum parâmetro de direção (`SteeringSetup`, ângulo das rodas dianteiras) foi tocado. Um teste automático trava isso.
O que **não** dá para garantir é que a curva continue com a mesma sensação: massa, suspensão e centro de massa mexem na
dinâmica. Se a curva piorar, é por isso, e o primeiro ajuste é `LateralGrip` ou `SteeringGain`.

### `arcade-1` (leve)

- **O que mudou:** 20% menos massa, centro de massa 25 cm mais baixo, mola 32% mais macia com quase o dobro de curso,
  barra estabilizadora um terço da do template, mais torque. Câmera 25% mais longe, 23% mais alta, FOV 95. Nada muda na
  traseira nem no freio de mão.
- **Medido:** acelera bem mais (76 km/h aos 2,0 s contra 53 do `Padrao`). O freio de mão **continua não derrapando**
  (ângulo de deslizamento máximo 3,7 graus, igual ao `Padrao`): este preset é o "leve" sem derrapagem, de propósito, para
  isolar o efeito do peso e da suspensão.
- **Esperado sentir:** carro mais ágil e que "quica" mais; a carroceria rola mais nas curvas; menos "duro e pesado".

### `arcade-2` (derrapador)

- **O que mudou:** igual ao `arcade-1`, mais: traseira com 85% da aderência e **freio de mão que solta a traseira**
  (sobra 35% da aderência traseira, com rampa de 0,12 s para entrar e 0,7 s para voltar) e freio de mão com metade da
  força (3000), para não parar o carro.
- **Medido:** na curva com freio de mão a 55 km/h o ângulo de deslizamento chega a **34 graus** (duas execuções: 34,3 e 34,8)
  e a guinada a 162 graus/s, contra 3 graus do `Padrao`. A mesma curva **sem** freio de mão fica em 1 a 2 graus: o carro
  só escorrega com o freio de mão (a traseira com 85% sozinha não faz derrapar). Acelera um pouco menos que o `arcade-1`
  (68 contra 76 km/h aos 2,0 s) por patinar mais.
- **Esperado sentir:** freio de mão que faz a traseira sair e o carro rodar de lado, voltando a se alinhar em uns 0,7 s
  depois de soltar.

### `arcade-3` (meio-termo)

- **O que mudou:** massa 1350, mola 210, curso 14, barra 0,10, centro de massa 15 cm mais baixo; traseira a 92% e o mesmo
  freio de mão do `arcade-2`.
- **Medido:** ângulo de deslizamento com freio de mão **21 graus** (20,4 e 21,2), guinada 118 graus/s: derrapa de leve.
- **Esperado sentir:** meio caminho entre o `Padrao` e o `arcade-2`: o freio de mão faz a traseira sair, mas menos, e o
  carro é um pouco mais pesado e firme que o `arcade-1`.

### Nitro (Shift)

Um nitro **simples, só de teste**: aceleração extra para frente (`AddForce` = massa x aceleração) enquanto Shift está
apertado, com barra fictícia de 0 a 100 que gasta 25 por segundo e recarrega 10 por segundo solto. **Não é o `FDNTurbo`
do projeto principal**: o projeto de teste é outro projeto Unreal e não compila o código do `Core` (reusar exigiria copiar
os arquivos). A barra não tem ganho por contramão, derrapada ou salto.
**Medido** (`arcade-1`): com 1,2 s de nitro o carro chegou a 121 km/h aos 2,4 s contra 88 km/h sem nitro; a barra foi de
100 a 70 e passou a recarregar ao soltar.

**Amassado (lote 10):** as teclas T, Y, U (frente), G, H, J (lado esquerdo) e V (limpar) dão batidas simuladas e o HUD mostra a
contagem de amassados. Ver [`prototipo-amassado.md`](prototipo-amassado.md).

## Roteiro de 5 minutos (3 perguntas por preset)

Mesmo trecho reto e as mesmas curvas para todos. Comece pelo `Padrao` (tecla 0) para ter a referência.

1. **`Padrao` (0), 30 s:** acelere, faça duas curvas fortes e use o freio de mão numa delas. Anote como é.
2. **`arcade-1` (1), 1 min:**
   - O carro parece **mais leve** que o `Padrao`, ou igual?
   - A **curva** continua tão boa quanto antes (a resposta do volante é a mesma)?
   - A **câmera** está numa distância boa?
3. **`arcade-2` (2), 2 min:** acelere até uns 80 km/h, entre numa curva e **aperte Espaço** por meio segundo.
   - A traseira **sai** de forma que dá gosto, ou roda demais e perde o controle?
   - **Soltando** o Espaço, o carro volta a andar reto de um jeito suave ou brusco?
   - O freio de mão parece **menos "travado"** que no `Padrao`?
4. **`arcade-3` (3), 1 min:** repita a curva com Espaço.
   - É **melhor ou pior** que o `arcade-2` para controlar o carro?
   - A derrapada é suficiente para dar prazer, ou é tímida?
   - Entre os três, qual **você ficaria**?
5. **Nitro (em qualquer preset), 30 s:** segure **Shift** numa reta.
   - Dá para **sentir** o empurrão, ou é fraco/forte demais?
   - O HUD mostra o **nitro ativo** e a barra descendo?
   - Gastar e esperar recarregar parece **justo**?

Anote em [`notas-sprint-2.md`](notas-sprint-2.md) o preset preferido e o que mudar (por exemplo "arcade-2 roda demais":
suba `HandbrakeRearGrip` de 0,35 para 0,40).

## Como foi medido (sem sentir)

`Tools\Unreal\car-probe.ps1` roda o carro sem intervenção e imprime o que o código mediu. Exemplos:

```powershell
.\Tools\Unreal\car-probe.ps1 -Uproject Sandbox\VeiculoTeste\VeiculoTeste.uproject -Row arcade-2 -Scenario Handbrake
.\Tools\Unreal\car-probe.ps1 -Uproject Sandbox\VeiculoTeste\VeiculoTeste.uproject -Row arcade-1 -Scenario Nitro
.\Tools\Unreal\car-probe.ps1 -Uproject Sandbox\VeiculoTeste\VeiculoTeste.uproject -Row arcade-3 -Shot
```

Medidas deste lote (mesmo mapa, aceleração a fundo e depois a manobra a 55 km/h; execuções variam um pouco):

| Preset | Velocidade aos 2,0 s | Deslizamento máx. com freio de mão (graus) | Guinada máx. (graus/s) | Deslizamento sem freio de mão (graus) |
|---|---|---|---|---|
| `Padrao` | 53 | 3,4 | 32 | 4,0 |
| `arcade-1` | 76 | 3,7 | 32 | 1,1 |
| `arcade-2` | 68 | 34,3 e 34,8 | 161 e 162 | 1,6 |
| `arcade-3` | 63 | 20,4 e 21,2 | 117 e 120 | 1,1 |

Conferido no log ao aplicar cada preset: a massa do corpo físico é a pedida (1200, 1350 e 1500 kg) e o centro de massa
local desce exatamente o pedido (Z de 50,09 no `Padrao` para 25,09 com 25 cm e 35,09 com 15 cm). A captura de tela
(`-Shot`) confirmou o HUD e o enquadramento da câmera.

**A aderência traseira tem um "precipício"** (medido na calibração): o deslizamento não cresce devagar com a perda de
aderência. O produto `RearGripFactor` x `HandbrakeRearGrip` abaixo de ~0,32 solta a traseira de vez; abaixo de ~0,28 o
carro **roda** (89 graus). Exemplos medidos com freio de mão a 3000: produto 0,32 = 12 graus; 0,30 = 37 a 41 graus;
0,28 = 90 graus (rodopio). Por isso os presets ficam em 0,30 (`arcade-2`) e 0,32 (`arcade-3`) e qualquer ajuste de
`RearGripFactor` ou `HandbrakeRearGrip` deve ser feito em passos pequenos (0,02), olhando o resultado. O torque do freio de
mão também pesa: com o mesmo produto, 1800 em vez de 3000 tirou a derrapada (8 graus). A razão está no modelo de pneu do
Chaos: a aderência disponível é um teto e a força lateral é cortada nele (`WheelSystem.cpp`, círculo de atrito).

## Parâmetros do Chaos 5.8 conferidos na engine

Conferidos nos cabeçalhos e fontes da engine instalada (`Plugins/Experimental/ChaosVehiclesPlugin` e
`Source/Runtime/Experimental/ChaosVehicles`):

- **Usados:** `UChaosVehicleWheel`: `SpringRate`, `SuspensionDampingRatio`, `SuspensionMaxRaise`, `SuspensionMaxDrop`,
  `RollbarScaling`, `FrictionForceMultiplier`, `MaxHandBrakeTorque`, `AxleType`. `UChaosVehicleMovementComponent`: `Mass`,
  `SetHandbrakeInput`, `SetTargetGear`, `RecreatePhysicsState` (de `UActorComponent`). `UChaosWheeledVehicleMovementComponent`:
  `EngineSetup.MaxTorque`, `SetWheelSlipGraphMultiplier`, `SetWheelFrictionMultiplier`, `Wheels`. Outros:
  `UPrimitiveComponent::SetCenterOfMass`, `AddForce`.
- **Não achados:**
  - **Função para mudar `RollbarScaling` com o carro andando.** O valor só é lido ao criar o veículo, da classe da roda
    (`ChaosWheeledVehicleMovementComponent.cpp`, ao montar os eixos), e `GetAxles()` é `const`. Por isso a barra estabilizadora
    e as outras propriedades de roda são gravadas na classe da roda e a troca de preset recria o estado físico.
  - **Multiplicador de aderência traseira com o freio de mão e rampa de volta.** Não existe no Chaos; está implementado no
    carro (`FDNHandbrakeRamp`), mudando a aderência a cada quadro com `SetWheelFrictionMultiplier` e
    `SetWheelSlipGraphMultiplier`.
  - **Uma propriedade de "centro de massa baixo" pronta.** Existe `bEnableCenterOfMassOverride` (valor absoluto, que
    exigiria saber o centro calculado); usei o deslocamento do corpo (`SetCenterOfMass`), conferido pelo log.
  - **Mass override que sobreviva:** `SetMassOverrideInKg` (usado antes, no lote 4) não vale para o Chaos: ao recalcular a
    massa, `UpdateMassProperties` reaplica o `Mass` do componente (`ChaosVehicleMovementComponent.cpp`). Agora a massa vai
    em `Mass` e o log confere a massa do corpo. Não verifiquei se o código antigo chegava a aplicar a massa das linhas
    `Arcade` e `Pesado`.

## Limites e pendências

- **Nada disto foi sentido.** Os números de "esperado" são hipóteses; o roteiro acima é para o Felipe confirmar ou corrigir.
- **Editor aberto trava o build.** Durante o lote 8 o editor estava aberto no `VeiculoTeste` e o código foi compilado e testado em
  uma cópia do projeto. No lote 10 o editor estava fechado e o `Sandbox/VeiculoTeste` real foi **compilado no lugar** (inclui os
  presets deste documento). Se o Felipe abrir o editor e ele pedir para recompilar, aceite; para compilar pela linha de comando
  o editor precisa estar fechado.
- **Trocar de preset andando** zera o câmbio por ~0,2 s e reaplica a suspensão; não é uma troca "limpa". Para comparar sem
  esse efeito, reinicie com Alt+P e use `-DNCarRow=arcade-2` ou o campo `TuningRow` do carro.
- **Medidas do mapa de teste:** a parede fica a ~50 m do ponto de partida, então as manobras medidas são curtas. O
  deslizamento medido a 55 km/h não vale para 120 km/h.
- **Nitro e HUD** são de teste, no projeto descartável. O turbo real (`FDNTurbo`, `UDNTurboComponent`) continua só no
  projeto principal e ainda não está ligado a nenhum carro.
- **Dano e deformação** (o carro não quebra) ficam para o protótipo medido à parte.
