# Regras do registro do jogador (save)

Contrato da parte pura (`Source/Detonauto/Core/DNRecords.h/.cpp`). Sem UObject,
sem mundo, sem ler nem gravar arquivo: o texto entra e sai por `FString`.
Quem grava no disco (SaveGame, caminho, backup) fica para depois.

## O que é guardado

Só a **melhor pontuação por cruzamento** (`int32`, >= 0). A medalha NÃO é
guardada: é derivada da pontuação com os limites atuais de `Crossings.csv`
(`FDNMedals`). Assim, mudar um limite no balanceamento não deixa o save
inconsistente.

## Regras

1. `RegisterResult(CrossingId, Score)` guarda `max(atual, Score)`. Devolve
   se foi **novo recorde** (Score estritamente maior que o atual; o primeiro
   resultado de um cruzamento, mesmo 0, conta como novo recorde).
2. `Score < 0` é ignorado (sem erro, devolve falso). Id vazio é ignorado.
3. Ids comparam sem diferenciar maiúsculas (como `FName`); o id guardado é
   o da primeira vez.
4. Nunca diminui: um resultado pior não muda nada.
5. `BestScore(CrossingId)` devolve a pontuação ou "sem registro" (distinto
   de 0).
6. `ToProgressInput(...)`: dado a lista de cruzamentos do jogo com limites,
   gera a lista `FDNCrossingProgress` que `FDNProgress::Compute` já espera
   (cruzamento sem registro = `EDNMedal::None`; registro abaixo do bronze
   também = `None`).

## Texto (JSON)

Formato, versão 1:

    {"version":1,"best":{"exemplo-rio-01":4200,"exemplo-rio-02":0}}

- `Serialize` gera sempre a mesma ordem (ids em ordem alfabética sem
  diferenciar a caixa), para o diff do save ser estável.
- `Deserialize(Text)` devolve `{registro, lista de avisos, bErro}`:
  - texto vazio, JSON inválido, raiz que não é objeto, `version` ausente,
    não numérica ou diferente de 1: `bErro = true`, registro vazio. Versão
    maior que 1 NUNCA é lida "no escuro".
  - entrada com pontuação não inteira, negativa, maior que `MAX_int32` ou
    de tipo errado: ignorada com aviso; as outras entradas valem.
  - id repetido (mesmo ignorando a caixa): vale o **maior**, com aviso.
  - campos desconhecidos na raiz são ignorados (compatível para frente).
- Ida e volta: `Deserialize(Serialize(R))` é igual a `R`.

## Fora de escopo

Dinheiro acumulado, carros desbloqueados, desbloqueio de cidades por
progresso (regra ainda não definida: NÃO inventar), gravação em disco.

## Notas

Usa o módulo `Json` do Unreal (`FJsonObject`) — acrescentar `"Json"` em
`Detonauto.Build.cs`. Se preferir escrever/ler o texto à mão para manter o
`Core/` livre do módulo, justifique e me pergunte antes.

## Decisões de implementação (lote 6)

O contrato acima não cobria; ficaram assim em `Core/DNRecords.h` e nos testes:

- **R1.** Número JSON "inteiro" é o que não tem parte fracionária: `4200` e `4200.0` valem, `4200.5` não, `-0` vale 0.
- **R2.** Numa versão 1, `best` ausente ou que não é objeto é erro (gravar por cima de um save assim apagaria tudo).
- **R3.** Texto sobrando depois do objeto raiz é JSON inválido (erro).
- **R4.** Com erro, a razão vem como primeiro aviso.
- **R5.** Id não é aparado: só o id vazio é ignorado.
- **R6.** `ToProgressInput` devolve um item por cruzamento da lista de entrada, na mesma ordem.
- **R7.** A leitura é em fluxo (`TJsonReader`), não `FJsonObject`: o objeto da engine junta chaves que só diferem na
  caixa (e as repetidas) antes de o código ver, e o contrato manda valer o maior com aviso. Os campos `version` e `best`
  valem só em minúsculas e repetidos são erro.
