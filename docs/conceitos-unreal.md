# Conceitos do Unreal para quem vem do C#

Guia de apoio da Sprint 3. Explica as peças que aparecem em todo código C++
do Unreal, em linguagem simples. Os exemplos de código aqui são genéricos. Os
nomes reais do template ficam em `docs/estrutura-do-projeto.md`, que a sessão
local escreve a partir do projeto de verdade.

Quem já programa em C# reconhece quase tudo. A diferença é que o Unreal tem
seu próprio sistema de objetos por cima do C++, com coleta de lixo,
serialização e integração com o editor.

## A ideia central: Actor e Component

- **Actor** é qualquer objeto que existe no mundo do jogo: um carro, um poste,
  uma câmera. Tem posição, pode ser criado e destruído.
- **Component** é uma peça que um Actor carrega: uma malha 3D, uma colisão, um
  som, um sistema de partículas. Um carro é um Actor com componentes de
  malha, rodas, câmera e som.
- Em vez de herdar para tudo, o Unreal prefere compor: um Actor com os
  componentes certos. Isso lembra o padrão de componentes do Unity.

## As classes que organizam o jogo

| Classe | Papel | Exemplo no Detonauto |
|---|---|---|
| `AActor` | Qualquer objeto do mundo | Poste, barreira, semáforo |
| `APawn` | Um Actor que pode ser controlado | O carro do jogador, um carro rival |
| `AController` | Quem controla um Pawn. Pode ser uma pessoa ou a IA | Controle do jogador, IA do rival |
| `APlayerController` | Controlador de um jogador humano. Lê os botões | Lê acelerador, freio e turbo |
| `AGameModeBase` | As regras da partida | Regras do modo Crash: largada, pontuação, fim |
| `AGameStateBase` | O estado da partida visível a todos | Pontuação e tempo restante |
| `APlayerState` | O estado de um jogador | Pontos e medalhas do jogador |

Quando o jogo começa, o GameMode cria o PlayerController, e o PlayerController
assume o Pawn. **Se o Pawn do GameMode não carrega, o Unreal usa um Pawn
padrão, que é uma câmera livre.** Foi o que aconteceu no primeiro teste do
VeiculoTeste.

## Os quatro macros que você vai ver em tudo

O Unreal tem um sistema de reflexão. Os macros dizem ao Unreal o que expor ao
editor, ao Blueprint e ao salvamento de dados.

```cpp
UCLASS()
class ADNVehicle : public AWheeledVehiclePawn
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Turbo")
    float TurboMax = 100.f;

    UFUNCTION(BlueprintCallable, Category = "Turbo")
    void AddTurbo(float Amount);
};
```

- **`UCLASS()`** marca uma classe que o Unreal gerencia.
- **`GENERATED_BODY()`** é obrigatório dentro dela. O Unreal gera código aqui.
- **`UPROPERTY(...)`** expõe uma variável. `EditAnywhere` mostra no editor,
  `BlueprintReadWrite` deixa o Blueprint ler e escrever. Uma variável de
  objeto sem `UPROPERTY` pode ser apagada pelo coletor de lixo.
- **`UFUNCTION(...)`** expõe uma função. `BlueprintCallable` deixa o
  Blueprint chamá-la.

O arquivo `.generated.h` que aparece no topo de cada cabeçalho é produzido
pelo build. Não se edita nem se apaga.

## O ciclo de vida de um Actor

1. **Construtor:** configura valores padrão e cria os componentes.
2. **`BeginPlay()`:** roda uma vez quando o Actor entra no jogo. Aqui se
   inicializa o que depende do mundo.
3. **`Tick(float DeltaTime)`:** roda a cada quadro. Use com cuidado: com 40
   carros de tráfego, o custo soma. Desligue o Tick nos Actors que não precisam.
4. **`EndPlay()`:** roda quando o Actor sai do jogo.

## Nomes que parecem estranhos

Os prefixos de classe são uma convenção obrigatória:

| Prefixo | Significa | Exemplo |
|---|---|---|
| `A` | Actor | `ADNVehicle` |
| `U` | Objeto que não é Actor, como componentes | `UDNDamageComponent` |
| `F` | Estrutura simples (struct) | `FDNCrashScore` |
| `E` | Enumeração | `EDNTakedownType` |
| `I` | Interface | `IDNDamageable` |
| `T` | Template | `TArray`, `TMap` |

Os tipos de texto e de lista do Unreal têm nomes próprios: `FString`,
`FName`, `FText`, `TArray`, `TMap`. Use esses, não os de `std::`.

## Entrada de controle: Enhanced Input

O Unreal 5 lê botões pelo Enhanced Input. Você cria **Input Actions**, como
"Acelerar" ou "Turbo", e um **Input Mapping Context** que liga cada ação a um
botão do teclado ou do controle. O código recebe a ação, não o botão. Assim o
mesmo código funciona no teclado, no controle do Xbox e no DualSense.

## Dados em tabelas

O plano do Detonauto guarda ajustes dos carros e dos cruzamentos em CSV. O
Unreal importa o CSV como **DataTable**, a partir de uma `struct` que herda de
`FTableRowBase`. O código lê as linhas pelo nome. Assim você muda a velocidade
máxima de um carro editando o CSV, sem recompilar.

## Dois erros comuns de quem vem do C#

1. **Esquecer o `UPROPERTY` em ponteiros de objeto.** Sem ele, o coletor de
   lixo não enxerga a referência e pode apagar o objeto.
2. **Mudar um cabeçalho com o editor aberto e usar Live Coding.** O Live
   Coding só aguenta mudanças dentro de funções. Ao criar classes, adicionar
   propriedades ou mudar um `.h`, feche o editor e compile pelo Visual Studio.

## Para ler a seguir

- `docs/estrutura-do-projeto.md`: as pastas e as classes reais do template.
- A documentação oficial do Unreal em dev.epicgames.com/documentation, nas
  seções de Gameplay Framework e de Programming.

## Fontes e limites deste guia

O guia foi escrito de memória a partir da arquitetura estável do Unreal 5, e
não foi conferido contra a documentação da versão 5.8. Os conceitos centrais
não mudam entre as versões 5.x. Nomes de classes e de macros de detalhe
devem ser confirmados com o código real do projeto.
