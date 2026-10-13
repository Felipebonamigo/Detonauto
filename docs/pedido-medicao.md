# Pedido de medição de desempenho (para a sessão local)

Pedido do Felipe, feito pela sessão de nuvem em 13 de outubro de 2026. Leia
também `docs/desempenho.md` e `docs/notas-sprint-2.md`, que têm a primeira
medição dele, lida de uma captura do `stat unit`.

Primeira medição, no editor (Play in Editor), mapa `VehicleBasic`, 1 carro, 47
atores: Frame 10,04 ms, Game 8,68 ms, GPU 8,86 ms, Draw 0,00 ms (suspeito),
memória de vídeo 5,62 de 10,96 GB, RenderRes 54,3% (1661x996), Draws 456, Prims
122 mil. É medição de editor e de cena simples: só serve de ponto de partida.

## Objetivo

Medir de verdade, fora do editor, com carga crescente, e responder: o Game thread
e a GPU cabem em 16,6 ms por quadro com 40 carros?

## O que fazer

1. **Hardware, somente leitura.** Identifique placa de vídeo (nome e VRAM),
   processador (nome e núcleos) e RAM com `Get-CimInstance`. Informe ao Felipe.
   Não altere nada do sistema.
2. **RenderRes 54,3%.** Descubra por que a resolução de render está abaixo da
   nativa. Leia, sem alterar, as configurações do projeto e os cvars em uso
   (`r.ScreenPercentage`, `r.DynamicRes`, upscaler, TSR, qualidade em
   `DefaultEngine.ini` e `DefaultScalability`). Diga a causa, ou que não
   conseguiu confirmar.
3. **Script de medição.** Crie `Tools/Unreal/measure.ps1` (vai para o Git). Ele
   roda o jogo em modo Standalone (`-game`, janela, resolução fixa e declarada,
   sem editor), espera 30 s de aquecimento, mede por 60 s e grava o tempo por
   quadro de Frame, Game, Draw e GPU. Escolha o método e confirme que funciona
   nesta versão, por exemplo CSV Profiler ou `stat startfile`. Calcule média, p95
   e máximo por cenário e escreva um relatório em texto. **`-nullrhi` não serve
   para GPU e Draw:** a medição precisa de render.
4. **Cenários** (carro do template e, se couber, o `DNTestCar`):
   a. 1 carro.
   b. 1 carro do jogador mais 20 carros iguais parados (corpos físicos, sem IA).
   c. 1 mais 40 parados.
   d. 1 mais 40 em movimento simples, se for viável sem IA complexa.
   Deixe **explícito** que esses carros são substitutos para estimar custo, não
   o tráfego nem o dano reais. Fixe resolução, qualidade e a semente de qualquer
   aleatoriedade. Se a escala de resolução estiver em 54%, meça também uma
   rodada em 100% e diga quanto a GPU sobe.
5. **Resultado.** Preencha a tabela de `docs/desempenho.md` (uma linha por
   cenário, com máquina, resolução, Frame, Game, Draw, GPU média e p95) e escreva
   uma leitura curta e honesta: o que cabe nos 16,6 ms, qual parte vira gargalo
   conforme sobe o número de carros e o que **não** foi medido. Commit pequeno,
   mensagem em português, e confira o push com `git log origin/main -3`.

## Avisos ao Felipe, antes de rodar

Durante a medição abrirão janelas do jogo por alguns minutos e o PC ficará
ocupado. Ele não deve usar o computador para outras coisas pesadas. Peça que ele
**feche o editor do Unreal** antes: o editor trava arquivos e distorce a medição.

## Limites

- Português do Brasil.
- Só `Sandbox\VeiculoTeste` para o projeto. `Tools/` pode ir para o Git.
- Nada de `Sandbox/` no Git.
- Nenhum commandlet que grave pacotes.
- Nenhuma alteração de configuração do Windows, do driver ou da engine.
- Se precisar gravar fora de `Sandbox`, pergunte antes.
- Mostre a saída dos comandos de git, sem esconder.

## Ao terminar

Resumo curto para o Felipe com a tabela, a causa do RenderRes, o hardware
identificado e as limitações da medição.
