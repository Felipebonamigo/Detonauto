---
name: achar-gargalo
description: 'Acha o que está pesando no quadro do Detonauto: qual linha de execução manda, qual função ou passo de GPU, quanto passa da meta (60 fps, 16,6 ms) e onde anotar. Use quando o fps cair, antes de aprovar sistema novo de dano, tráfego, efeito ou multidão, ou quando o Felipe disser "está travando", "caiu o fps", "mede isso" ou "por que pesa".'
---
# Achar o gargalo

A meta: 60 fps (16,6 ms por quadro) num PC com placa no nível do PS5 (CLAUDE.md, regra 5). Orçamento de trabalho:
Game 8 ms, Render 8 ms, GPU 14 ms (`docs/arquitetura.md`). A RTX 4070 Ti do PC é mais forte que o alvo: estimativa
(não medida) de que uma GPU de PS5 levaria perto do dobro do tempo de GPU (`docs/desempenho.md`).

Medir o fps diz *se* pesa; esta rotina diz *o quê*. Sem saber o quê, otimização é chute.

## Passos

1. Reproduza com números: a mesma cena, a mesma câmera, o mesmo preset.
   Hoje: `Tools\Unreal\crash-measure.ps1 -Uproject Detonauto.uproject` (cenários de tráfego e física, já com
   o CSV Profiler; grava `Saved\Measure\<data-hora>\tabela.md` e `relatorio.txt`) e `Tools\Unreal\lookdev-measure.ps1`
   (cena de aparência com o perfil `DN_AlvoPS5`; só imprime no console). O `measure.ps1` ainda depende do projeto do Sandbox.
2. Ache a linha de execução ou a etapa que manda (seções abaixo).
3. Ache a função ou o passo culpado e meça de novo sem ele (desligue o sistema, troque o efeito por nada) para provar.
4. Corrija o culpado e meça de novo nas mesmas condições. Sem antes e depois, não está provado.
5. Anote data, cena, preset, máquina, antes e depois, e o que mudou em `docs/desempenho.md`, na tabela da seção do lote (`| Cenário | Frame média / p95 | Game | Draw | GPU média / p95 | Quadros |`)
   ou no registro geral.

## No Unreal (PC)

1. **Qual linha de execução manda.** Com o jogo rodando, `stat unit`: `Frame` é o total; o maior entre `Game` (lógica),
   `Draw` (preparo da renderização) e `GPU` é o gargalo. `stat unitgraph` mostra no tempo, bom para picos.
2. **Gravar para ler depois.** O perfil em CSV é texto e o Claude lê direto:
   `-csvCaptureFrames=600` na linha de comando, ou `csvprofile start` e `csvprofile stop` no console.
   O arquivo sai em `Saved/Profiling/CSV/`. Cada coluna é um contador (FrameTime, GameThreadTime, RenderThreadTime, GPUTime
   e os stats ligados); some a média, o p95 e o pior quadro.
3. **Achar a função.** Unreal Insights: rode com `-trace=cpu,gpu,frame,bookmark -statnamedevents`; o `.utrace` sai em
   `Saved/Profiling/` (ou no servidor de traces do Insights). Abra no `UnrealInsights.exe`, Timing Insights, e ordene os
   timers por tempo exclusivo na linha de execução que o passo 1 apontou. Para o Claude ler, exporte os timers para CSV
   pelo próprio Insights; antes de pôr o comando de exportação num script, confira o nome exato na documentação do
   Insights da versão 5.8.
4. **GPU.** `ProfileGPU` (Ctrl+Shift+,) escreve no log o custo de cada passo de renderização; `stat gpu` mostra ao vivo.
   Compare com e sem o efeito novo, mesma câmera.
5. Meça sempre com o build de desenvolvimento ou de teste, nunca no editor com a janela pequena; anote a resolução, o
   preset gráfico e o upscaler.


## Armadilhas

- Média esconde engasgo: olhe também o p95 e o pior quadro.
- Primeira execução compila shader e carrega asset: descarte os primeiros segundos.
- Diferença menor que cerca de 1 ms entre rodadas é ruído.
- Janela sem foco trava em 60 fps. Feche qualquer outro Unreal antes (o `crash-measure.ps1` recusa rodar com um editor aberto).
- O `crash-measure.ps1` não liga o perfil `DN_AlvoPS5`: os números dele são da 4070 Ti sem o limite do alvo.
- O Unreal Insights ainda não foi usado no projeto (`docs/desempenho.md` marca "a confirmar"): o primeiro uso confere os
  argumentos e anota no `docs/desempenho.md`.
