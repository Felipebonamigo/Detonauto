# Estado atual do projeto

Documento de passagem. Uma sessão nova do Claude (local ou na nuvem) lê este
arquivo, o `CLAUDE.md` e o `docs/plano-visual.md` e consegue continuar sem o
histórico das conversas. Atualizar a cada lote fechado.

Última revisão: 14 out 2026.

## Como o trabalho está dividido

- **Sessão da nuvem:** desenho, contratos em `docs/`, revisão do código no
  GitHub, cronograma e conversa com o Felipe.
- **Sessão local (PC do Felipe):** compila, roda testes, joga cenários
  automáticos, escreve C++ e scripts, faz as capturas.
- **Felipe:** joga e decide sensação, visual e escopo.
- Fluxo de cada lote: contrato com números em `docs/` → sessão local implementa
  com testes primeiro e prova por mutação → commits pequenos na `main` → a nuvem
  revisa e aprova → o Felipe joga quando houver o que sentir.

## O que existe (origin/main)

| Área | Onde | Estado |
|---|---|---|
| Regras puras (turbo, Crash, takedown, medalhas, dano, tráfego, globo, progresso, save, roda visual, parâmetros do amassado) | `Source/Detonauto/Core/` | Testadas; mais de 500 testes de Automation |
| Dados de ajuste em CSV com validador e teste de deriva | `Data/` | Cidades, cruzamentos, tráfego, turbo, Crash, takedown, dano, carros |
| Subsistemas de dados e de save | `Core/DNGameDataSubsystem`, `Core/DNRecordsSubsystem` | Testados sem mundo |
| Carro jogável | `Vehicle/ADNVehicle` | Presets por dados, turbo real com FOV, freio de mão com rampa, assistência de derrapagem, 5 câmeras por dados (tecla C), rodas no eixo certo, amassado por material, HUD de depuração |
| Tráfego e rodada de Crash | `Traffic/`, `Vehicle/DNCrashTestGameMode` | Lote 13 parcial: código na main, medição ainda não rodou |
| Mapa de teste | `ADNVehicleArena`, criado por código no `/Engine/Maps/Entry` | Feio de propósito; **não serve para julgar visual** |
| Arte | `Content/Detonauto/Vehicle/SportsCar` | Carro do template da Epic, provisório |

## Lotes

| Lote | Assunto | Estado |
|---|---|---|
| 1–7 | Regras puras, dados, validadores, save, subsistemas e componentes | Aprovados |
| 8 | Presets arcade no Sandbox | Aprovado; substituído pelo lote 12 |
| 9 | Regras do dano | Aprovado |
| 10–11 | Protótipo do amassado e calibração da batida | Aprovados; técnica revista em `docs/plano-visual.md` |
| 12 | Carro no projeto principal | Aprovado |
| 13 | Tráfego real e rodada de Crash | Parcial; falta a medição |
| 14 | Bugs do 1º playtest (rodas, amassado, curva) e ajustes (derrapagem, turbo, câmeras) | Fechado na main (`docs/lote-14.md`): B1, B2, J1–J4 feitos e capturados no `-Play`; B3 medido, causa não achada, proposta aguardando o Felipe; mutação 41 de 43 no código (2 equivalentes), 14 de 14 nos CSV |

## Primeiro playtest (14 out)

Notas em `docs/notas-playtest-1.md`. Resumo: arcade-3 é o mais próximo do gosto
do Felipe; presets 0 e 1 duros; arcade-2 derrapa sozinho; turbo fraco demais;
rodas estavam de lado e borradas; amassado não aparecia; câmera mais longe é
melhor e ele quer cinco modos.

## Armadilhas já descobertas

- `bUseUnity = false` no `Detonauto.Build.cs`; o agrupamento unity fazia funções
  de teste colidirem.
- Nunca usar `[..]` no padrão de `AddExpectedError`: vira classe de caracteres e
  engole falhas.
- `FString ==` ignora maiúsculas e minúsculas; use `Equals(..., ESearchCase::CaseSensitive)` quando importar.
- `FJsonObject` junta chaves que diferem só na caixa; para detectar repetição,
  ler em fluxo com `TJsonReader`.
- Chaos: o atrito da roda é multiplicado pelo do material físico do piso
  (o template usa 1,0; o padrão da engine é 0,7). Qualquer ajuste de derrapagem
  depende do piso.
- Chaos: `SetMassOverrideInKg` não vale; a massa vem do `Mass` do componente.
- Chaos: `RollbarScaling` só é lido ao criar o veículo; trocar preset andando
  exige `RecreatePhysicsState` e zerar a marcha alvo.
- Aderência traseira tem "precipício": produto aderência × freio de mão perto de
  0,30 muda de 12° para 40° de deslize; passos de 0,02.
- Material em malha Nanite precisa da flag de uso com Nanite, senão a engine
  troca pelo material padrão.
- Parâmetro vetorial entrega só RGB ao nó Custom do material.
- Teste verde não prova efeito visível: todo efeito visual precisa de captura no
  modo `-Play` (ver `docs/plano-visual.md`).
- Chaos: giro, esterço e curso da roda vêm no espaço do **componente**; aplicar
  no espaço do osso deita a roda. Conta certa em `Core/DNWheelVisual.h`.
- O Blueprint do template tem números que o C++ do protótipo não tinha (curva de
  torque, freio traseiro 1500). Para comparar com o template, ler o Blueprint por
  C++ no Sandbox (o Python não expõe tudo).
- A frente do `SK_SportsCar` é a grade, sem vértices: amassado por vértice ali não
  aparece. A câmera de perseguição nunca vê a frente nem o lado esquerdo.
- Mutante de CSV não recompila: rodar depois de compilar o código **limpo**, senão
  testa o binário do último mutante de código.
- Teste nunca indexa array sem conferir o tamanho: o índice fora derruba o runner
  inteiro em vez de reprovar um teste.
- PowerShell 5.1: `Get-Content`/`Set-Content` sem `-Encoding` estragam UTF-8, e
  `git commit -F -` com here-string não funciona (passa a mensagem como caminho).
  Editar com a ferramenta de edição; mensagem de commit em arquivo.
- Roteiro de teclas reais: `vehicle-probe.ps1 -Keys "W@0.5-9;C@3;SHOT@4;QUIT@8"`
  entra pelo `PlayerController`, o mesmo caminho do teclado do Felipe.
- Medição de desempenho só com nenhum outro Unreal aberto; `crash-measure.ps1`
  recusa se houver um.
- Blender 5.2.2 LTS em `C:\Program Files\Blender Foundation\Blender 5.2\blender.exe`
  (instalado por winget em 14 out). `Tools/Blender/export_fbx_batch.py` testado:
  escala 1:1, e o +X do Blender chega como +X do Unreal (modelar o carro com a
  frente para +X).

## Pendências do Felipe

- Busca de "Detonauto" no INPI, classes 9 e 41.
- Verify da engine no Epic Launcher (40 arquivos regravados no início).
- Backup em HD externo.
- Playtest do lote 14: roteiro em `docs/pendente-playtest.md`, seção 8. Responder
  o que é a curva "dura" (B3).
- Fechar o outro Unreal aberto (`AgeOfEarthUE`) quando a sessão local for medir
  desempenho (lote 13 e stat unit da cena de aparência).

## Próximo passo

1. Lote 13: medição (`crash-measure.ps1`), com nenhum outro Unreal aberto.
2. Lote 15: plano visual, passos 1 e 2 (base de renderização em texto e cena de
   aparência `L_Lookdev` por script, com capturas antes e depois).
