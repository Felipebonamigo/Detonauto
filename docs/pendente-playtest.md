# Pendente de playtest

O que a automação (`Tools/Unreal/test.ps1`) **não** consegue provar sobre a cola entre o `Core` e o motor, porque só
existe com o jogo rodando. Cada item: o que olhar e o resultado esperado. Nada disto depende da sensação de dirigir.

Os subsistemas e os componentes já têm testes sem mundo (`Core/Tests/DNSubsystems.spec.cpp` e `DNComponents.spec.cpp`).
**Ainda não há ator no mapa usando os componentes.** Os itens 4 e 5 só podem ser feitos depois que alguém os colocar num ator.

## 1. Os dados carregam quando o jogo abre

1. Abra o projeto no editor e aperte **Play** (PIE) em qualquer mapa.
2. Abra **Window > Output Log** e filtre por `LogDetonauto`.
3. **Esperado:** uma linha `Dados do jogo: Tabelas carregadas e válidas.` e nenhuma linha `Dados do jogo:` com erro.
4. Para ver o erro funcionando: feche o jogo, troque um número de `Data/Turbo.csv` por `abc`, aperte Play de novo.
   **Esperado:** linhas `Dados do jogo:` em vermelho dizendo qual arquivo e qual campo, e **o jogo abre do mesmo jeito**
   (erro de dado não derruba). Desfaça a troca depois (`git checkout Data/Turbo.csv`).

## 2. O registro é criado e gravado

1. Com o jogo parado, apague `Saved/SaveGames/Detonauto.json` se existir.
2. Aperte Play. **Esperado:** no log, `Registro do jogador: Sem arquivo de registro ... começando vazio.` e **nenhum** arquivo criado ainda.
3. Quando existir um jeito de terminar um cruzamento (ainda não existe), confira que `Saved/SaveGames/Detonauto.json`
   aparece com `{"version":1,"best":{...}}` e que **não sobra** `Detonauto.json.tmp` ao lado.

## 3. Arquivo corrompido não é perdido

1. Feche o jogo. Crie `Saved/SaveGames/Detonauto.json` com o texto `isto nao e json`.
2. Aperte Play. **Esperado:** erro `Registro do jogador: Arquivo ... corrompido` no log; ao lado do original aparece
   `Detonauto.json.corrompido-<data>` com o seu texto; o jogo começa com o registro vazio.

## 4. Os dados chegam aos componentes dentro de um mundo (precisa de um ator)

O `BeginPlay` dos componentes pega a configuração do `UDNGameDataSubsystem` pelo `GameInstance`. Os testes só chamam
`ConfigureFromData` direto; o caminho do `BeginPlay` com mundo real não foi exercitado.

1. Crie um ator de teste (C++) com um `UDNTurboComponent` e outro `UDNCrashRoundComponent`.
2. Mude `MaxMultiplier` em `Data/Crash.csv` para `3`, aperte Play, chame `CollectMultiplier()` cinco vezes.
3. **Esperado:** `GetMultiplier()` termina em `3`. Com o valor padrão (`8`), termina em `6`.

## 5. Eventos com o jogo rodando (precisa de um ator)

Os testes provam que os eventos disparam uma vez e só quando o valor muda. Falta ver a ordem com os quadros reais:
ligar o turbo (borda de subida do botão), esgotar a barra e encerrar uma rodada de Crash parada. Só dá para conferir
quando houver o carro ligado aos componentes.

## 6. Build empacotado (pendência já registrada em `docs/arquitetura.md`)

Antes do primeiro build empacotado, `Data/` precisa ser copiado para o pacote (`DirectoriesToAlwaysStageAsNonUFS`,
caminho `../Data`), e **ainda não foi testado** se o carregador acha os arquivos lá. Enquanto isso, o item 1 só vale no editor.
