---
name: fechar-lote
description: 'Fecha um lote do Detonauto: mede o custo de dano, tráfego ou efeitos (crash-measure.ps1, lookdev-measure.ps1, measure.ps1, registro em docs/desempenho.md), registra a licença de asset novo, faz commits pequenos em português, atualiza docs/estado-atual.md e marca o progresso na página dos sprints. Use para "fechar o lote", "encerrar a sessão", "atualizar o estado atual", "registrar a medição" ou "marcar o sprint".'
---
# Fechar um lote

Fluxo combinado (`docs/estado-atual.md`): contrato com números em `docs/` → sessão local implementa com testes primeiro e
prova por mutação → commits pequenos na `main` → a nuvem revisa e aprova → o Felipe joga quando houver o que sentir.

## 1. Conferir o que já foi provado
- Build e testes verdes e mutação feita (skill `verificar-no-pc`); efeito visual com captura olhada (skill `jogar-e-capturar`).
- O que faltou não some: o lote fecha como **parcial** dizendo o quê (exemplo: lote 13, "falta a medição").

## 2. Medir antes de aprovar (regra 5)
Sistema novo de dano, tráfego ou efeitos só é aprovado medido: pior cenário, antes e depois, máquina anotada. Feche todo
Unreal, inclusive o de outro projeto (`AgeOfEarthUE`), e não mexa no PC: janela sem foco trava em 60 fps e infla a GPU.
```powershell
.\Tools\Unreal\crash-measure.ps1 -Uproject Detonauto.uproject
.\Tools\Unreal\crash-measure.ps1 -Uproject Detonauto.uproject -Scenario C-130-40,P-0,P-40 -Warmup 30 -Measure 60
.\Tools\Unreal\lookdev-measure.ps1 -Uproject Detonauto.uproject
```
- `crash-measure.ps1` (tráfego e Crash, `DNCrashTestGameMode`). `C-<km/h>-<n>[-Physics]`: rodada de Crash (pontos, destruídos,
  medalha, duração, acordaram), em 640x360. `P-<n>[-Physics]`: Frame/Game/Draw/GPU pelo CSV Profiler em `-ResX/-ResY`
  (1920x1080); com `P-0` na lista sai o custo por carro em Game. Padrão: `C-100-20, C-100-40, C-130-20, C-130-40,
  C-130-40-Physics, P-0, P-40, P-40-Physics`; nos `P-`, aquecimento 8 s e medição 10 s (`docs/desempenho.md` pede uns 30 s
  de aquecimento). Recusa rodar com um `UnrealEditor` aberto. Não liga o perfil `DN_AlvoPS5`. Ainda não rodou: fica para
  depois do lote 13b.
- `lookdev-measure.ps1` (visual): `L_Lookdev` no perfil `DN_AlvoPS5`, 1920x1080, cenários `parado` (`-Warmup` 15, `-Measure`
  10) e `andando` (mede de 6 a 14 s). Imprime média / p95 e descarta (saída 1) medição travada em ~60 fps. Com outro Unreal aberto só avisa.
  Rode uma vez antes para compilar os shaders.
- `measure.ps1` **não roda no projeto principal hoje**: `?game=/Script/VeiculoTeste.DNMeasureGameMode`, o mapa
  `/Game/VehicleTemplate/Maps/VehicleBasic`, o log `LogVeiculoTeste` e o carro `DN` (`ADNTestCar`) são do `Sandbox/VeiculoTeste`
  (fora do Git); `Source/Detonauto` não tem `DNMeasureGameMode`. Uso atual: `-Uproject Sandbox\VeiculoTeste\VeiculoTeste.uproject`.
  Levar ao Detonauto = copiar `Tools/Unreal/measure-gamemode/` (o README diz o que ajustar) e trocar o módulo; combine antes.
- `crash-measure` e `measure` gravam em `Saved\Measure\<data-hora>\` (`relatorio.txt`, `tabela.md`, `.csv`/`.log` por cenário).
- Registre em `docs/desempenho.md`, numa seção do lote (modelo "Lote 15"): data, máquina (GPU, CPU, resolução), perfil,
  cenário, Frame/Game/Draw/GPU média / p95, e uma leitura honesta: o que cabe nos 16,6 ms, o Game thread contra os 8 ms de
  `docs/arquitetura.md`, "quanto confiar" e o que **não** foi medido. A 4070 Ti é mais forte que o alvo; diferença menor
  que ~1 ms entre rodadas não é confiável.

## 3. Asset novo
- Nenhum asset entra sem linha em `docs/licencas.csv`: `arquivo_ou_pacote,origem,url,licenca,credito_obrigatorio,
  texto_do_credito,gerado_por_ia,usado_em,data_entrada,observacoes` (exemplo: as texturas do Poly Haven do lote 15).
  `gerado_por_ia` marcado em todo conteúdo de IA que o jogador vê (Steam); código escrito com o Claude não precisa.
- Licenças permitidas e proibidas: CLAUDE.md, "Licenças de assets". City Sample: migre só o que o jogo usa, nunca o pacote.
- Modelo 3D: skill `modelo-3d` (procurar no doc do estúdio e no `acervo-3d`; entrar nos dois no mesmo dia).

## 4. Commits
- Pequenos, um assunto cada, na `main`, em português no imperativo ("Adiciona turbo por contramão"), com o rodapé de
  atribuição que a sessão pedir.
- PowerShell 5.1: `git commit -F -` com here-string não funciona; grave a mensagem num arquivo e use `git commit -F <arquivo>`.
  `Get-Content`/`Set-Content` sem `-Encoding` estragam UTF-8: edite com a ferramenta de edição.
- `Saved/`, `Sandbox/` e pacotes do Fab ficam fora (`.gitignore`). Depois do push: `git log origin/main -3`.

## 5. Documentos de passagem (regra 7)
- `docs/estado-atual.md`: "Última revisão", tabelas "O que existe" e "Lotes", "Armadilhas já descobertas" (só o que custou
  retrabalho), "Pendências do Felipe" e "Próximo passo". Uma sessão nova começa por ele.
- Documento do lote, quando houver (modelos `docs/lote-14.md` e `docs/lookdev.md`: causa, medida, mutação, comandos, capturas,
  o que ficou de fora). O que só se confirma jogando vai para `docs/pendente-playtest.md`.

## 6. Progresso dos sprints
- Página: https://claude.ai/artifact/NkqnCgL7CW5YDtbE5hbSA7 (cronograma geral: https://claude.ai/artifact/UxU6zFtkNGTvHEwYH3ZB98).
  O Felipe marca as caixas lá; o progresso fica no banco da página.
- Leitura pela ferramenta ArtifactData: blocos em `blocks` (`label` "Sprint N", tarefas `t1`, `t2`... com dono
  `voce`/`claude`/`juntos`; o id do bloco não é o número do sprint: `b07` é a Sprint 6) e progresso em
  `progress/<id do bloco>`, campo `done` com `{"<id da tarefa>": true}`. `blocks` e `meta` só mudam na revisão de escopo.
- Lote não é sprint: só marque a tarefa que o lote cumpriu de fato, confirmando com o Felipe. Sem acesso à página, diga no
  relatório final qual bloco e qual tarefa marcar.
