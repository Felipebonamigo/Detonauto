# Desempenho: orçamento e como medir

Guia da Sprint 6. Explica a meta de 60 fps do Detonauto, como ler as medições
básicas do Unreal e como registrar resultados. Escrito de memória a partir do
que é estável no Unreal 5.x e **não conferido contra o editor 5.8**: os nomes
de comandos e de janelas abaixo devem ser confirmados na primeira vez que o
Felipe usar cada um.

## A meta

60 quadros por segundo num PC com placa de vídeo no nível do PS5. Isso dá
**16,6 milissegundos por quadro**. Se um quadro demora mais, o jogo perde fluidez.
Num jogo de corrida, que depende da sensação de velocidade, a queda de fps
é percebida na hora. Por isso o desempenho é requisito, não polimento.

## Por que o orçamento é por parte

O quadro passa por três trabalhos que rodam em paralelo, e o mais lento
define o tempo total:

| Parte | O que faz | Exemplo no Detonauto |
|---|---|---|
| **Game thread** | Lógica do jogo, física, IA, animação | Carros, tráfego, pontuação, dano |
| **Render thread** | Prepara o que a placa de vídeo vai desenhar | Quantidade de objetos visíveis |
| **GPU** | Desenha de fato a imagem | Iluminação, sombras, efeitos, resolução |

O orçamento inicial, a ajustar com medição real, está em
`docs/arquitetura.md`. Os valores de lá são suposição.

## Medição rápida: `stat fps` e `stat unit`

No jogo rodando (Alt+P), abra o console com a tecla `'` (ou `~`, conforme o
teclado) e digite:

- `stat fps` mostra os quadros por segundo e o tempo de cada quadro.
- `stat unit` mostra quatro tempos em milissegundos:

| Linha | O que é | Se for o maior |
|---|---|---|
| **Frame** | Tempo total do quadro | É o que decide o fps |
| **Game** | Game thread | Lógica ou física está pesada |
| **Draw** | Render thread | Muitos objetos ou muitas chamadas de desenho |
| **GPU** | Placa de vídeo | Imagem pesada: efeitos, sombras, resolução |

Regra de leitura: o maior entre Game, Draw e GPU é o gargalo. Reduzir os
outros dois não melhora o fps.

Para desligar, digite o mesmo comando de novo.

## Como medir direito

1. Meça com o jogo **empacotado ou em modo "Standalone"**, não só no editor.
   O editor tem custo extra e mostra números piores que o jogo final.
2. Meça sempre o **pior cenário**: batida com mais peças soltas, 40 de tráfego,
   explosão em cadeia. Média de cena vazia não prova nada.
3. Anote a **máquina** (placa de vídeo, processador, resolução) com cada medição.
   Número sem máquina não serve de comparação.
4. Meça **antes e depois** de cada mudança. Um sistema novo só entra se a
   medição do pior cenário continua dentro do orçamento.

## Unreal Insights (para quando o `stat unit` não bastar)

O Unreal Insights grava um quadro a quadro de cada thread, mostrando que
função gastou o tempo. Serve para achar a causa de um pico. O fluxo geral, a
confirmar na primeira vez:

1. Abrir o Unreal Insights, que vem com a engine.
2. Rodar o jogo com a gravação ligada, por exemplo pelo argumento
   `-trace=cpu,gpu,frame` na linha de comando.
3. Abrir o arquivo gravado no Insights e olhar os quadros mais longos.

Use o Insights só quando houver um pico ou um gargalo que o `stat unit` não
explica.

## Pegadinhas

- **Medir no editor sem querer.** Dá números piores que o jogo final.
- **VSync e limite de fps.** Com o fps limitado a 60, o `stat unit` pode
  mostrar o tempo total travado em 16,6 ms mesmo com sobra. Para medir o custo
  real, desligue o limite. No console, `t.MaxFPS 0` costuma funcionar
  (suposição, confirmar).
- **Primeiros segundos.** Shaders e streaming de assets deixam o início mais lento.
  Meça depois de uns 30 segundos de jogo.
- **Medição sem janela.** A medição que a sessão local fez, com `-nullrhi`,
  não tem render nem GPU. Só vale para o Game thread, e ainda assim não
  representa o jogo real.

## Registro das medições

Cada medição entra em uma tabela neste arquivo, com a data, a máquina, o
cenário e os quatro tempos do `stat unit`. Ainda está vazia:

| Data | Máquina | Cenário | Frame | Game | Draw | GPU | Observação |
|---|---|---|---|---|---|---|---|
| 13 out 2026 | PC do Felipe (modelo a informar), VRAM 10,96 GB | Editor, Play in Editor, `VehicleBasic`, 1 carro, 47 atores | 10,04 ms | 8,68 ms | 0,00 ms | 8,86 ms | Primeira medição. Ver leitura abaixo |

### Leitura da primeira medição (13 out 2026)

**O que os números dizem:**

- O quadro leva cerca de 10 ms, o que equivale a uns 100 quadros por segundo. Está
  abaixo dos 16,6 ms da meta de 60 fps.
- O Game thread (8,68 ms) e a GPU (8,86 ms) estão quase empatados, e os dois
  respondem pelo tempo do quadro. Nenhum deles é um gargalo isolado.

**O que os números não dizem, e por quê:**

- **É medição no editor.** O Play in Editor tem custo extra, então o jogo
  empacotado deve ir melhor. Falta medir em modo Standalone para saber.
- **A cena é simples.** É um carro, sem tráfego, sem dano e sem explosão. O
  Detonauto vai ter 40 carros de tráfego e peças soltas, e o custo vai subir.
- **O Game thread já gasta 8,68 ms com um único carro.** O orçamento inicial
  em `docs/arquitetura.md` previa 8 ms para o Game thread no jogo completo. Se parte
  disso for custo do editor, a folga é maior que parece, mas não dá para
  afirmar sem medir fora do editor.
- **Draw em 0,00 ms** é um valor estranho. Pode ser um efeito da forma como o
  editor reporta o tempo da thread de desenho, e não um custo real nulo.
- **RenderRes 54,3% (1661x996).** A resolução de render aparece abaixo da nativa. Pode
  ser um recurso de escala dinâmica ou de upscale ligado, e então a GPU estaria
  trabalhando com menos pixels do que o monitor mostra. **Suposição:** não
  confirmei a causa.
- **A linha Input (433,86 ms) é implausível** como latência do jogo e foi descartada.

**Próximas medições, em ordem de valor:** o mesmo carro em modo Standalone, depois
com 20 e 40 carros de tráfego, e depois com uma batida em cadeia.

## Medição Standalone com carga crescente (13 out 2026)

Feita pela sessão local com `Tools/Unreal/measure.ps1`. **Leia a seção "O que não foi medido" antes
de tirar conclusões.**

**Máquina:** NVIDIA GeForce RTX 4070 Ti, AMD Ryzen 7 5800X3D (8 núcleos, 16 threads), 32 GB de RAM,
monitor 4K com escala do Windows em 150%. O Windows informa 4 GB de VRAM para a placa, mas esse é o limite
do campo de 32 bits; a captura do Felipe mostrou 10,96 GB disponíveis.

**Condições:** jogo em modo Standalone (`-game`, janela 1920x1080, sem editor), mapa `VehicleBasic`,
qualidade `sg.*` da configuração do projeto (sem alteração), VSync desligado e `t.MaxFPS 0`, áudio
desligado (`-nosound`). Aquecimento de 30 s e medição de 60 s por cenário. Semente 12345 para a posição e a
rotação dos carros extras. O CSV Profiler da engine grava, por quadro: Frame (`FrameTime`), Game
(`GameThreadTime`), Draw (`RenderThreadTime`, a thread de render) e GPU (`GPUTime`). O jogador dirige sozinho
(acelerador e volante em vai-e-vem), então ele bate em paredes e a cena vista muda de uma rodada para outra.

**Os carros extras são substitutos para estimar custo.** São carros iguais ao do jogador, como corpos
físicos do Chaos Vehicles. Não são o tráfego real (sem IA, sem modelos variados) nem o dano real (sem
deformação, peças soltas ou explosão). "Parados" = freio de mão acionado, acordados a cada 0,5 s para a física
contar. "Em movimento" = 70% de acelerador e volante em vai-e-vem com fase aleatória por carro.

Valores em milissegundos, **média / p95** (95% dos quadros foram mais rápidos que o p95). Orçamento para 60 fps:
16,6 ms.

| Cenário | Extras | Máquina / resolução | Frame | Game | Draw | GPU |
|---|---|---|---|---|---|---|
| T-1: carro do template | 0 | 4070 Ti + 5800X3D, 1920x1080 | 6,69 / 7,97 | 1,61 / 2,24 | 6,68 / 7,97 | 5,39 / 5,92 |
| T-21: template, extras parados | 20 | idem | 7,20 / 8,78 | 4,72 / 6,02 | 7,18 / 8,76 | 4,75 / 5,29 |
| T-41: template, extras parados | 40 | idem | 7,66 / 9,86 | 6,99 / 9,55 | 7,05 / 8,82 | 5,70 / 6,27 |
| T-41M: template, extras em movimento | 40 | idem | 9,11 / 11,36 | 9,11 / 11,22 | 5,79 / 6,51 | 5,63 / 6,20 |
| T-41@54: como T-41, escala de render 54% | 40 | idem | 7,40 / 9,22 | 6,51 / 8,95 | 6,98 / 8,57 | 4,94 / 5,51 |
| D-1: carro novo da Sprint 4 (`DNTestCar`) | 0 | idem | 7,13 / 8,93 | 1,56 / 2,01 | 7,12 / 8,92 | 4,92 / 5,49 |
| D-21: `DNTestCar`, extras parados (*) | 20 | idem | 12,22 / 28,88 | 4,64 / 6,38 | 12,20 / 28,79 | 7,77 / 27,00 |
| D-41: `DNTestCar`, extras parados | 40 | idem | 8,64 / 10,45 | 6,66 / 8,44 | 8,60 / 10,42 | 6,37 / 7,80 |
| D-41M: `DNTestCar`, extras em movimento | 40 | idem | 9,01 / 11,09 | 8,25 / 10,12 | 8,90 / 10,89 | 6,09 / 7,55 |

(*) **Valor fora da curva, não reproduzido.** O D-21 foi medido três vezes: 7,11, 12,22 e 8,23 ms de Frame
médio. O 12,22 (o da tabela) parece um soluço isolado do PC, não custo do cenário. Use 7 a 8 ms.

### Leitura

**O que cabe nos 16,6 ms.** Nestas condições, sim: a média de Frame ficou entre 6,7 e 9,1 ms em todos os
cenários, exceto o valor fora da curva (uns 110 a 150 quadros por segundo), e o p95 ficou em 11,4 ms ou menos, exceto no valor fora da curva
acima. Com 40 carros extras em movimento (T-41M e D-41M), o p95 foi de 11,4 e 11,1 ms: sobra uns 5 ms.

**O que sobe com o número de carros é o Game thread.** No template: 1,6 ms com 1 carro, 4,7 com 21, 7,0 com 41
parados e 9,1 com 41 em movimento. Dá uns 0,13 a 0,16 ms por carro parado e 0,19 ms por carro em movimento. No
`DNTestCar` a conta é parecida (1,6, 4,6, 6,7 e 8,3). Com 40 em movimento, o Game thread (9,1 ms) já passa os
8 ms que `docs/arquitetura.md` reservou para ele, e isso ainda é só física de carros, **sem IA de tráfego, sem
dano e sem destroços**. O que sobrar do orçamento desse tempo é o que o resto do jogo tem para gastar.

**Draw e GPU quase não se mexem com os carros.** A GPU ficou entre 4,7 e 6,4 ms em todos os cenários (sem contar
o valor fora da curva). O custo da GPU parece vir da cena e da qualidade (padrão do template), não dos carros.

**Quem é o gargalo.** Com 0 a 20 carros, o maior tempo é o Draw (thread de render), mais que a GPU (6,7 contra
5,4 ms em T-1). Com 40 carros parados, Game e Draw empatam (7,0 contra 7,1). Com 40 em movimento, Game passa a
ser o maior. **Suposição não verificada:** o `RenderThreadTime` do CSV pode incluir espera pela GPU, então
"Draw maior que GPU" não prova que a thread de render esteja ocupada calculando. A causa não foi investigada.

**Escala de render (54% contra 100%).** Com 40 carros parados, a GPU foi de 5,70 ms (100%) para 4,94 ms (54%)
na rodada principal, e de 5,66 para 5,06 ms numa repetição separada: uns 11 a 13% a mais de GPU em 100%. O Frame
praticamente não mudou (7,66 contra 7,40 ms). Conclusão: nesta cena e nesta placa, a GPU não é o limite, então
a resolução de render pouco muda o tempo do quadro.

**O RenderRes 54,3% do editor.** No Standalone, o log confirma viewport 1920x1080 com `r.ScreenPercentage=0.0`
(o padrão do jogo, 100%). A causa **provável** do 54,3% no editor: o padrão do editor para viewports em tempo
real é "baseado na resolução do display" (`r.Editor.Viewport.ScreenPercentageMode.RealTime`), e o monitor do
Felipe é 4K com escala de 150%. O projeto não define escala de render (`DefaultEngine.ini` sem
`r.ScreenPercentage`; `sg.ResolutionQuality=0` significa "usar o padrão do projeto"), e a escala dinâmica está
desligada (`DynamicResolutionFrameTarget=0`). **Não confirmei como o editor chega exatamente em 54,3%.** Vale só
para o editor; o jogo empacotado não usa esse modo.

### Quanto confiar nestes números

- **Ruído entre rodadas iguais.** O T-41 deu 7,44 e 7,66 ms de Frame em duas rodadas. O D-21, três valores de 7,1 a
  12,2 ms. Diferenças menores que uns 1 ms entre cenários **não são confiáveis**. Não dá para dizer que o
  `DNTestCar` é mais leve ou mais pesado que o carro do template.
- **A primeira rodada do lote foi descartada.** Os três primeiros cenários deram 8,5, 9,4 e 10,0 ms (p95 de até
  21,8 ms), contra 6,7, 7,2 e 7,7 ms depois, com o cache de shaders já preenchido. Medir logo depois de compilar
  ou na primeira execução mostra números piores que a realidade.
- **Picos.** O máximo por quadro ficou entre 21 e 38 ms em todos os cenários (54 ms no valor fora da curva), até com 1 carro (Game máx. 20,9 ms em
  T-1). Isso é um ou poucos quadros longos em 60 s, e **não foi investigado** (suposição: carregamento de
  assets ou coleta de lixo). Para um jogo de corrida, vale olhar com o Unreal Insights.

### O que NÃO foi medido

- Tráfego real (IA, modelos variados), dano real, peças soltas, explosão em cadeia, câmera lenta, efeitos de
  partículas e áudio (a medição usou `-nosound`).
- Os cenários do jogo final: o mapa é o `VehicleBasic` do template. Os extras são todos o mesmo modelo, o que é mais
  barato de desenhar que modelos variados.
- Máquinas mais fracas. **Esta máquina é bem mais forte que o alvo** (a RTX 4070 Ti e o Ryzen 7 5800X3D passam o nível
  de um PS5), então os resultados são otimistas para o alvo de 60 fps "no nível do PS5".
- Resolução 4K, memória de vídeo, jogo empacotado (a medição roda o binário de desenvolvimento com conteúdo não
  empacotado), e o Draw de 0,00 ms do editor (no CSV o Draw tem valor real; a leitura de 0,00 no `stat unit` do editor
  é, provavelmente, característica do editor, suposição).
- O jogador dirige com um piloto automático que bate em paredes; a cena vista muda entre rodadas.

**Para repetir:** `Tools/Unreal/measure.ps1` (feche o editor antes). O script depende do GameMode de medição `DNMeasureGameMode`. Uma cópia de referência está em `Tools/Unreal/measure-gamemode/`, com um README que diz de que ela depende e como copiá-la para o projeto. A versão que roda hoje vive no projeto de teste, em `Sandbox/` (fora do Git).

## Lote 15: base de renderização e perfil "alvo PS5" (14 out 2026)

### O que mudou em `Config/DefaultEngine.ini`

Cada nome foi conferido no código da engine 5.8 antes de gravar. Medido no jogo (`-ExecCmds` com o nome da variável imprime o valor e
quem definiu):

| Variável | Antes (padrão do código) | Depois | Para quê |
|---|---|---|---|
| `r.DynamicGlobalIlluminationMethod` | 0, nenhuma | 1, Lumen | luz indireta do sol, do céu e de emissivos |
| `r.ReflectionMethod` | 2, só de tela | 1, Lumen | reflexo do céu, dos prédios e dos carros |
| `r.Shadow.Virtual.Enable` | 0 | 1, sombras virtuais | sombra nítida de perto e de longe |
| `r.GenerateMeshDistanceFields` | 0 | 1 | Lumen por software e sombra do Sky Light |
| `r.AntiAliasingMethod` | 4, TSR | 4, TSR (agora escrito) | anti-aliasing e reconstrução de resolução |
| `r.AllowStaticLighting` | padrão | False | tudo dinâmico; menos variações de shader |
| `r.ClearCoatNormal` | padrão | True | pintura automotiva com verniz (passo 4 do plano visual) |
| `r.SkinCache.CompileShaders` | padrão | True | alvos de deformação do dano por zona (passo 5) |
| `r.DefaultFeature.AutoExposure.ExtendDefaultLuminanceRange` | padrão | True | exposição em EV100 |
| `r.EyeAdaptation.CachedLightingPreExposure` (em `[SystemSettings]`) | 4 | 8 | com 4 o Lumen e a captura do céu cortavam a luz acima de EV 12 (a engine avisava na tela); 8 cobre EV -4 a 16 |

**Conclusão:** até o lote 14 o jogo rodava **sem Lumen, sem sombras virtuais e sem campos de distância** (confirmado no log, "LastSetBy:
Constructor"). Ray tracing por hardware continua desligado (decisão: Lumen por software para o alvo de 60 fps no nível do PS5). Mudar
essas linhas obriga a engine a recompilar todos os shaders uma vez (vários minutos na primeira abertura).

### O perfil "alvo PS5"

`Config/DefaultDeviceProfiles.ini`, perfil `DN_AlvoPS5`: Lumen, sombras, reflexos, efeitos, folhagem e shading em "alto" (2); distância
de visão, texturas, anti-aliasing e pós-processo em "épico" (3); resolução interna 100% numa janela de 1920x1080 (1080p interno).

- Uso: `vehicle-probe.ps1 ... -AlvoPS5` (liga `-DeviceProfile=DN_AlvoPS5`).
- **Armadilha:** o `Saved/Config/WindowsEditor/GameUserSettings.ini` da máquina (configuração do usuário, fora do Git) reaplica os grupos
  `sg.*` por cima do perfil. Por isso o `-AlvoPS5` também manda os mesmos valores como comandos de console, lidos do próprio `.ini` do
  perfil (um lugar só). O arquivo do usuário não é tocado. Conferido no log: `sg.GlobalIlluminationQuality = "2" LastSetBy: Console`.

### Medição da cena de aparência (`Tools/Unreal/lookdev-measure.ps1`)

RTX 4070 Ti + Ryzen 7 5800X3D, janela 1920x1080, perfil `DN_AlvoPS5`, VSync desligado, `t.MaxFPS 0`, CSV Profiler. Cena: `L_Lookdev`
(1327 atores, 112 prédios em blocos, céu, nuvens volumétricas, neblina, Lumen, sombras virtuais) com o carro do jogador e sem tráfego.

| Cenário | Frame média / p95 | Game | Draw | GPU média / p95 | Quadros |
|---|---|---|---|---|---|
| parado, câmera de jogo olhando a rua contra o sol | 5,06 / 6,32 ms | 1,44 / 2,35 ms | 5,05 / 6,31 ms | 4,71 / 6,06 ms | 1977 em 10 s |
| andando, acelerador no fundo (6 a 14 s) | 5,33 / 6,66 ms | 1,55 / 2,23 ms | 5,32 / 6,65 ms | 4,97 / 6,29 ms | 1500 em 8 s |

Um quadro isolado medido com `profilegpu` (parado, 18 s): 5,1 ms de GPU, dos quais o TSR leva uns 0,37 ms.

**Quanto confiar:**

- **Janela sem foco = medição inválida.** Com a janela do jogo fora do primeiro plano, o jogo fica travado em 60 fps mesmo com
  `t.MaxFPS 0`, a placa baixa o clock e o tempo de GPU parece três vezes maior (o mesmo cenário deu 5,1 ms com foco e 16,9 ms travado,
  593 quadros em 10 s). O script descarta sozinho uma medição com ~60 quadros por segundo e média acima de 16 ms.
- Em parte das execuções havia outro Unreal aberto no PC (outro projeto); o script avisa quando há. As duas linhas da tabela foram
  medidas sem ele.
- **A placa é bem mais forte que o alvo.** Estimativa, não medida: uma GPU no nível do PS5 leva perto do dobro do tempo da 4070 Ti, ou
  seja, uns 10 ms de GPU nesta cena. Sobra orçamento para tráfego, dano e efeitos, mas cada um precisa ser medido aqui de novo.
- A cena é de blocos simples. Com prédios do City Sample (malhas densas, materiais com várias camadas) o custo sobe.

## Lote 17: dano visual por zona, 6 carros amassados (14 out 2026)

Mesma máquina e protocolo do lote 15 (`lookdev-measure.ps1`, perfil `DN_AlvoPS5`, 1920x1080, nenhum outro Unreal aberto). A cena já
tem o mobiliário e os prédios do lote 16. Os carros extras são `ADNVehicle` parados à frente e à direita, todos dentro do quadro da
câmera de jogo (`-DNParkedCars=6`; `-DNParkedDamage=0` deixa intactos). Cada um tem a carroceria `SK_DNSportsCarBody` (7007 vértices no
Blender, 5 alvos de deformação, cache de skin ligado).

| Cenário | Frame média / p95 | Game | Draw | GPU média / p95 | Quadros |
|---|---|---|---|---|---|
| parado, só o carro do jogador | 4,70 / 5,32 ms | 1,34 / 2,11 ms | 4,69 / 5,31 ms | 4,35 / 4,81 ms | 2128 em 10 s |
| parado, + 6 carros intactos | 4,82 / 5,31 ms | 1,92 / 2,61 ms | 4,81 / 5,30 ms | 4,52 / 4,99 ms | 2073 em 10 s |
| parado, + 6 carros com as 5 zonas em 1,0 | 4,96 / 5,74 ms | 2,08 / 3,09 ms | 4,95 / 5,73 ms | 4,60 / 5,13 ms | 2014 em 10 s |
| andando, só o carro do jogador | 4,87 / 5,62 ms | 1,59 / 2,54 ms | 4,86 / 5,62 ms | 4,45 / 4,97 ms | 1643 em 8 s |
| andando, + 6 carros intactos | 5,09 / 5,78 ms | 2,32 / 3,38 ms | 5,08 / 5,77 ms | 4,61 / 5,18 ms | 1571 em 8 s |
| andando, + 6 carros com as 5 zonas em 1,0 | 4,91 / 5,45 ms | 2,34 / 3,44 ms | 4,90 / 5,45 ms | 4,51 / 5,01 ms | 1628 em 8 s |

**Leitura:**

- **Alvos de deformação: custo abaixo do que a medição separa.** Amassados contra intactos: +0,14 ms parado e −0,18 ms andando no
  Frame, +0,08 e −0,10 ms na GPU. É ruído (diferença menor que ~1 ms entre rodadas não é confiável). Faz sentido: 7 mil vértices por
  carro passam por um cálculo de skin por quadro.
- **Os carros em si custam no Game:** +0,6 a +0,7 ms com 6 carros a mais (física do Chaos de 6 veículos), longe dos 8 ms do orçamento.
- O quadro inteiro fica perto de 5 ms (GPU 4,5 ms na 4070 Ti; estimativa de uns 9 a 10 ms numa GPU de PS5).
- **Não medido:** carro amassado em movimento rápido perto da câmera, Lumen com muitas carrocerias deformadas na tela, e o tráfego
  (que ainda usa cubos, `Traffic/DNTrafficVehicle`). Quando o tráfego usar o carro de verdade, medir de novo com o `crash-measure.ps1`.

## Lote 16b: polimento da cena (14 out 2026)

Mesmo protocolo. Cena com o poste novo (226 objetos de rua), copa dos jacarandás mais cheia (Nanite preservando área) e vitrines mais
escuras; só o carro do jogador.

| Cenário | Frame média / p95 | Game | Draw | GPU média / p95 | Quadros |
|---|---|---|---|---|---|
| parado | 4,83 / 5,45 ms | 1,33 / 1,77 ms | 4,82 / 5,43 ms | 4,48 / 5,04 ms | 2072 em 10 s |
| andando | 5,37 / 5,91 ms | 1,79 / 2,67 ms | 5,35 / 5,92 ms | 4,64 / 5,26 ms | 1490 em 8 s |

Contra o lote 16 (4,55 e 4,72 ms): +0,3 parado e +0,65 andando, dentro do ruído de ~1 ms. Se a próxima medição confirmar a subida
andando, a suspeita é a copa mais densa (mais cartões de folha mascarados na tela).

## Lote 17c: 16 peças de rua derrubadas (14 out 2026)

Mesmo protocolo, nenhum outro Unreal aberto. O carro anda sozinho a 60 km/h (`-DNCruiseKmh=60`, sem tecla) e a medição vai de 8 a
18 s (`-Scenario parado -Warmup 8 -Measure 10`). Com as peças, a fileira `-DNPropLine` põe 16 peças na faixa (poste, lixeira, banco e
hidrante, 4 vezes), a partir de 40 m e a cada 12 m: o carro derruba as 16 entre 4 e 15 s, e até 16 simulam ao mesmo tempo. O veredito
da fileira deu 16 de 16 nas duas execuções. Física: `Exclusive/GameThread/Physics` e `Exclusive/AllWorkers/Physics` do CSV Profiler.

| Cenário | Frame média / p95 | Game | GPU média / p95 | Física, linha do jogo | Física, trabalhadores | Quadros |
|---|---|---|---|---|---|---|
| rua intacta (1) | 4,83 / 5,40 ms | 1,44 / 1,71 ms | 4,52 / 5,05 ms | 0,14 / 0,19 ms | 0,23 / 0,35 ms | 2069 em 10 s |
| rua intacta (2) | 4,80 / 5,36 ms | 1,44 / 1,70 ms | 4,50 / 5,03 ms | 0,14 / 0,19 ms | 0,23 / 0,34 ms | 2082 em 10 s |
| 16 peças (1) | 4,84 / 5,39 ms | 1,61 / 2,22 ms | 4,53 / 5,07 ms | 0,12 / 0,16 ms | 0,33 / 0,45 ms | 2068 em 10 s |
| 16 peças (2) | 4,82 / 5,39 ms | 1,62 / 2,25 ms | 4,52 / 5,06 ms | 0,12 / 0,16 ms | 0,34 / 0,47 ms | 2073 em 10 s |

**Leitura:** o quadro não muda (+0,02 ms; a GPU limita). A linha do jogo sobe 0,18 ms na média e 0,55 ms no p95 (o tick das peças
soltas, o varrer e a quebra em si), e a física nos trabalhadores sobe 0,11 ms. As duas execuções de cada cenário ficaram a menos de
0,04 ms uma da outra. Longe dos 8 ms de orçamento da linha do jogo.

## Para o Felipe testar na Sprint 6

1. Abra o carro do template, jogue com Alt+P e digite `stat unit`.
2. Anote os quatro números parado e depois em velocidade.
3. Repita com o carro novo da Sprint 4.
4. Preencha a primeira linha da tabela acima e me mande.
