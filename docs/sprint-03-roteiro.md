# Sprint 3: C++ no Unreal

De 30 de novembro a 11 de dezembro de 2026. Objetivo: escrever o primeiro
código C++ do jogo, mesmo que descartável, e confirmar que o nome Detonauto
está livre no INPI.

A Sprint 3 parte da Sprint 2: projeto `VeiculoTeste` abrindo, scripts
`build.ps1` e `test.ps1` funcionando e `docs/estrutura-do-projeto.md` lido.

## Tarefa 1: busca de Detonauto no INPI (Felipe)

A busca é gratuita e leva uns 20 minutos. Ela é o último item de checagem do
nome. Steam, itch.io, marcas dos EUA e domínios já foram conferidos.

1. Acesse busca.inpi.gov.br e entre na pesquisa de marcas. O sistema pode
   pedir um cadastro gratuito. Se pedir, faça.
2. Pesquise a palavra **Detonauto** na pesquisa por marca, com o tipo
   "exata". Anote quantos resultados aparecem e a situação de cada um
   (registro, pedido, extinto, arquivado).
3. Repita com **Detonauto*** (com o asterisco, se o sistema aceitar) para
   achar nomes que começam igual, e com **Detona Auto**, em duas palavras.
4. Para cada resultado, olhe a classe de Nice. Duas classes importam:
   - **Classe 9:** programas de computador e jogos baixáveis.
   - **Classe 41:** entretenimento, o que inclui serviços de jogos.
   Resultados em outras classes, como a 12 de veículos, têm pouco risco de
   conflito, mas anote também.
5. Salve as capturas de tela e me mande, ou cole os números dos processos.

O que decide:

| Resultado | O que fazer |
|---|---|
| Nenhum resultado nas classes 9 e 41 | Nome livre. Siga para o pedido de marca, na Sprint 13 |
| Resultado em outra classe | Provavelmente livre. Me mande para avaliarmos |
| Resultado em 9 ou 41, extinto ou arquivado | Em geral livre. Me mande o número |
| Resultado em 9 ou 41, em vigor | Risco real. Não anuncie o nome. Voltamos ao nome |

Isto é uma busca de anterioridade, não um parecer jurídico. Antes do pedido
de marca, vale uma conversa rápida com um agente de propriedade industrial.

## Tarefa 2: o primeiro código em C++ (Claude escreve, Felipe testa)

Um projeto descartável em `Sandbox/`: um Actor em C++ com propriedades e
funções, controlado pelo Enhanced Input. A ideia é praticar o ciclo
escrever, compilar, ver no editor.

Escopo do projeto descartável 1:

1. Uma classe `ADNTestActor` que herda de `APawn`, com uma malha simples.
2. Duas propriedades expostas ao editor, `Speed` e `TurnRate`, com
   `UPROPERTY(EditAnywhere)`.
3. Uma função `AddScore(int32 Amount)` chamável pelo Blueprint.
4. Controle por Enhanced Input: uma Input Action para mover e outra para
   girar, ligadas num Input Mapping Context.
5. Uma linha de texto na tela com a pontuação, usando `GEngine->AddOnScreenDebugMessage`.

O Claude, na sessão local, escreve o código e compila. O Felipe só precisa
abrir o editor, apertar Alt+P e ver se o objeto se move e se o texto aparece.

Antes de cada compilação o Claude usa `build.ps1`. Para testes, `test.ps1`.

## Tarefa 3: o jogo dos conceitos (Claude explica, Felipe lê)

O Claude explica Actor, Component, GameMode, PlayerController e Pawn usando
o código real do projeto descartável e do template. O guia genérico está em
`docs/conceitos-unreal.md`.

Depois de ler, o Felipe responde sem consultar:

1. Qual a diferença entre um `AActor` e um `APawn`?
2. Para que serve o `UPROPERTY` em um ponteiro de objeto?
3. Quando se pode usar Live Coding, e quando é preciso fechar o editor?

## Pronto quando

- A busca no INPI está feita e o resultado, registrado em `docs/inpi.md`.
- O projeto descartável 1 compila, roda e responde ao controle.
- Os três itens de conceito foram respondidos sem consulta.
- O progresso foi marcado na página de sprints.
