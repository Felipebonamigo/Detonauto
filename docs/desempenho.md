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

## Lote 18: desfoque nas bordas (15 out 2026)

O passe do material `M_DNEdgeBlur` medido sozinho, por `profilegpu` (tempo de GPU de cada passe), com a força máxima do CSV (0,5)
fixada parado (`-DNEdgeBlurForce=0.5`), perfil `DN_AlvoPS5`, 1920x1080, 12 amostras de 16 a 27 s:

| Passe | Mediana | Pior das 12 |
|---|---|---|
| `PostProcessMaterial 1920x1080 Material=M_DNEdgeBlur` | 0,038 ms | 0,041 ms |

**Leitura:** 12 amostras bilineares por pixel fora do raio interno; cabe com folga. Abaixo de 100 km/h o peso do material é 0 e o passe
nem roda. O TDFende estava aberto: o quadro inteiro oscilou de 4,9 a 14,6 ms entre as amostras, mas o passe ficou entre 0,035 e
0,041 ms (a disputa pela GPU só pode aumentar o tempo do passe, então o número é um teto). **Pendente:** quadro inteiro com e sem o
desfoque (média e p95 pelo `lookdev-measure.ps1`, `-ExtraArgs '-DNEdgeBlurForce=0.5'` contra `'-DNNoEdgeBlur'`) com o PC livre. Das
cinco tentativas de 15 out, quatro foram descartadas por outro Unreal aberto (as três com o desfoque entre elas); a que valeu, sem o
desfoque, deu Frame 4,74 / 5,29 ms e GPU 4,45 / 4,94 ms (média / p95).

**Quadro inteiro, medido depois com o PC livre** (15 out, nenhum outro Unreal aberto, já com as peças do 18b, parado, duas execuções
alternadas):

| Cenário | Frame média / p95 | Game | GPU média / p95 | Quadros |
|---|---|---|---|---|
| desfoque na força máxima (`-DNEdgeBlurForce=0.5`) (1) | 4,82 / 5,33 ms | 1,64 / 2,36 ms | 4,54 / 5,03 ms | 2073 em 10 s |
| sem desfoque (`-DNNoEdgeBlur`) (1) | 4,81 / 5,29 ms | 1,63 / 2,35 ms | 4,52 / 5,01 ms | 2080 em 10 s |
| desfoque na força máxima (2) | 4,85 / 5,35 ms | 1,62 / 2,35 ms | 4,56 / 5,05 ms | 2063 em 10 s |
| sem desfoque (2) | 4,81 / 5,32 ms | 1,65 / 2,39 ms | 4,53 / 5,01 ms | 2079 em 10 s |

**Leitura:** +0,01 a +0,04 ms de Frame e +0,02 a +0,03 ms de GPU, do tamanho do passe medido sozinho (0,038 ms). Fechado.

## Lote 18b: peças que soltam e vidros (15 out 2026)

Mesmo protocolo (`lookdev-measure.ps1 -Scenario parado`, perfil `DN_AlvoPS5`, 1920x1080), **nenhum outro Unreal aberto**. Cada carro
tem 19 malhas a mais, todas com esqueleto: 11 peças e 8 vidros e lentes, cada uma com os 5 alvos. A carroceria foi subdividida antes do
corte. Os estacionados são `ADNVehicle` (`-DNParkedCars=6`) e também ganham peças.

| Cenário | Frame média / p95 | Game média / p95 | GPU média / p95 | Física, linha do jogo | Física, trabalhadores | Quadros |
|---|---|---|---|---|---|---|
| só o jogador, sem peças (`-DNNoParts`) | 4,56 / 5,05 ms | 1,39 / 2,03 ms | 4,29 / 4,77 ms | 0,10 / 0,11 ms | 0,12 / 0,15 ms | 2193 em 10 s |
| só o jogador, com peças | 4,81 / 5,30 ms | 1,59 / 2,32 ms | 4,53 / 5,01 ms | 0,10 / 0,11 ms | 0,12 / 0,15 ms | 2080 em 10 s |
| + 6 intactos, sem peças (1) | 4,76 / 5,25 ms | 1,95 / 2,73 ms | 4,48 / 4,97 ms | 0,31 / 0,37 ms | 0,19 / 0,23 ms | 2100 em 10 s |
| + 6 intactos, sem peças (2) | 4,76 / 5,24 ms | 1,94 / 2,70 ms | 4,49 / 4,97 ms | 0,31 / 0,36 ms | 0,18 / 0,22 ms | 2100 em 10 s |
| + 6 com as 5 zonas em 1,0, sem peças (1) | 4,77 / 5,28 ms | 1,96 / 2,72 ms | 4,49 / 4,98 ms | – | – | 2099 em 10 s |
| + 6 com as 5 zonas em 1,0, sem peças (2) | 4,78 / 5,26 ms | 1,99 / 2,76 ms | 4,50 / 4,99 ms | – | – | 2093 em 10 s |
| + 6 intactos, com peças presas (1) | 5,54 / 6,20 ms | 3,00 / 3,85 ms | 5,11 / 5,63 ms | 0,36 / 0,83 ms | 0,19 / 0,25 ms | 1806 em 10 s |
| + 6 intactos, com peças presas (2) | 5,55 / 6,24 ms | 3,05 / 3,89 ms | 5,11 / 5,62 ms | 0,38 / 0,94 ms | 0,19 / 0,24 ms | 1802 em 10 s |
| + 6 com 36 peças penduradas (dano 0,45) | 5,63 / 6,35 ms | 3,46 / 4,21 ms | 5,18 / 5,68 ms | 0,39 / 0,75 ms | 0,31 / 0,58 ms | 1776 em 10 s |
| + 6 em 1,0, 66 soltas já pousadas (24 dormindo, o resto congelado pelo orçamento) | 5,41 / 6,00 ms | 3,01 / 3,79 ms | 5,09 / 5,59 ms | 0,36 / 0,80 ms | 0,24 / 0,39 ms | 1848 em 10 s |
| + 6 intactos, com peças, janela de 15 a 19 s | 5,53 / 6,20 ms | 3,02 / 3,81 ms | 5,12 / 5,64 ms | 0,37 / 0,83 ms | 0,20 / 0,29 ms | 724 em 4 s |
| + 6 quebrados em 16 s (`-DNParkedDamageAtS=16`), 24 voando, mesma janela (1) | 5,51 / 6,14 ms | 3,48 / 5,11 ms | 5,13 / 5,61 ms | 0,35 / 0,61 ms | 0,32 / 0,61 ms | 726 em 4 s |
| idem (2) | 5,54 / 6,19 ms | 3,52 / 5,02 ms | 5,17 / 5,66 ms | 0,34 / 0,54 ms | 0,35 / 0,63 ms | 722 em 4 s |

Na janela de 15 a 19 s, o dano entra a 1 s do começo e as 24 peças que o orçamento deixa simular voam até ~4,2 s (log).

**Leitura:**

- **Peças presas, 7 carros:** +0,79 ms de Frame, +0,63 de GPU e +1,1 de Game, ou seja, +0,11 / +0,09 / +0,16 ms por carro. As duas
  execuções de cada lado ficaram a 0,05 ms uma da outra. O Game sobe porque cada carro tem 27 componentes de malha com esqueleto presos
  ao chassi (19 novos e os 8 de antes), e cada um faz tick e atualiza a posição a cada quadro. É a alavanca se o tráfego ganhar peças:
  tick desligado com a peça presa, ou a pose do chassi como líder (`SetLeaderPoseComponent`).
- **Alvos de deformação com a carroceria densa** (fecha a pendência do 17b): 6 em 1,0 contra 6 intactos, sem peças, +0,01 a +0,02 ms
  de Frame e de GPU. Nada.
- **36 penduradas:** +0,46 ms de Game e +0,12 ms na física dos trabalhadores (36 restrições), contra os intactos com peças.
- **24 voando ao mesmo tempo:** +0,5 ms de Game na média e +1,2 no p95. O p95 inclui o quadro em que 66 peças soltam de uma vez
  (6 carros × 5 zonas por `DNZone`). A física dos trabalhadores sobe 0,15 ms e a GPU não muda.
- Tudo longe do orçamento (Game 8 ms; GPU ~5 ms na 4070 Ti, ~10 ms estimados no PS5). O p95 da física na linha do jogo subiu com
  as peças presas (0,37 → 0,83–0,94 ms) sem a média mudar: picos, não custo fixo. Medir de novo quando o tráfego ganhar peças.

## Lote 13: rodada de Crash e custo do tráfego (15 out 2026)

`Tools/Unreal/crash-measure.ps1` com os 8 cenários padrão, PC livre (nenhum outro Unreal aberto), 1920x1080, semente 12345. É a
estrada de teste do `ADNCrashTestGameMode` sobre `/Engine/Maps/Entry`, sem a cena de aparência, e o tráfego ainda são as caixas
provisórias. Relatório completo em `Saved/Measure/20261015-022749/relatorio.txt`.

| Cenário | Pontos | Destruídos | Medalha | Duração da rodada | Acordaram |
|---|---|---|---|---|---|
| C-100-20 (Rail) | 58 | 0 | nenhuma | 5,36 s | 2 de 20 |
| C-100-40 (Rail) | 1320 | 0 | bronze | 7,53 s | 5 de 40 |
| C-130-20 (Rail) | 54 | 0 | nenhuma | 11,44 s | 2 de 20 |
| C-130-40 (Rail) | 2186 | 0 | bronze | 29,66 s | 38 de 40 |
| C-130-40 (Physics) | 11162 | 0 | prata | 18,63 s | 0 de 40 (já nascem físicos) |

| Cenário | Frame média / p95 | Game média / p95 | GPU média / p95 |
|---|---|---|---|
| P-0, sem tráfego | 3,66 / 3,98 ms | 1,63 / 2,34 ms | 3,40 / 3,45 ms |
| P-40, Rail | 3,83 / 4,22 ms | 2,26 / 2,93 ms | 3,55 / 3,64 ms |
| P-40, Physics | 3,81 / 4,21 ms | 2,47 / 3,09 ms | 3,51 / 3,61 ms |

**Leitura:**

- **Custo:** 0,016 ms de Game por carro em trilho e 0,021 ms em física, com 40 carros parados ao lado da estrada. Cabe com folga; o
  `Rail` não é gargalo nessa densidade, então o Mass (proposta 3) continua sem motivo.
- **Rodada:** ninguém é destruído em nenhum cenário, e a pontuação por dano (13c) é o que dá pontos. A 130 km/h com 40 carros em trilho,
  38 acordam e a rodada dura 29,7 s, com 10 962 eventos de impacto. Com todos físicos desde o início ela rende 5 vezes mais (11 162,
  prata) em 18,6 s. Os números da rodada são para a calibração de medalhas e de dano do Crash, não para o desempenho.
- Medir de novo quando o tráfego usar o carro de verdade (lote 20), com peças (ver a nota dos 27 componentes no 18b).

## Para o Felipe testar na Sprint 6

1. Abra o carro do template, jogue com Alt+P e digite `stat unit`.
2. Anote os quatro números parado e depois em velocidade.
3. Repita com o carro novo da Sprint 4.
4. Preencha a primeira linha da tabela acima e me mande.

## Lote 19: efeitos de batida, 6 carros (16 out 2026)

`lookdev-measure.ps1 -Scenario parado -Warmup 14 -Measure 6` (janela de 14 a 20 s), perfil `DN_AlvoPS5`, 1920x1080, o jogador mais 6
estacionados com peças (`-DNParkedCars=6`). O dano entra a 14,5 s (`-DNParkedDamageAtS=14.5`, as 5 zonas em 1,0): cada carro solta
peças e vidros (cacos), a frente passa a soltar fumaça escura e fogo. É o pior caso de efeitos do contrato, sem a câmera de batida.

| Cenário | Frame média / p95 | Game média / p95 | GPU média / p95 | Física, linha do jogo | Quadros |
|---|---|---|---|---|---|
| 6 intactos (`-DNParkedDamage=0`) | 5,74 / 6,39 ms | 3,42 / 4,12 ms | 5,13 / 5,69 ms | 0,37 / 0,52 ms | 1046 em 6 s |
| 6 destruídos aos 14,5 s, com os 9 sistemas (1) | 6,18 / 6,39 ms | 4,10 / 4,50 ms | 5,22 / 5,74 ms | 0,36 / 0,74 ms | 971 em 6 s |
| idem (2) | 6,22 / 6,44 ms | 4,04 / 4,62 ms | 5,23 / 5,73 ms | 0,34 / 0,61 ms | 965 em 6 s |

**Leitura:** contra os intactos, os destruídos com efeitos custam +0,44 a +0,48 ms de Frame, +0,09 a +0,10 ms de GPU (p95 +0,04 a +0,05 ms)
e +0,6 ms de Game. O contrato pede no máximo +1,5 ms de GPU no p95: está bem dentro. As duas execuções ficaram a 0,04 ms uma da outra.

**Niagara isolado (16 out, com o Age of Earth aberto, autorizado pelo Felipe):** mesmo cenário, 4 pares alternados com e sem
`Content/Detonauto/Effects`. O outro Unreal ficou aberto o tempo todo, então o script marca todas as rodadas como descartadas e o
Frame (7 a 15 ms) varia muito; duas rodadas "sem" deram GPU de 11 a 14 ms (placa dividida) e ficaram de fora. A GPU das demais: com
efeitos 5,72 / 5,81 / 5,54 / 5,38 ms (média 5,61); sem efeitos 5,58 / 5,68 ms (média 5,63). **Leitura: o custo de GPU do Niagara
nesse cenário é indistinguível do ruído (cerca de ±0,15 ms).** Combina com o +0,09 a +0,10 ms da medição limpa acima, que junta dano e
efeitos, mas não prova um número menor. Evidência fraca; repetir com o PC livre se o custo vier a importar.

**Limites desta medição:**
- O número junta o dano (peças voando, vidros, restrições) com os efeitos. Para isolar o Niagara, tirei `Content/Detonauto/Effects` do
  caminho e repeti, mas as 4 rodadas foram descartadas pelo próprio script: outro projeto (AgeOfEarth) abriu um Unreal no meio. Os
  números dessas rodadas não valem e ficaram de fora. Uma segunda tentativa, alternando com e sem os assets, também saiu distorcida
  (Frame de 8 a 14 ms, outro Unreal abrindo e fechando). Repetir com o PC livre, e de preferência por uma chave no jogo em vez de
  mover a pasta de assets.
- Não entram a fumaça de pneu do jogador, a câmera de batida dentro da fumaça (item 7 do contrato) nem faíscas (só disparam em batida
  real, não pelo `DNZone`). A GPU de uma batida real com faíscas e a câmera dentro da fumaça segue sem medida.

## Marco 1 da fatia jogável: a corrida no percurso 1 (17 out 2026)

`Tools/Unreal/race-measure.ps1` (piloto automático, perfil `DN_AlvoPS5`, 1920x1080, VSync desligado, `t.MaxFPS 0`, aquecimento de 10 s e medição de 30 s: o carro corre
retas e curvas; nenhum outro Unreal aberto). Rua de prédios do City Sample (asfalto, calçada, meio-fio, fachadas de 4 a 10 andares, céu e névoa), HUD de Canvas e, no
segundo cenário, 3 rivais. Média / p95 em ms:

| Cenário | Frame | Game | Draw | GPU |
|---|---|---|---|---|
| sem rivais (1 carro) | 7,94 / 13,00 | 2,27 / 3,45 | 7,93 / 12,98 | 7,34 / 12,14 |
| com 3 rivais (4 carros) | 7,24 / 12,14 | 3,09 / 4,12 | 7,23 / 12,14 | 6,82 / 11,90 |

(Uma primeira rodada de 20 s só com o jogador, ainda compilando shaders, deu 7,22 / 8,80. A diferença entre os dois cenários no Frame é ruído de rota: o p95 depende de
quanto tempo o carro passa com o sol de frente e olhando as retas longas.)

- **Cabe nos 16,6 ms**, com folga na média (7 a 8 ms) e no p95 (12 a 13 ms). É **GPU-bound** (Draw e GPU acompanham o Frame); o Game thread fica em 2 a 3 ms.
- **Cada rival custa ~0,27 ms de Game** (3,09 contra 2,27 ms em média): o carro com física Chaos, as 11 peças e a regra do piloto.
- **Ressalva de sempre:** esta placa (RTX 4070 Ti) é bem mais forte que a de um PS5; os números são otimistas para "o nível do PS5". A margem de ~2x na média ainda deixaria
  o p95 por volta do limite. Medir de novo quando houver um PC de referência mais fraco, ou reduzir o custo da cena antes de empilhar mais prédios.
- **Não medido:** memória de vídeo e o *streaming* das texturas (as de City Sample vieram em 8K, ~1,5 GB no disco). Reduzir para 2k continua na fila (regra do lote 20); depende de um
  passo de reimportação no editor que ainda não existe.

### Remedição depois do visual da corrida (17 out 2026, rodadas 1 a 3)

Mesma ferramenta e perfil (`race-measure.ps1`, `DN_AlvoPS5`, 1080p, 10 s de aquecimento e 30 s de medição, nenhum outro Unreal aberto), agora com fachadas em 6 cores, calçada e meio-fio do City Sample, postes, jacarandás e bancos destrutíveis, faixas no asfalto, nuvens volumétricas, pós-processamento e o HUD com barra segmentada. Média / p95 em ms:

| Cenário | Frame | Game | Draw | GPU |
|---|---|---|---|---|
| sem rivais (1 carro) | 7,16 / 7,86 | 1,75 / 1,97 | 7,15 / 7,85 | 6,78 / 7,29 |
| com 3 rivais (4 carros) | 7,40 / 8,06 | 2,60 / 2,88 | 7,39 / 8,04 | 7,01 / 7,53 |

Continua **GPU-bound e sob os 16,6 ms** com folga (p95 de 8 ms). Os p95 da medição anterior (12 a 13 ms) vinham de ruído: havia outros Unreal abertos em parte das rodadas; esta rodada ficou limpa. Os rivais custam ~0,85 ms de Game no conjunto (0,28 ms cada). As nuvens volumétricas e as 5 variantes de material das fachadas não pesaram visivelmente. A ressalva de sempre: a RTX 4070 Ti é mais forte que o alvo; VRAM e streaming das texturas (8K do City Sample) seguem sem medir.
