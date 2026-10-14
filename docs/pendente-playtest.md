# Pendente de playtest

O que a automação (`Tools/Unreal/test.ps1`) **não** consegue provar sobre a cola entre o `Core` e o motor, porque só
existe com o jogo rodando. Cada item: o que olhar e o resultado esperado. Nada disto depende da sensação de dirigir.

Os subsistemas e os componentes já têm testes sem mundo (`Core/Tests/DNSubsystems.spec.cpp` e `DNComponents.spec.cpp`).
O carro do jogo (`ADNVehicle`, lote 12) já usa o `UDNTurboComponent`; os itens 4 e 5 valem agora para ele e estão no item 7.
O `UDNCrashRoundComponent` ainda não está em nenhum ator.

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

## 7. O carro do jogo no mapa vazio (lote 12)

O que só aparece jogando. Antes: feche o editor, `Tools/Unreal/build.ps1 -Uproject Detonauto.uproject`, e abra com
`Tools/Unreal/vehicle-probe.ps1 -Uproject Detonauto.uproject -Play` (detalhes em [`carro.md`](carro.md)). O log vai para
`Saved/Logs/vehicle-probe-Padrao-play.log`.

1. **Mapa e HUD.** Abre a janela com um carro laranja sobre um piso quadriculado, uma parede cinza ao fundo e cinco linhas de
   texto no alto (preset, velocidade, turbo, amassados, teclas). **Esperado:** nenhum `Error:` no log, e a linha
   `ADNVehicle: preset 'Padrao' aplicado: massa pedida=1500 kg ...`.
2. **Rodas girando e esterçando (o que a automação não prova).** Acelere e vire: as rodas **giram** e as da frente **esterçam**;
   ao cair de uma altura, a roda sobe e desce com a suspensão. A captura parada mostrou as rodas no lugar e do tamanho certo; o
   movimento não foi visto.
3. **Turbo (item 5 antigo).** Segure Shift: a linha do turbo vira `ATIVO`, a barra cai 25 por segundo e o carro acelera mais.
   Esgotar a barra: `vazio`; em modo de teste ela **enche de novo uns 2 s depois**. Soltar o botão no meio desliga na hora.
   Apertar com a barra vazia não liga nada.
4. **Dados chegam ao carro (item 4 antigo).** Troque `MassKg` do `Padrao` em `Data/Cars.csv` para `1200`, abra o jogo: o log diz
   `massa pedida=1200 kg`. Troque por `5` (fora da faixa): o log mostra `Cars.csv, linha 'Padrao': o campo 'MassKg' ...`, o carro
   nasce com os números do template e o jogo abre do mesmo jeito. Desfaça (`git checkout Data/Cars.csv`).
5. **Presets e freio de mão.** 1, 2, 3 e 0 trocam o preset **andando**, sem o carro parar. Em `arcade-2` e `arcade-3`, Espaço numa
   curva derrapa e volta; em `arcade-1` e `Padrao` não derrapa (medido: 32, 17, 4 e 3 graus). Se o `arcade-2` **rodar** (carro de
   lado ou de ré), anote: é o precipício do `docs/presets-carro.md`.
6. **Amassado e dano.** Bata na parede (60 m à frente) a mais de 50 km/h: aparece um amassado e o HUD conta a batida. T/Y/U e G/H/J
   fazem batidas simuladas; V limpa. Passando de 8 amassados o material não mostra mais (tem 8 espaços).
7. **Piso.** O atrito do piso é 1,0 como a pista do template. Se o carro parecer "escorregadio" demais ou de menos, **diga**: o
   número está em `ADNTestArena::FloorFriction` e muda a derrapada de todos os presets.
