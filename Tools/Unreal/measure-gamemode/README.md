# Modo de medição (GameMode em C++)

`DNMeasureGameMode.h` e `.cpp` são o GameMode que o `Tools/Unreal/measure.ps1` usa para medir
desempenho. Estão aqui como **cópia de referência**: a versão que roda hoje vive no projeto de teste
(`Sandbox/VeiculoTeste/Source/VeiculoTeste`, fora do Git). Se você mudar uma, mude a outra.

## Para que serve

Roda um cenário de medição sem intervenção: escolhe o carro do jogador, coloca N carros extras
(substitutos de custo: carros iguais ao do jogador, **não** o tráfego nem o dano reais), aquece, liga o
CSV Profiler (`CsvProfile Start`), mede, desliga (`CsvProfile Stop`) e fecha o jogo. O jogador dirige
sozinho (acelerador e volante em vai-e-vem). O `measure.ps1` abre o jogo com
`<mapa>?game=/Script/<Módulo>.DNMeasureGameMode` e lê o CSV que sobra em `Saved/Profiling/CSV`.

Argumentos de linha de comando que ele entende (o `measure.ps1` os monta):

| Argumento | Efeito |
|---|---|
| `-DNCarClass=Template` ou `DN` | Carro do jogador e dos extras |
| `-DNExtra=N` | Quantidade de carros extras |
| `-DNExtraMove` | Extras em movimento (sem isto ficam parados, acordados a cada 0,5 s) |
| `-DNWarmup=S`, `-DNMeasure=S` | Segundos de aquecimento (padrão 30) e de medição (padrão 60) |
| `-DNSeed=N` | Semente da posição e rotação dos extras (padrão 12345) |

## De que depende (conferido no código)

**Módulos no `Build.cs`** (`PublicDependencyModuleNames`): `Core`, `CoreUObject`, `Engine` e
`ChaosVehicles` (de onde vêm `AWheeledVehiclePawn` e `UChaosVehicleMovementComponent`).

**Includes do projeto de teste** (o `.cpp` os usa e você precisa ajustar ao copiar):

- `#include "VeiculoTeste.h"`: traz a categoria de log `LogVeiculoTeste`, usada nos `UE_LOG`. Troque pela
  categoria de log do seu projeto.
- `#include "DNTestCar.h"`: é a classe `ADNTestCar` (carro do projeto de teste), usada quando
  `-DNCarClass=DN`. Se o seu projeto não tem essa classe, apague essa linha e o `if` que a usa (o ramo `DN`).

**Caminho de asset fixo** (no começo do `.cpp`): o carro do template é carregado de
`/Game/VehicleTemplate/Blueprints/SportsCar/BP_SportsCar_Pawn.BP_SportsCar_Pawn_C`. Troque pelo
Blueprint do seu carro se for outro.

**Suposição:** o mapa precisa ter um `PlayerStart` e chão onde um raio vertical acerte (`ECC_WorldStatic`),
porque é assim que os extras são posicionados. Os extras sem chão são pulados e o log diz quantos foram colocados.

## Como copiar para um projeto Unreal

1. Copie `DNMeasureGameMode.h` e `DNMeasureGameMode.cpp` para a pasta do módulo C++ do jogo
   (`Source/<Módulo>/`).
2. Confirme que o `Build.cs` do módulo lista `ChaosVehicles`.
3. Ajuste os pontos da seção anterior (log, `DNTestCar.h`, caminho do Blueprint).
4. Compile: `Tools/Unreal/build.ps1 -Uproject <projeto>.uproject`.
5. Meça: `Tools/Unreal/measure.ps1 -Uproject <projeto>.uproject`. O script usa o mapa
   `/Game/VehicleTemplate/Maps/VehicleBasic` por padrão; passe `-Map /Game/...` para outro. Ele exige o
   nome do módulo no caminho `?game=/Script/VeiculoTeste.DNMeasureGameMode`, que está fixo no script:
   troque `VeiculoTeste` pelo nome do módulo do seu projeto.
