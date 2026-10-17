# O carro do jogo (lote 12)

O carro do protótipo (`ADNTestCar`, no projeto descartável `Sandbox/VeiculoTeste`) foi levado para o projeto principal como
`ADNVehicle`, **usando direto o Core**: o dano é o `FDNDamage` e o turbo é o `UDNTurboComponent` / `FDNTurbo`, não mais as
cópias de teste. O Sandbox fica como estava, só de referência (`Tools/Unreal/veiculo-teste/`).

## Onde está cada coisa

| O quê | Onde |
|---|---|
| Presets (números do carro) | `Data/Cars.csv` (`Id` + as 19 colunas de `FDNCarConfig`, entre elas a assistência de derrapagem `DriftAssist*`), validados por `FDNDataValidation::ValidateCars` |
| Struct pura e rampa do freio de mão | `Source/Detonauto/Core/DNCar.h` |
| Assistência de derrapagem (regra pura) | `Source/Detonauto/Core/DNDriftAssist.h` |
| Modos de câmera (tecla C) | `Data/Cameras.csv` + `Source/Detonauto/Core/DNCamera.h` |
| Conta das rodas visuais (giro e esterço no espaço certo) | `Source/Detonauto/Core/DNWheelVisual.h` |
| Parâmetros do material de amassado, lado da batida, agrupador de contatos | `Source/Detonauto/Core/DNDentParams.h` |
| O carro (Chaos Vehicles em C++, câmera, entrada, HUD, amassado) | `Source/Detonauto/Vehicle/DNVehicle.*` |
| Rodas (sem Blueprint) | `Source/Detonauto/Vehicle/DNVehicleWheels.h` |
| Mapa de teste (piso, parede, luz) criado em C++ | `Source/Detonauto/Vehicle/DNVehicleArena.*` |
| GameMode de teste e cenários de medição | `Source/Detonauto/Vehicle/DNVehicleTestGameMode.*` |
| Assets do carro esportivo (22, da Epic, só para Unreal) | `Content/Detonauto/Vehicle/SportsCar/` (migração: `Tools/Unreal/migrate_sports_car.ps1`) |
| Material-pai do amassado | `Content/Detonauto/Vehicle/M_DNDentParent` (criado por `Tools/Unreal/create_dent_material.ps1 -Dest /Game/Detonauto/Vehicle`) |
| Jogar e medir | `Tools/Unreal/vehicle-probe.ps1` |

Nenhum Blueprint, nenhum `.umap` do projeto: o GameMode abre sobre `/Engine/Maps/Entry` (mapa vazio que já vem na engine) e cria o
piso, a parede e a luz em C++.

## Como jogar

Feche o editor, compile e rode:

```powershell
.\Tools\Unreal\build.ps1 -Uproject Detonauto.uproject
.\Tools\Unreal\vehicle-probe.ps1 -Uproject Detonauto.uproject -Play
```

Teclas (as mesmas do protótipo): W/S acelera e freia, A/D esterça, Espaço = freio de mão, **Shift = turbo**, **C = próxima câmera**, 1/2/3/0 = presets
`arcade-1`/`arcade-2`/`arcade-3`/`Padrao`, T/Y/U = batida simulada na frente (pequena, média, máxima), G/H/J = no lado esquerdo,
V = limpa os amassados. O HUD mostra preset, velocidade, turbo e amassados. Para mudar um número do carro, edite `Data/Cars.csv`
e jogue de novo (o arquivo é lido ao abrir o jogo; valor fora da faixa é recusado no log, com o campo e a linha).

Significado de cada coluna, o que cada preset muda e o roteiro de teste: [`presets-carro.md`](presets-carro.md).

## Turbo (por enquanto só consome)

- Shift é o botão do turbo; a barra é a do `FDNTurbo` (0 a 100, consumo e turbo prolongado do `Data/Turbo.csv`).
- Os ganhos reais (contramão, raspão, derrapada, tempo no ar, detona) **só entram quando houver os eventos** que os produzem;
  o carro repassa ao Core só o botão e a velocidade, e deixa os outros campos do quadro como "não aconteceu".
- **Modo de teste** (`bTurboTestMode`, ligado por padrão): a barra começa cheia e **enche de novo 2 s depois de esvaziar**, para
  dar para testar sem fim. O enchimento usa o evento de detona do Core (um quadro com `bTakedown`), sem mexer no estado do Core.
  Não é regra de jogo: sai quando houver ganhos de verdade.
- `NitroAccelMps2` (coluna de `Cars.csv`) é a aceleração extra enquanto o turbo está ativo; 0 desliga o empurrão naquele preset.
  As colunas `NitroDrainPerSecond` e `NitroRegenPerSecond` do protótipo **não existem mais**: o gasto vem do Core.

## Decisões deste lote (o contrato não cobria)

1. **Rodas visuais sem Anim Blueprint.** No template quem gira e esterça as rodas é o Anim Blueprint (binário). Aqui as malhas das
   rodas ficam presas aos ossos `Phys_Wheel_*` e o `Tick` copia para elas os mesmos três números que o `UVehicleAnimationInstance`
   entregaria ao Blueprint: giro (pitch), esterçamento (yaw) e curso da suspensão. **Conferido em captura parada** (rodas no lugar,
   do tamanho certo, de frente e de trás); **o giro e o esterçamento em movimento ficam para o playtest**.
2. **Atrito do piso = 1,0.** O Chaos multiplica o atrito das rodas pelo do material físico da superfície. Os presets foram medidos
   na pista do template, que usa o material `NonSlippery` (atrito 1,0, restituição 0). O piso padrão da engine tem 0,7, e nele o
   `arcade-2` com freio de mão **roda** (89,7 graus) em vez de derrapar (32). O piso da arena ganha um material físico em runtime com
   1,0; a parede fica com o material padrão, como na calibração das batidas (lote 11).
3. **Validação com limites, em vez de limitar em silêncio.** O protótipo cortava valores fora da faixa (`Clamp`); o `Cars.csv`
   recusa. As faixas são as mesmas. A regra do "precipício" é checada: `RearGripFactor` x `HandbrakeRearGrip` **>= 0,295**
   (medido: 0,32 = 12 graus, 0,30 = 37 a 41, 0,294 = 77, 0,28 = 90). O 0,295 deixa passar o `arcade-2` (0,2975) e recusa o que roda.
4. **Faixas de cada campo** (inclusivas): `MassKg` 100–10000, `MaxTorque` 50–5000, `LateralGrip` 0,1–5, `SuspensionStiffness` 10–2000,
   `SteeringGain` 0,1–3, `CenterOfMassDropCm` 0–60, `SuspensionDampingRatio` 0,05–1, `SuspensionTravelCm` 2–40, `RollbarScaling` 0–1,
   `RearGripFactor` 0,2–2, `HandbrakeRearGrip` 0,05–1,5, `HandbrakeRamp{In,Out}Seconds` 0,01–3, `HandbrakeTorque` 0–20000,
   `NitroAccelMps2` 0–40, `DriftAssistStartDeg` 0–80, `DriftAssistGain` 0–500, `DriftAssistMax` 0–2000 (ganho e teto são um par: os dois zero ou os dois maiores que zero). **As colunas de câmera saíram deste arquivo (lote 14): a câmera agora é `Data/Cameras.csv`.**
5. **`Cars.csv` não herda as linhas `Arcade` e `Pesado`** do `TestCars.csv`: eram do lote 4 e nenhuma tecla as usa.
6. **Dados pelo subsistema.** `UDNGameDataSubsystem` ganhou `GetDamageConfig()` (padrão se `Damage.csv` é inválido, como as outras) e
   `TryGetCarConfig(Id)` (falso se o preset não existe **ou** `Cars.csv` é inválido: com a tabela inválida nenhum preset vale).
   O carro, nesse caso, nasce com os números do template (os padrões de `FDNCarConfig`) e registra o erro.
7. **Cenários de medição no GameMode, não no carro.** O `ADNVehicle` não sabe de sonda nem de captura; o
   `ADNVehicleTestGameMode` dirige o carro pela API pública (`SetHandbrake`, `SetTurboButton`, `SwitchPreset`...). Os cenários são
   os do protótipo, menos as estatísticas de tempo de quadro (a medição de desempenho tem script próprio, `measure.ps1`).

## Medido no projeto principal (sem sentir; execuções variam um pouco)

Mesma manobra do protótipo (acelera a fundo e entra na curva a 55 km/h, freio de mão por 0,8 s), no mapa vazio com o piso de atrito 1,0:

| Preset | Deslizamento máx. com freio de mão (graus) | Guinada máx. (graus/s) | No protótipo |
|---|---|---|---|
| `Padrao` | 3,4 | 32 | 3,4 / 32 |
| `arcade-1` | 3,7 | 32 | 3,7 / 32 |
| `arcade-2` | 32,3 | 157 | 34,3 e 34,8 / 161 e 162 |
| `arcade-3` | 16,9 | 108 | 20,4 e 21,2 / 117 e 120 |

Troca de preset andando (cenário `Switch`): a velocidade se mantém (55,9 -> 55,9 km/h ao trocar para `arcade-2`; 88,1 -> 88,3 ao voltar
para `Padrao`) e o câmbio volta a engatar em ~0,2 s. Turbo (cenário `Turbo`, `arcade-1`): a barra gasta 25 por segundo (100 -> 70 em 1,2 s)
e o carro chega a 122 km/h aos 2,4 s. Batida na parede a 128 km/h: variação de velocidade de 44,7 m/s, amassado de 25 cm e carro destruído
pelas regras do dano.

## Lote 14 (playtest 1)

Bugs de rodas, amassado e curva, a assistência de derrapagem, o turbo e as cinco câmeras: [`lote-14.md`](lote-14.md) (causas, medidas e o que ficou anotado para depois).

## O que ainda não existe

- Ganhos reais do turbo (eventos de contramão, raspão, derrapada, ar e detona), e a UI da barra.
- Reação do carro a ser destruído (peças soltando): o amassado visual é só o material; `FDNDamage` já devolve as peças, o carro ainda não as usa.
- Controle (gamepad) e remapeamento de teclas.
