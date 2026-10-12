# Estrutura do projeto de teste (VeiculoTeste)

Guia da Sprint 2/3 para quem aprende C++ no Unreal. Tudo aqui foi conferido
contra o projeto real em `Sandbox\VeiculoTeste` (criado a partir do template
Vehicle C++ do Unreal 5.8) e contra os scripts de `Tools/Unreal/`. O que não foi
conferido está marcado como **suposição**. Para os conceitos (Actor, Pawn,
macros), leia antes `docs/conceitos-unreal.md`.

O projeto de teste é descartável e fica em `Sandbox/`, fora do Git. O projeto
definitivo do Detonauto terá outro nome e a mesma estrutura.

## Pastas e arquivos da raiz do projeto

| Item | O que é | No Git do jogo final? |
|---|---|---|
| `VeiculoTeste.uproject` | Descreve o projeto: o módulo `VeiculoTeste` e os plugins ligados (`ChaosVehiclesPlugin`, `ModelingToolsEditorMode`, `GameInputWindows`). | Sim |
| `Source/` | Todo o código C++. | Sim |
| `Content/` | Assets: malhas, materiais, mapas, Blueprints. | Sim, pelo LFS |
| `Config/` | Arquivos `.ini` de configuração (`DefaultEngine.ini`, `DefaultInput.ini` etc.). | Sim |
| `Binaries/` | Programas compilados (a DLL do módulo). | Não |
| `Intermediate/` | Temporários do build, incluindo código gerado. | Não |
| `Saved/` | Logs, relatórios de teste, configurações do usuário. | Não |
| `VeiculoTeste.sln` | Solução do Visual Studio, gerada a partir do `.uproject`. | Não (regenerável) |

Duas pastas do roteiro da Sprint 2 **não existem** neste projeto: `Plugins/`
(o template não traz plugins próprios) e `DerivedDataCache/` (o cache fica no
perfil do usuário, fora do projeto). Não é erro.

## Content/: o que veio e de onde

- `Content/VehicleTemplate/`: mapas, Blueprints, materiais e malhas do template.
  O mapa principal é `/Game/VehicleTemplate/Maps/VehicleBasic`.
- `Content/Vehicles/` (carros, rodas, materiais físicos) e `Content/Input/`
  (ações e contextos de entrada): conteúdo compartilhado que o editor visual
  copia da pasta `Templates\TemplateResources` da instalação. Os nomes das
  pastas importam: os assets se referem a `/Game/Vehicles/...` e
  `/Game/Input/...`.
- `Content/Variant_OffRoad/` e `Content/Variant_TimeTrial/`: Blueprints das
  duas variantes do template. Os meshes e mapas das variantes **não** foram
  copiados para este projeto.

## Source/: o que existe

```
Source/
  VeiculoTeste.Target.cs          alvo do jogo
  VeiculoTesteEditor.Target.cs    alvo do editor
  VeiculoTeste/
    VeiculoTeste.Build.cs         dependências do módulo
    VeiculoTeste.h / .cpp         entrada do módulo
    VeiculoTestePawn, ...GameMode, ...PlayerController, ...UI
    SportsCar/   OffroadCar/      os dois carros e suas rodas
    Variant_OffRoad/  Variant_TimeTrial/  modos extras do template
```

### Os arquivos `.Target.cs`

Dizem ao build **o que** montar. `VeiculoTeste.Target.cs` monta o jogo
(`TargetType.Game`); `VeiculoTesteEditor.Target.cs` monta o editor
(`TargetType.Editor`). Ambos listam `ExtraModuleNames.Add("VeiculoTeste")`: o
único módulo do projeto. É o alvo do editor que `build.ps1` compila.

### `VeiculoTeste.Build.cs`: o que ele lista e por quê

Um módulo é um pacote de código com suas próprias dependências. O `Build.cs`
é a lista do que este módulo precisa:

| Módulo | Para que o projeto usa (conferido no código) |
|---|---|
| `Core`, `CoreUObject`, `Engine` | Base de todo código Unreal: tipos, objetos, Actors. |
| `InputCore`, `EnhancedInput` | Entrada. O Pawn e o PlayerController usam `UEnhancedInputComponent` e `UInputMappingContext`. |
| `ChaosVehicles`, `PhysicsCore` | Física de veículos. O Pawn usa `UChaosWheeledVehicleMovementComponent`; as rodas herdam de `UChaosVehicleWheel`. |
| `UMG`, `Slate` | Interface. `UVeiculoTesteUI` herda de `UUserWidget`. |

Também lista `PublicIncludePaths` para cada subpasta (`SportsCar`,
`OffroadCar`, `Variant_*`), para os `#include` não precisarem do caminho
completo. Quando você usar uma classe de outro módulo e o link falhar, o nome
do módulo entra aqui. Mudou o `Build.cs`: compile pelo Visual Studio, não por
Live Coding.

### `VeiculoTeste.h` e `VeiculoTeste.cpp`

A entrada do módulo: `IMPLEMENT_PRIMARY_GAME_MODULE` registra o módulo e
`DECLARE_LOG_CATEGORY_EXTERN(LogVeiculoTeste, ...)` cria a categoria de log
usada nos `UE_LOG` do projeto.

## As classes C++ do template

| Classe | Herda de | O que faz (visto no código) |
|---|---|---|
| `AVeiculoTestePawn` | `AWheeledVehiclePawn` | Base de todo carro. Cria duas câmeras com braço (`FrontCamera`, `BackCamera`), liga o veículo físico e recebe a entrada: direção, acelerador, freio, freio de mão, olhar em volta, trocar câmera e resetar o carro. Tem funções `Do...` (`DoSteering`, `DoThrottle`...) chamáveis por Blueprint ou pelo controle de toque. Tem `FlippedCheck` para detectar carro capotado. |
| `AVeiculoTesteSportsCar` | `AVeiculoTestePawn` | O carro esportivo (`abstract`: só o Blueprint filho é jogável. **Suposição:** é o `BP_SportsCar_Pawn`; não conferi a classe-pai dele). |
| `AVeiculoTesteOffroadCar` | `AVeiculoTestePawn` | O carro off-road. Declara os componentes de chassi e dos quatro pneus (`Chassis`, `TireFrontLeft`...). |
| `UVeiculoTesteWheelFront` / `...WheelRear` | `UChaosVehicleWheel` | Rodas dianteira e traseira base. Ficam em `SportsCar/` e `OffroadCar/` as versões de cada carro (`UVeiculoTesteSportsWheelFront` e as outras). |
| `AVeiculoTestePlayerController` | `APlayerController` | Registra os contextos de entrada, cria a interface de velocidade/marcha e a de toque, assume o carro (`OnPossess`) e cuida de respawn. |
| `AVeiculoTesteGameMode` | `AGameModeBase` | Mínimo: só define `PlayerControllerClass`. O Pawn padrão vem do Blueprint `BP_VehicleAdvGameMode`. |
| `UVeiculoTesteUI` | `UUserWidget` | HUD de velocidade e marcha. `UpdateSpeed` e `UpdateGear` chamam eventos implementados em Blueprint (`OnSpeedUpdate`, `OnGearUpdate`). |
| `AOffroadGameMode`, `ATimeTrialGameMode`, `ATimeTrialPlayerController`, `ATimeTrialTrackGate`, `UTimeTrialUI`, `UTimeTrialStartUI` | `AGameModeBase`, `APlayerController`, `AActor`, `UUserWidget` | Variantes: mapa off-road e contra-relógio com portões de pista. Não são usadas pelo mapa `VehicleBasic`. |

Como as peças se ligam (**suposição**: deduzida do código e dos logs, não vista
no editor): o mapa `VehicleBasic` usa o GameMode `BP_VehicleAdvGameMode`
(`GlobalDefaultGameMode` no `DefaultEngine.ini`). Ele traz o
`AVeiculoTestePlayerController` e o Pawn do carro. Se esses Blueprints não
carregam, o Unreal cai no Pawn padrão, uma câmera livre.

## Compilar de dois jeitos

### Editor aberto: Live Coding

Salve o código e aperte `Ctrl+Alt+F11` no editor. **Suposição:** é o atalho
que o roteiro da Sprint 2 informa; não foi testado nesta máquina. Serve para
mudar o corpo de funções. Para mudar um `.h`, criar classe, adicionar
`UPROPERTY` ou mudar o `Build.cs`, feche o editor e compile pelo Visual Studio.

### Editor fechado: Visual Studio

Abra `Sandbox\VeiculoTeste\VeiculoTeste.sln`, escolha **Development Editor** e
**Win64** e aperte `Ctrl+B`. **Suposição:** o fluxo no Visual Studio segue o
roteiro; neste projeto o que foi executado e conferido foi a linha de comando
abaixo, que usa o mesmo UnrealBuildTool.

Se a solução não existir ou estiver quebrada, gere de novo:

```powershell
& "C:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\Build.bat" -ProjectFiles "-project=<caminho>\VeiculoTeste.uproject" -game -engine
```

### Pela linha de comando: `build.ps1` e `test.ps1`

Os scripts ficam em `Tools/Unreal/` e servem para qualquer projeto: recebem o
`.uproject` e acham a engine sozinhos (registro do Windows ou arquivo do
Launcher).

```powershell
.\Tools\Unreal\build.ps1 -Uproject Sandbox\VeiculoTeste\VeiculoTeste.uproject
.\Tools\Unreal\test.ps1  -Uproject Sandbox\VeiculoTeste\VeiculoTeste.uproject
```

- `build.ps1` compila o alvo Editor em Win64 Development. Sai com `0` se
  compilou e com o código do UnrealBuildTool se falhou (visto: `6` com erro de
  sintaxe).
- `test.ps1` abre o editor sem janela, roda `Automation RunTests <filtro>` e
  imprime aprovados e reprovados. Sai com `0` só se todos passaram; `1` se algum
  falhou, se nenhum teste foi encontrado ou se estourou o tempo
  (`-TimeoutSeconds`, padrão 600).
- O filtro padrão é o **nome do projeto**: os testes do jogo devem se chamar
  `NomeDoProjeto.Algo`. Use `-Filter` para rodar só um grupo.
- Compile antes de testar. Feche o editor visual antes de compilar: ele trava a
  DLL do módulo (`UnrealEditor-VeiculoTeste.dll`).
- Os scripts só gravam dentro da pasta do projeto (`Intermediate`, `Binaries`,
  `Saved`) e nos logs do UnrealBuildTool no perfil do usuário.

Nunca rode comandos do Unreal que regravam pacotes (como o commandlet
`ResavePackages`) sem limitar a pasta: ele mexeu na instalação da engine uma vez.

## Conferência contra `docs/conceitos-unreal.md`

O guia está de acordo com o código, com estes pontos a corrigir:

1. A seção "As classes que organizam o jogo" diz que o primeiro teste do
   VeiculoTeste caiu no Pawn padrão por o Pawn do GameMode não carregar. É a
   consequência, não a causa: a causa real foram assets apontando para o nome
   antigo do módulo (`/Script/TP_VehicleAdv`) e pastas no lugar errado em
   `Content/`.
2. O guia diz que o GameMode "cria o PlayerController". No template,
   `AVeiculoTesteGameMode` só **define a classe** do controlador
   (`PlayerControllerClass`); o Pawn padrão é definido no Blueprint do GameMode.
3. O guia diz que variável de objeto sem `UPROPERTY` pode ser apagada pelo
   coletor de lixo. Vale como regra, mas o template tem uma exceção
   deliberada: `ChaosVehicleMovement` em `AVeiculoTestePawn` é um
   `TObjectPtr` **sem** `UPROPERTY`, e é seguro porque o componente pertence ao
   próprio Pawn. Para ponteiros que você mesmo cria, mantenha o `UPROPERTY`.
4. Os macros mostrados no guia são `UCLASS()` simples. O template usa
   `UCLASS(abstract)` nos carros, no GameMode e no HUD, e
   `UCLASS(abstract, Config="Game")` no PlayerController. `abstract` impede
   criar a classe direto: só o Blueprint filho é usado no jogo.

## Três perguntas para considerar a tarefa entendida

1. Você mudou um `.h` do Pawn e adicionou uma `UPROPERTY`. Live Coding serve?
   Se não, o que você faz e por quê?
2. Você quer usar uma classe de áudio de outro módulo do Unreal, e o build
   falha no link. Em qual arquivo você mexe, e o que escreve nele?
3. O Alt+P abre o jogo com uma câmera livre, sem carro. Cite duas coisas que
   você olharia primeiro, e em que arquivo ou log.
