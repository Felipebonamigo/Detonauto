# Os conceitos do Unreal no código real

Sprint 3, tarefa 3. Cada conceito de `docs/conceitos-unreal.md` aparece aqui em
um trecho de código de verdade: do projeto descartável (`ADNTestActor`) ou do
template Vehicle (`AVeiculoTestePawn`). O projeto descartável vive em
`Sandbox/` e não vai para o Git, por isso os trechos estão copiados aqui.

## Actor e Pawn: `class ADNTestActor : public APawn`

```cpp
UCLASS()
class ADNTestActor : public APawn
```

- Um `AActor` é qualquer coisa que existe no mundo: tem posição e pode ser
  criado e destruído.
- Um `APawn` é um Actor que **pode ser controlado**. É a diferença: o Pawn
  recebe entrada de um Controller, e o Actor comum não.
- Por isso só o Pawn tem `SetupPlayerInputComponent`, onde o nosso cubo liga as
  teclas às funções. Um poste (Actor comum) não tem isso.

## Component: o cubo e a câmera são peças do Pawn

```cpp
Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
SetRootComponent(Mesh);

CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
CameraBoom->SetupAttachment(Mesh);
Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
Camera->SetupAttachment(CameraBoom);
```

O `ADNTestActor` não "é" um cubo nem "é" uma câmera. Ele **carrega** uma malha
(`Mesh`), um braço (`CameraBoom`) e uma câmera (`Camera`). O Unreal prefere
compor peças a herdar de tudo. O carro do template faz o mesmo: o
`AVeiculoTestePawn` tem `FrontCamera`, `BackCamera` e os braços de cada uma.

`CreateDefaultSubobject` só pode ser chamado no construtor.

## GameMode e PlayerController: quem põe o Pawn em cena

```cpp
ADNTestGameMode::ADNTestGameMode()
{
    DefaultPawnClass = ADNTestActor::StaticClass();
}
```

- O **GameMode** são as regras da partida. Este diz "o Pawn do jogador é um
  `ADNTestActor`".
- O **PlayerController** é quem assume o Pawn e lê o teclado. Neste projeto
  usamos o padrão do Unreal.
- No template Vehicle, o `AVeiculoTesteGameMode` só define
  `PlayerControllerClass`. O Pawn padrão vem do Blueprint do GameMode.
- Se o Pawn do GameMode não carrega, o Unreal cai em uma câmera livre: foi o
  sintoma do primeiro teste do VeiculoTeste.

## UPROPERTY: expor ao editor e proteger da coleta de lixo

```cpp
UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="DN Teste")
float Speed = 600.0f;

UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="DN Teste")
int32 Score = 0;

UPROPERTY(Transient)
TObjectPtr<UInputAction> MoveAction;
```

- `EditAnywhere` põe `Speed` no painel Details. `BlueprintReadWrite` deixa o
  Blueprint ler e escrever.
- `Score` é `VisibleAnywhere, BlueprintReadOnly`: aparece, mas ninguém muda por
  fora. Só a função `AddScore` muda.
- Em **ponteiro de objeto**, o `UPROPERTY` tem outro papel: o coletor de lixo
  do Unreal só enxerga referências marcadas. `MoveAction` é criada com
  `NewObject` e não pertence a nenhum componente. Sem o `UPROPERTY`, o Unreal
  poderia apagá-la enquanto o jogo roda.
- Exceção do template: `ChaosVehicleMovement` no `AVeiculoTestePawn` é um
  `TObjectPtr` sem `UPROPERTY`. É seguro porque o componente pertence ao
  próprio Pawn, que já o mantém vivo. Para ponteiros que você mesmo cria, mantenha o `UPROPERTY`.

## UFUNCTION e a regra de pontuação separada

```cpp
UFUNCTION(BlueprintCallable, Category="DN Teste")
void AddScore(int32 Amount);
```

`BlueprintCallable` deixa um Blueprint chamar a função. A regra em si (ignorar
negativos, travar em `MAX_int32`) fica em `FDNScore`, uma struct simples sem
Unreal, para os testes de Automation rodarem sem mundo nem física.

## Enhanced Input: o código recebe a ação, não a tecla

```cpp
MappingContext->MapKey(MoveAction, EKeys::W);
Input->BindAction(MoveAction, ETriggerEvent::Triggered, this, &ADNTestActor::Move);
```

`MoveAction` é a ação "mover". O Mapping Context liga a tecla W a ela. A
função `Move` só conhece a ação. Trocar W por um analógico do controle não
muda o código de `Move`.

## Live Coding ou fechar o editor?

- **Live Coding** (editor aberto): só quando você mudou o **corpo** de uma
  função, como a conta dentro de `FDNScore::Add`.
- **Feche o editor e compile pelo Visual Studio** quando mudar um `.h`, criar
  classe, adicionar `UPROPERTY` ou `UFUNCTION`, ou mudar o `Build.cs`.
- Pela linha de comando: `Tools/Unreal/build.ps1` (o editor visual precisa
  estar fechado: ele trava a DLL).

## Armadilhas dos testes

Três coisas que já custaram tempo no lote 7 e valem para todo teste novo.

- **Não use colchetes em `AddExpectedError`.** O padrão se comporta como expressão regular: `"[Registro]"` vira
  "qualquer uma das letras R, e, g, i, s, t, o", casa com quase qualquer mensagem e **engole as falhas do próprio
  teste**. Resultado: teste verde que não testa nada (seis mutantes sobreviveram assim). Use um texto literal sem
  caracteres especiais, por exemplo `"Registro do jogador:"`, e por isso os logs do jogo não levam etiqueta entre colchetes.
- **Subsistema de `GameInstance` precisa de um `UGameInstance` como dono** (`ClassWithin`). `NewObject<T>(GetTransientPackage())`
  dispara um `ensure`, que derruba o primeiro teste que passar por ali. Use `DNTest::NewSubsystem<T>()`
  (`Core/Tests/DNTestData.h`), que cria o dono sem mundo e o mantém vivo.
- **O módulo não usa unity build** (`bUseUnity = false` em `Detonauto.Build.cs`). Com unity, arquivos `.cpp` eram
  juntados em um só e as funções de ajuda com nome curto de testes diferentes (`City`, `Join`, `NaN`) colidiam toda
  vez que um arquivo novo mudava o agrupamento.

**Como saber se um teste tem dentes:** estrague o código de propósito (mutação) e veja o teste ficar vermelho. Se
continuar verde, o teste não vale.

## Para responder sem consultar

1. Qual a diferença entre um `AActor` e um `APawn`?
2. Para que serve o `UPROPERTY` em um ponteiro de objeto?
3. Quando se pode usar Live Coding, e quando é preciso fechar o editor?
