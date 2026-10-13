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

## Para o Felipe testar na Sprint 6

1. Abra o carro do template, jogue com Alt+P e digite `stat unit`.
2. Anote os quatro números parado e depois em velocidade.
3. Repita com o carro novo da Sprint 4.
4. Preencha a primeira linha da tabela acima e me mande.
