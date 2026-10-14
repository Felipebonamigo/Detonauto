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
| Regras puras (turbo, Crash, takedown, medalhas, dano, tráfego, globo, progresso, save, roda visual, parâmetros do amassado) | `Source/Detonauto/Core/` | Testadas; 622 testes de Automation |
| Base de renderização | `Config/DefaultEngine.ini`, `Config/DefaultDeviceProfiles.ini` | Lumen (GI e reflexos), sombras virtuais, campos de distância, TSR; perfil `DN_AlvoPS5` (`docs/desempenho.md`) |
| Cena de aparência | `Content/Detonauto/Maps/L_Lookdev`, feita por `Tools/Unreal/create_lookdev.py` | Para julgar visual: céu, sol de fim de tarde, rua com texturas CC0, prédios em blocos (`docs/lookdev.md`) |
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
| 14 | Bugs do 1º playtest (rodas, amassado, curva) e ajustes (derrapagem, turbo, câmeras) | Aprovado (`docs/lote-14.md`). B3: não mexer no esterço; o preset padrão virou o arcade-3. Revisão: assistência só com 3 rodas no chão |
| 15 | Plano visual, passos 1 e 2: base de renderização e cena de aparência | Na main, aguardando revisão (`docs/lookdev.md`, `docs/desempenho.md`): capturas antes e depois nos 4 ângulos; Frame 5,1 ms parado e 5,3 ms andando no perfil alvo PS5 (RTX 4070 Ti) |
| 13b | Correções do tráfego achadas na revisão independente | Aprovado (`docs/regras-trafego.md`, seção do 13b) |
| 16 | Visual do carro e da rua: rodas, pintura, vidros, pneus, prédios, mobiliário | Na main, aguardando revisão (`docs/lookdev.md`, seção do lote 16): rodas de lado corrigidas (bug do lote 14), pintura com verniz e flocos, um material por slot, interior mapping, 282 objetos CC0; Frame 4,55 ms parado no perfil alvo PS5 |

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
- O `/Engine/Maps/Entry` tem PlayerStart: "o mapa tem PlayerStart" não distingue
  mapa de verdade. A cena de aparência marca o dela com a tag `DNMapStart`.
- `EditorAssetLibrary.delete_asset` no commandlet diz que apagou, mas o arquivo
  continua no disco e o `new_level` seguinte falha. Scripts de conteúdo refazem no
  mesmo pacote (esvaziar o mapa, apagar os nós do material) em vez de apagar.
- Nomes dos pinos do nó `If` no Python: `"A > B"`, `"A == B"`, `"A < B"`
  (`get_material_expression_input_names` lista os de qualquer nó).
- O `Saved/Config/WindowsEditor/GameUserSettings.ini` da máquina reaplica os `sg.*`
  por cima do perfil de dispositivo. O `-AlvoPS5` repete os valores do perfil por
  console. Não editar o arquivo do usuário.
- Janela do jogo sem foco fica travada em 60 fps mesmo com `t.MaxFPS 0`, e o tempo
  de GPU parece 3 vezes maior. Medir sem mexer no PC; o `lookdev-measure.ps1`
  descarta medição travada.
- Exposição física de dia (EV 13 a 15) pede `r.EyeAdaptation.CachedLightingPreExposure=8`;
  com o padrão 4 o Lumen corta a luz acima de EV 12.
- Roteiro `-DNKeys` vai entre aspas (pode ter espaço, como `EXEC:csvprofile start`).
- Python 5.8: o enum `MaterialProperty` não tem o pino de verniz (ClearCoat); o
  caminho é `MakeMaterialAttributes` (tem `ClearCoat` e `ClearCoatRoughness`) ligado
  em `MP_MATERIAL_ATTRIBUTES`. Substrate está desligado por padrão.
- Corpos cinemáticos não colidem entre si: carro em trilho só não atravessa o da
  frente porque segue a regra `FollowSpeed`.
- Material físico de atrito baixo precisa de `FrictionCombineMode = Min` (com
  override); na média com o piso de atrito 1,0 o carro frearia do mesmo jeito.
- Os ossos das rodas do SportsCar estão girados ±90 graus no pose de referência:
  malha presa a osso não pode herdar a rotação dele. Prova de roda é captura de
  LADO (de trás o erro se disfarça).
- O SportsCar do template não tem vidro de janela nem interior: janelas são o slot
  do chassi. Emissivo em cena de dia com exposição física precisa de milhares de
  nits para aparecer.
- Substituição de texto em PowerShell com "`n" dentro de script passado ao
  Claude Code estraga linhas: editar código só com a ferramenta de edição.

## Pendências do Felipe

- Busca de "Detonauto" no INPI, classes 9 e 41.
- Verify da engine no Epic Launcher (40 arquivos regravados no início).
- Backup em HD externo.
- Playtest do lote 14: roteiro em `docs/pendente-playtest.md`, seção 8. Responder
  o que é a curva "dura" (B3).
- Fechar o outro Unreal aberto (`AgeOfEarthUE`) e não usar o PC enquanto a sessão
  local mede desempenho (lote 13).
- Olhar as capturas da cena de aparência (`docs/lookdev.md`) e dizer se a luz de
  fim de tarde é a direção certa.
- Proposta de recursos do Unreal decidida pela nuvem em 14 out (ver o fim de
  `docs/proposta-recursos-unreal.md`); o Felipe pode vetar qualquer item.

## Próximo passo

1. Lote 13c: pontuação por fração de dano (`docs/regras-de-jogo.md`, "Revisão de
   14 out"), com nova medição a 100 e 130 km/h com 40 carros; não recalibrar as
   medalhas sem o par.
2. Unreal MCP: o Felipe autorizou no chat local (14 out). Ligar só se os plugins
   existirem no 5.8 e com ele fora do PC; os scripts Python seguem oficiais.
3. Lote 13: medição de custo (`crash-measure.ps1`), com nenhum outro Unreal aberto.
4. Lote 17: dano visual por zona (`docs/regras-dano-visual.md`).
5. City Sample: o Felipe precisa baixar no Fab (prédios, mobiliário e veículos);
   é o caminho para a cidade e o carro finais (`docs/lookdev.md`).
