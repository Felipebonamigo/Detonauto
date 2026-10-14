---
name: ler-crash
description: 'Lê o que o Detonauto deixa quando trava ou fecha sozinho (logs e pasta de crash do Unreal), acha a linha de código e transforma em Issue "[Playtest]" com o passo a passo. Use quando o jogo travar, fechar sozinho, um testador mandar log ou pasta de crash, ou o Felipe disser "crashou", "fechou do nada" ou "deu erro".'
---
# Ler um crash

## Onde fica

- Editor e testes: `Saved\Logs\`. O editor grava `Detonauto.log`; os scripts gravam o deles com `-abslog`
  (`vehicle-probe-*.log`, `crash-probe-*.log`, `test-automation.log`) e o `crash-measure.ps1` em
  `Saved\Measure\<data-hora>\<cenário>.log`.
- Crash do editor ou do jogo aberto pelo editor: `Saved\Crashes\<pasta>\` com `CrashContext.runtime-xml`, o log e o
  `UEMinidump.dmp`. O projeto ainda não registrou nenhum.
- Jogo empacotado (quando houver): `%LOCALAPPDATA%\Detonauto\Saved\Logs\` e `...\Saved\Crashes\`.
- Save corrompido não trava: vira `Detonauto.json.corrompido-<data>` ao lado do save (`docs/pendente-playtest.md`).

## Passos

1. Junte os arquivos. Do PC do Felipe, leia direto. De um testador, peça a pasta zipada anexada à Issue "[Playtest]"
   (o modelo da Issue tem o campo de anexo). Anote a versão do jogo e a data.
2. Ache a primeira linha que é do jogo. No log, procure `Fatal error`, `Unhandled Exception`, `Assertion failed` e `Ensure condition failed` (os scripts
   do projeto já filtram `Error:|Fatal`). No `CrashContext.runtime-xml`, a pilha está em `<CallStack>`. A primeira linha
   com `UnrealEditor-Detonauto` (ou `Detonauto`, no empacotado) é a do jogo: ela traz arquivo e linha.
3. Leia as 50 linhas de log antes do erro: o que o jogo estava fazendo (cena, modo, último comando).
4. Ligue ao código: abra o arquivo e a linha, e o `git log` daquele trecho perto da data da versão.
5. Reproduza com a skill `jogar-e-capturar` (no PC). Se reproduziu, o defeito vira teste visto falhar antes da correção.
6. Registre: Issue "[Playtest]" com gravidade "trava" ou "quebra a partida", o trecho da pilha, o arquivo e a linha, e
   como reproduzir. A correção segue a skill `triar-playtest`.

## Armadilhas

- Sem o `.pdb` da mesma compilação, a pilha vem só com endereços. O `.pdb` fica fora do git e fora do depot da Steam;
  guarde o de cada versão que foi para teste.
- `ensure` que falha não fecha o jogo, mas aparece no log como erro: leia também esses.
- Teste Automation que indexa array sem conferir o tamanho derruba o runner inteiro (`docs/estado-atual.md`): parece crash
  do jogo, mas é do teste.
- Log de testador pode ter nome de usuário e caminho do computador dele: não copie isso para a Issue pública.
