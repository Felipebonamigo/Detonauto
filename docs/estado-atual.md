# Estado atual do projeto

Documento de passagem. Uma sessão nova do Claude (local ou na nuvem) lê este
arquivo, o `CLAUDE.md` e o `docs/plano-visual.md` e consegue continuar sem o
histórico das conversas. Atualizar a cada lote fechado.

Última revisão: 15 out 2026.

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
| Regras puras (turbo, Crash, detona, medalhas, dano, tráfego, globo, progresso, save, roda visual, parâmetros do amassado, peso dos alvos de deformação, mobiliário destrutível, câmera de velocidade e de batida, peças que soltam e vidros) | `Source/Detonauto/Core/` | Testadas; 710 testes de Automation |
| Base de renderização | `Config/DefaultEngine.ini`, `Config/DefaultDeviceProfiles.ini` | Lumen (GI e reflexos), sombras virtuais, campos de distância, TSR; perfil `DN_AlvoPS5` (`docs/desempenho.md`) |
| Cena de aparência | `Content/Detonauto/Maps/L_Lookdev`, feita por `Tools/Unreal/create_lookdev.py` | Para julgar visual: céu, sol de fim de tarde, rua com texturas CC0, prédios em blocos (`docs/lookdev.md`) |
| Dados de ajuste em CSV com validador e teste de deriva | `Data/` | Cidades, cruzamentos, tráfego, turbo, Crash, detona, dano, carros, mobiliário |
| Subsistemas de dados e de save | `Core/DNGameDataSubsystem`, `Core/DNRecordsSubsystem` | Testados sem mundo |
| Carro jogável | `Vehicle/ADNVehicle` | Presets por dados, turbo real com FOV, freio de mão com rampa, assistência de derrapagem, 5 câmeras por dados (tecla C), rodas no eixo certo, carroceria que amassa por zona (alvos de deformação) com o amassado local por cima, câmera de velocidade (FOV, tremor, desfoque nas bordas) e de batida (câmera lenta, órbita, controle do destroço), peças que penduram e soltam e vidros que trincam e estouram (`Vehicle/ADNCarPart`, `Vehicle/ADNGlassShards`), tecla R que recomeça na largada com tudo reposto (`DNRestart`, também pelo console), HUD de depuração |
| Mobiliário destrutível | `Crash/ADNBreakableProp`, `Crash/UDNBreakableSubsystem` | Poste, árvore, banco, lixeira e hidrante da `L_Lookdev` caem quando o carro bate; o carro segue perdendo de 2% a 23% da velocidade; destroço nunca segura o carro e é varrido da frente (`docs/lote-17c.md`) |
| Tráfego e rodada de Crash | `Traffic/`, `Vehicle/DNCrashTestGameMode` | Lote 13 medido em 15 out (`docs/desempenho.md`): 0,016 ms de Game por carro em trilho; nenhuma rodada destrói carro, os pontos vêm do dano (13c) |
| Mapa de teste | `ADNVehicleArena`, criado por código no `/Engine/Maps/Entry` | Feio de propósito; **não serve para julgar visual** |
| Arte | `Content/Detonauto/Vehicle/SportsCar` | Carro do template da Epic, provisório; `SK_DNSportsCarBody` é a carroceria dele com os 5 alvos e os vãos, refeita por `Tools/Unreal/damage-body.ps1`; as 19 peças e vidros ficam em `SportsCar/Parts`, cortados por `Tools/Blender/split_car_parts.py` (`damage-body.ps1 -Parts`) |

## Lotes

| Lote | Assunto | Estado |
|---|---|---|
| 1–7 | Regras puras, dados, validadores, save, subsistemas e componentes | Aprovados |
| 8 | Presets arcade no Sandbox | Aprovado; substituído pelo lote 12 |
| 9 | Regras do dano | Aprovado |
| 10–11 | Protótipo do amassado e calibração da batida | Aprovados; técnica revista em `docs/plano-visual.md` |
| 12 | Carro no projeto principal | Aprovado |
| 13 | Tráfego real e rodada de Crash | Medido em 15 out (`docs/desempenho.md`); aguardando revisão |
| 14 | Bugs do 1º playtest (rodas, amassado, curva) e ajustes (derrapagem, turbo, câmeras) | Aprovado (`docs/lote-14.md`). B3: não mexer no esterço; o preset padrão virou o arcade-3. Revisão: assistência só com 3 rodas no chão |
| 15 | Plano visual, passos 1 e 2: base de renderização e cena de aparência | Na main, aguardando revisão (`docs/lookdev.md`, `docs/desempenho.md`): capturas antes e depois nos 4 ângulos; Frame 5,1 ms parado e 5,3 ms andando no perfil alvo PS5 (RTX 4070 Ti) |
| 13b | Correções do tráfego achadas na revisão independente | Aprovado (`docs/regras-trafego.md`, seção do 13b) |
| 16 | Visual do carro e da rua: rodas, pintura, vidros, pneus, prédios, mobiliário | Aprovado (`docs/lookdev.md`); polimento no 16b |
| 13c | Pontuação por dano no Crash | Aprovado (`docs/regras-de-jogo.md`, revisão de 14 out): livro por veículo, P1 a P6, mutação 7 de 7 |
| 17 | Dano visual, camada 1: deformação grande por zona | Aprovado (`docs/lote-17.md`): regra com mutação 18 de 19 (1 equivalente), carroceria com 5 alvos gerada no Blender. Leitura da revisão: ainda "macio", não chapa amassada; segue no 17b |
| 16b | Polimento visual: pintura, poste moderno, folhagem, vitrines | Na main, aguardando revisão (`docs/lookdev.md`, seção 16b) |
| 17b | Desgaste da pintura por zona e vincos mais duros | Na main, aguardando revisão (`docs/lote-17b.md`): capturas dos níveis nos 4 ângulos com o sol por trás, batida real a 100 km/h na `L_Lookdev`, lado a lado com o 17, frente em 1,0 mais destruída (recuo em rampa e capô em tenda). Custo com 37,8 mil vértices pendente (PC ocupado por outras sessões) |
| 17c | Mobiliário destrutível | Aprovado na mecânica (`docs/lote-17c.md`): 16 de 16 peças derrubadas a 60 km/h, perda medida igual à da regra, custo +0,18 ms na linha do jogo. Contrato: sai `IgnoreCarS`, entram o varrer (B9), o orçamento pela peça mais lenta (B10) e o tombo em volta do pé (B3). Falta o Felipe jogar |
| 18 | Câmera de velocidade e de batida | Aprovado (`docs/lote-18.md`, revisão de 15 out, com as três mudanças de contrato): FOV, tremor e desfoque nas bordas pela velocidade; câmera de batida com câmera lenta, órbita que mostra o amassado e não entra em prédio; controle do destroço medido (−1,24 m / +1,28 m). Passe do desfoque 0,038 ms. Pendentes: quadro inteiro com e sem desfoque com o PC livre, e o Felipe jogar |
| 19 | Efeitos de batida (Niagara) | Em andamento (`docs/lote-19.md`): os 9 sistemas `NS_DN*` montados pelo MCP em `Content/Detonauto/Effects`, compilando e aparecendo nas capturas (faíscas, cacos, fumaça de pneu, fumaça e fogo do motor, poste e árvore; `docs/img/efeitos-lote19/`). Custo com 6 carros destruídos: +0,1 ms de GPU (`docs/desempenho.md`); falta isolar o Niagara (outro Unreal abriu no meio), uma captura do raspão e o Felipe julgar. Receita e achados no `lote-19.md` |
| 18b | Peças que soltam e vidros que quebram | Na main, aguardando revisão (`docs/lote-18b.md`, `docs/regras-pecas.md`): 11 peças (Presa → Pendurada → Solta) e 8 vidros e lentes, malhas cortadas no Blender por script; trinca procedural no para-brisa; cacos sem Niagara. Itens 1 a 7 com capturas e custo (+0,11 ms de Frame e +0,16 de Game por carro com as peças presas). Achados: P5 (o empurrão atravessava o carro) e a câmera entrando no carro. Falta o Felipe jogar |

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
  recusa se houver um, e o `lookdev-measure.ps1` descarta o cenário se houver um
  antes ou depois dele. Outras sessões (TDFende, AgeOfEarth) usam a Unreal no
  mesmo PC: espere 60 s seguidos sem nenhuma aberta antes de medir.
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
- Python do Unreal: iterar um `unreal.Array` de structs (`for slot in materials`)
  devolve cópias; mexer nelas não muda o array. Mexa por índice e devolva
  (`slots[i] = slot`) e confira relendo.
- Malha com alvos de deformação precisa da flag `used_with_morph_targets` no
  material-base, senão o jogo mostra o xadrez cinza (`WorldGridMaterial`).
- Roteiro montado em PowerShell: número formatado com `-f` sai com vírgula
  (`0,3`) e o console lê 0. Formatar com `CultureInfo.InvariantCulture`.
- Saída de `*>` no PowerShell 5.1 é UTF-16: `grep` do Bash não acha texto nela
  (use `iconv -f utf-16` ou `Get-Content -Encoding unicode`).
- `Set-Content -Encoding utf8` no PowerShell 5.1 grava BOM: mensagem de commit
  feita assim sai com um caractere invisível no título (aconteceu no 9f66cab).
  Mensagem de commit só pela ferramenta de escrita de arquivo.
- Unreal MCP: os plugins `Experimental/Toolsets/*` precisam da regra de
  `GameFeatureData` no `DefaultGame.ini` (senão todo commandlet sai com 1), e no
  `-game` deixam tracebacks de Python inofensivos, filtrados por lista explícita
  em `Tools/Unreal/Find-Unreal.ps1`.
- Nada de `ConstructorHelpers` para assets no construtor do carro: o que ele carrega
  na partida de todo commandlet fica preso como raiz, e refazer esses assets por
  script quebra (`Assertion failed: !IsRooted()`). O `ADNVehicle` guarda caminhos
  (`TSoftObjectPtr`, caminho completo `Pacote.Objeto`) e carrega em
  `PreRegisterAllComponents`; a pasta do carro está em `DirectoriesToAlwaysCook`.
- `delete_all_material_expressions` (5.8) apaga só metade dos nós por chamada:
  chamar até zerar (`create_dent_material.py`). Os restos se acumulavam e criaram
  duas saídas do verniz, e o material deixou de compilar (pintura cinza).
- Scripts de conteúdo regravam assets iguais com binário novo: antes do commit,
  desfaça (`git checkout --`) os que não mudaram de propósito, para não gastar LFS.
- Chaos: o traço da suspensão das rodas corre no canal **WorldDynamic** (resposta em
  `WheelTraceCollisionResponses`). Ignorar o carro só no canal Vehicle não basta: as
  rodas sobem no objeto e o carro é lançado sem contato no chassi, e nenhum evento
  de batida aparece. Destroço tem tipo próprio (`ECC_DNDebris`), que as rodas ignoram.
- Contato que não aparece no log não é prova de que não houve contato: o `OnCarHit`
  sai cedo para peça já quebrada. Para achar quem freia o carro, logar em cada
  fronteira (contato com destroço, rodas, chassi) antes de mudar a regra.
- Captura de 1920x1080 trava o quadro por ~0,3 s: o tempo de jogo anda, a física só o
  passo máximo, e uma sequência "+0,15 s, +0,3 s..." sai quase igual. Para sequência
  de capturas, `-ExtraArgs '-UseFixedTimeStep','-FPS=60'`.
- Chaos: velocidade e giro aplicados a um corpo valem para o **centro de massa**. Peça
  que tomba em volta do pé precisa de `v_cm = v_pé + ω × (cm − pé)` (B3), senão o pé
  volta para trás.
- Malha grande (árvore) com a colisão automática vira um convexo do tamanho dela, que
  encosta nos prédios ao simular. As malhas `UCX_` no FBX não servem na 5.8 (a
  importação gera convexos a mais e dispara um `ensure`). O `split_prop.py` grava um
  `collision.json` e o `create_lookdev.py` aplica cápsulas analíticas.
- **LFS (regra desde 15 out 2026):** asset grande se itera localmente e entra em UM commit,
  com a versão final, nunca um commit por tentativa. O histórico já tem ~870 MB; a copa do
  jacarandá (121 MB) entrou três vezes no 17c. Textura de modelo no máximo 2k. Antes do lote 20,
  fazer uma árvore leve (tronco decimado, folhas em cartões, alvo de 20 MB ou menos).
- Destroço (`DNDebris`) ignora o carro: um empurrão que aponte para dentro do carro faz a peça
  sumir dentro da carroceria. As peças soltas saem para fora pelo lado delas (P5 em
  `Core/DNCarParts.h`), e a câmera atravessa as peças (canal `Camera` ignorado), senão a
  esfera da câmera de batida para nelas e entra no carro.
- Blender: a peça montada pelo bmesh perde as normais personalizadas. Grave as normais
  num atributo de canto antes de cortar e reaplique com `normals_split_custom_set`
  (`split_car_parts.py`). A transferência pela face mais próxima pega a casca errada.
- Importação FBX descarta o alvo de deformação que não desloca nenhum vértice: só a
  carroceria pode exigir os 5 alvos; peça longe de uma zona chega com menos.
- HLSL de nó Custom: `line` é palavra reservada (tipo de primitiva). O material compila
  no editor e cai para o material padrão no jogo; o log do jogo mostra `error X3000:
  syntax error: unexpected token 'line'`. Confira o log depois de mexer no HLSL.
- Python da 5.8: `set_material_instance_static_switch_parameter_value` devolve `False`
  mesmo quando liga a chave. Confira lendo o valor de volta
  (`get_material_instance_static_switch_parameter_value`).
- Unreal MCP: a sessão do app do Claude (Code) não carrega o escopo local do Claude Code,
  só os servidores de usuário e o `.mcp.json` do projeto. Por decisão do Felipe (15 out), o
  `unreal-mcp-detonauto` também está no escopo de usuário (`claude mcp add --scope user`,
  `http://127.0.0.1:8010/mcp`). A porta é só deste jogo: Age of Earth 8020 e TDFende 8030
  não colidem. O servidor só responde com o editor aberto, e a sessão precisa começar
  com o editor já aberto (ela não recarrega servidores).
- O `-DNPropLine` imprime um veredito no QUIT (`DNPropLine: resumo ... APROVADO`): é
  o teste no motor do "bater e seguir". Contra peça que não deve quebrar (10 km/h no
  poste) ele diz REPROVADO de propósito.
- Câmera lenta (`SetGlobalTimeDilation`): o `DeltaSeconds` do tick e o `GetTimeSeconds`
  andam no tempo do jogo. Câmera e capturas usam `FApp::GetDeltaTime()` (tempo real); o
  roteiro `-DNKeys` continua no tempo do jogo e estica durante a câmera lenta.
- Custo de um passe só: `EXEC:profilegpu` no roteiro grava o tempo de GPU de cada passe
  no log (`PostProcessMaterial ... Material=M_...`). Com outra sessão usando a GPU, o
  passe varia pouco e o quadro inteiro varia muito.
- Material de pós-processamento com nó Custom: ligar um nó `SceneTexture`
  (`PostProcessInput0`) numa entrada do Custom e amostrar com
  `SceneTextureLookup(Parameters, UV, 14, true)` (a versão com `Parameters` desfaz a
  pré-exposição, como o nó); UV por `ViewportUVToSceneTextureUV` e
  `ClampSceneTextureUV` (`create_edge_blur_material.py`).

## Pendências do Felipe

- Busca de "Detonauto" no INPI, classes 9 e 41.
- Verify da engine no Epic Launcher (40 arquivos regravados no início).
- Backup em HD externo.
- Playtest do lote 14: roteiro em `docs/pendente-playtest.md`, seção 8. Responder
  o que é a curva "dura" (B3).
- Olhar as capturas da cena de aparência (`docs/lookdev.md`) e dizer se a luz de
  fim de tarde é a direção certa.
- Proposta de recursos do Unreal decidida pela nuvem em 14 out (ver o fim de
  `docs/proposta-recursos-unreal.md`); o Felipe pode vetar qualquer item.

## Próximo passo

1. O Felipe joga o playtest 2 (`docs/roteiro-playtest-2.md`): lotes 14 a 18b, com a
   tecla R para recomeçar. O lote 18b foi aprovado pelo par em 15 out.
2. Medições do PC livre feitas em 15 out (`docs/desempenho.md`): desfoque do 18 no
   quadro inteiro (+0,01 a +0,04 ms), alvos com a carroceria densa (17b, nada) e o
   lote 13.
3. Lote 19: os 9 sistemas Niagara estão montados (`docs/lote-19.md`). Custo medido (+0,1 ms de GPU com 6 carros destruídos); falta isolar o Niagara com o PC livre
   e o Felipe julgar o visual. Pendência da revisão: escurecer o fundo do porta-malas e o difusor
   cinza-claros (textura do chassi do template) quando mexer no material de novo.
   Lotes 20 e 21 (`docs/fatia-vertical.md`): primeiro cruzamento e som base. Antes do
   20: árvore leve (20 MB ou menos) e texturas de modelo em 2k.
4. **Antes de pôr peças no tráfego (lote 20):** cada carro com peças tem 27
   componentes de malha com esqueleto presos ao chassi, e isso custa +0,16 ms de
   Game por carro (`docs/desempenho.md`, 18b). Medir o tick deles desligado com a
   peça presa, ou a pose do chassi como líder (`SetLeaderPoseComponent`), antes de
   multiplicar por 40 carros.
5. City Sample: o Felipe precisa baixar no Fab (prédios, mobiliário e veículos);
   é o caminho para a cidade e o carro finais (`docs/lookdev.md`). O carro dele
   passa pelo `Tools/Unreal/damage-body.ps1` para ganhar os alvos.

**Carros nacionais (16 out, decisão do Felipe):** o Speed Tchê faz primeiro os mesmos carros, com desenho parecido ao do Detonauto, só que
cartunizado (aqui é realismo). Esperar o desenho de lá e reaproveitar o que for comum (proporções, nomes, divisão em peças, moldes no
`acervo-3d`) em vez de modelar às cegas. O Cuiabano daqui fica parado no que já foi commitado.

**Rumo (16 out, decisão do Felipe):** carros nacionais pausados (Cuiabano em andamento, `Tools/Blender/make_cuiabano.py`); a prioridade
é uma fatia jogável com carro, pista de cidade, corrida com detona, Crash, derby e arremesso. Desenho a aprovar em
`docs/fatia-jogavel.md`.

**Fatia jogável, marco 1a (17 out): pista cinza jogável.** A corrida ponto a ponto roda: `ADNRaceGameMode` monta o percurso 1 (1976 m, 14 m de largura, 3 curvas, 7 pontos de verificação e chegada, linha em `Data/CoursePoints.csv`), põe o carro na largada e conta progresso, pontos de verificação e chegada pelas regras do Core (`FDNCourse`, `FDNRace`). O piloto automático (`-DNRaceAuto`) completa o percurso em 80,9 s a ~100 km/h, no máximo a 4,6 m da linha central, com a suíte em 767 de 767. Para jogar: `.\Tools\Unreal\vehicle-probe.ps1 -Uproject Detonauto.uproject -GameMode DNRaceGameMode -Play`. **Diversão:** já dá para correr de A a B com curvas e ver o tempo, mas sem rival, sem detona e sem tráfego ainda, então não tem desafio. **Visual:** nenhum ganho: chão verde, asfalto cinza liso, pórticos amarelos e céu preto; a cena final é o passo a2 (asfalto e calçada do City Sample) e a3 (prédios). City Sample: cópia em `D:\CitySample` (fora do repositório), orçamento de 3 GB no repo (`docs/fatia-jogavel.md`); desenho em `docs/percurso-1.md`.

**Fatia jogável, marco 1a+ (17 out): a pista agora é uma rua de cidade.** Migrados do City Sample ~1,5 GB (`Content/Road`, `Building` kit CH/C L10_A, `Material`, `Textures`, `Megascans`; orçamento de 3 GB; linhas em `docs/licencas.csv`). O percurso 1 tem asfalto molhado e desgastado, calçada, meio-fio, prédios de 4 a 10 andares, céu e névoa; o carro corre por ela (80,9 s no piloto automático). **Diversão:** igual ao marco anterior (sem rival, detona nem tráfego), mas correr entre prédios dá sensação de velocidade que a pista cinza não dava. **Visual:** salto grande: de pista cinza com céu preto para um desfiladeiro de prédios com céu azul, sombras e reflexo no asfalto. Ressalvas: fachadas sem quinas, sem mobiliário, vãos nas curvas, tudo bem claro (exposição) e texturas em 8K; custo (quadros) ainda não medido no perfil alvo PS5.

**Fatia jogável, marco 1b (17 out): rivais.** A corrida agora tem 3 rivais (`Data/Rivals.csv`) que saem da grade à frente do jogador, que larga em último depois de uma contagem de 3 s, e correm o percurso inteiro dirigindo pela regra pura `FDNRivalDriver`. Medido com o jogador no piloto automático: Veloz 69,6 s, Firme 74,6 s, Cauteloso 80,8 s, jogador 83,0 s. HUD de depuração com posição (`Posicao: 4 / 4`), tempo e progresso; classificação no log. Suíte: 789 de 789. **Diversão:** primeira vez que há contra quem correr: dá para ver os carros à frente e querer passar. Ainda sem contato (eles não desviam nem batem de volta) e sem detona, então ganhar ainda é só ser mais rápido. **Visual:** grade de largada em asfalto entre prédios, carros iguais (todos vermelhos). Custo (quadros) com 4 carros ainda não medido no perfil alvo PS5.

**Fatia jogável, marco 1c (17 out): detona e turbo na corrida.** Bater num rival com força (>= 20 km/h relativos e dano de uma zona a ~25% dentro de 1,5 s) dá detona: o turbo do jogador enche, o rival fica destroçado 3 s e volta à pista; HUD de depuração mostra "DETONA! <rival> (<tipo>)", o contador e o turbo. Regras novas com teste e mutação (`FDNRaceTakedown`, W1 a W4); suíte: 799 de 799. **Diversão:** agora há o que fazer com os rivais além de passá-los: derrubar enche o turbo, a recompensa do gênero. Falta o outro lado: o rival ainda não bate de volta. **Visual:** sem mudança (o destroço do rival é o dano que o carro já tinha). Ressalvas: o jogador destruído não tem o que fazer (sem tela de fim, marco 1d); a janela de dano 0,05 é um palpite a ajustar jogando; só provado com o modo de teste `-DNRaceHunt`, ainda não jogado por você. Custo com 4 carros não medido.

**Fatia jogável, marco 1d (17 out): HUD e tela de resultado. O marco 1 fecha.** A corrida tem HUD de verdade (posição, tempo, velocidade, turbo, detonas, progresso de todos, contagem e aviso de detona) e uma tela de resultado com a classificação. R recomeça a corrida inteira. Textos testados (`FDNRaceFormat`); suíte: 804 de 804. Para jogar: `.\Tools\Unreal\vehicle-probe.ps1 -Uproject Detonauto.uproject -GameMode DNRaceGameMode -Play`. **Diversão:** dá para jogar uma corrida do começo ao fim e saber como foi: largada, 3 rivais, detona que enche o turbo, chegada com classificação, "mais uma" com R. É a primeira vez que o jogo fecha um ciclo. **Visual:** o HUD é simples (texto, barras, painel escuro), legível sobre o céu claro e o asfalto; sem identidade visual ainda. Ressalvas que o playtest do Felipe precisa julgar: dificuldade (os rivais correm entre 69 e 81 s e o jogador manual ainda não foi medido), o 0,05 de dano do detona, o carro único e vermelho para todos, e a falta de som. Custo (quadros) com 4 carros, prédios e HUD ainda sem medir no perfil alvo PS5.

**Nome Detona (17 out, decisão do Felipe):** "takedown" é nome de outro jogo e saiu de todo texto que o jogador lê (HUD "Detonas", aviso "DETONOU!", tela de resultado) e dos documentos de design e de regras. Os nomes internos de código ficam por enquanto (`FDNTakedown`, `FDNRaceTakedown`, `Data/Takedown.csv`, `TakedownSetsBarTo`, `ADNRaceGameMode::Takedowns`, as linhas de log "TAKEDOWN"); a renomeação para `FDNDetona`, `Detona.csv` etc. fica para um commit dedicado, quando houver folga, sem misturar com feature.

**Corrida, revisão do par (17 out):** carro do jogador destruído renasce na linha em 2,5 s (a corrida não acaba mais por isso); detona vale mesmo se o jogador também se destrói, desde que o rival tenha sido batido primeiro; rival tocado revida por 2 s; HUD nítido (fonte de contorno por distância). Regras novas W5 e W6 com teste e mutação; suíte: 814 de 814. **Diversão:** destruir o carro deixa de ser o fim da corrida e passa a ser um custo de tempo; o rival que revida dá o que fazer depois do detona. **Visual:** o texto do HUD ficou nítido (era o maior defeito visual do quadro); a rua segue igual (próximos passos do par: fachadas variadas, calçada, poste e árvore). Falta o Felipe jogar o `docs/roteiro-playtest-3.md`.

**Maior buraco visual que resta (revisão do lote 18):** o carro de tráfego é uma caixa
colorida. Batida no tráfego não dá para o Felipe julgar assim. O carro de verdade vem com o
City Sample Vehicles (lote 20); o download no Fab é do Felipe.

Pendências de design (decidir com playtest, não fazer agora):

- **Lote 20, o modo do tráfego muda a pontuação 5 vezes** (medição do lote 13, 15 out):
  no C-130-40, com o mesmo cruzamento, Rail deu 2 186 pontos (bronze) e Physics 11 162
  (prata). O modo do tráfego não pode decidir a medalha. Investigar de onde vem a
  diferença: massa efetiva do Rail acordado, atrito, tempo para acordar e quanto dano
  cada carro recebe. Igualar, ou explicar e escolher um modo só para o Crash.
  - **Leitura do código (16 out; hipótese, não medida):** os eventos de impacto são parecidos
    (10 962 no Rail, 9 625 no Physics), então a diferença está no dano por impacto, não na
    contagem. O `Physics` de todos mantém a velocidade de cada carro a cada quadro até o
    primeiro contato (`bMaintainSpeed`, `ADNTrafficVehicle::Tick`) e **ignora o "seguir o
    carro da frente"** (`FollowSpeed`, só no Rail). No relatório, os carros acordados andam a
    29,7 m/s no Physics e a 7,7 m/s no Rail. Velocidade de aproximação maior dá Δv maior e
    dano (e pontos) muito maiores. Massa e atrito são os mesmos nos dois (a massa vem do mesmo
    `MassKg` no acordar). Se for isso, o Physics de todos nunca foi um modo de jogo, só um teste
    de custo, e o Rail é o que vale para o Crash; falta provar com um log por carro (Δv do
    primeiro impacto e pontos, por modo) antes de fixar as medalhas.

- A 130 km/h a rodada rende MENOS que a 100 (o carro é desviado no primeiro
  impacto): o contrário do que um arcade quer. Alavanca provável: proporção de
  massa jogador × tráfego nas batidas e/ou controle do carro em câmera lenta
  depois da batida.
- Variação grande entre rodadas iguais (física em passo livre): avaliar subpasso
  fixo do Chaos (passo fixo/assíncrono), medindo efeito na direção e no custo;
  ranking exige repetibilidade.
- Medalhas do cruzamento de exemplo ficam em 1000/5000/12000 até existir o
  primeiro cruzamento de verdade (a estrada reta não é cruzamento).

Unreal MCP: ligado em 14 out (plugins ModelContextProtocol e AllToolsets, só no
editor; porta 8010 em `Config/DefaultEditorPerProjectUserSettings.ini`; servidor
`unreal-mcp-detonauto` no escopo local do Claude Code do PC). Responde quando o
editor está aberto. Os scripts Python seguem sendo a forma oficial.
