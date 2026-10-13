# Notas de teste da Sprint 2

Registro do que o Felipe viu ao jogar o template de veículo no projeto de teste
`VeiculoTeste`. Campos em branco ainda não foram respondidos. Nada aqui foi
medido por um programa: são as impressões e os números que o Felipe informar.

## O que já está confirmado

| Data | O que foi testado | Resultado |
|---|---|---|
| 13 out 2026 | Abrir o `VeiculoTeste.uproject`, apertar Alt+P com o carro do template | O carro aparece e anda. O Felipe achou o visual bonito |

| 13 out 2026 | Dirigir o carro do template | O Felipe conseguiu andar e dar ré |

Isso confirma que o conserto da cópia manual do template funcionou: os
redirecionamentos do nome antigo do módulo e as pastas de conteúdo no lugar certo.

## Sensação de dirigir (a preencher)

1. A direção é mais pesada ou mais leve que a de um Burnout?
2. A curva responde rápido demais, devagar demais ou na medida?
3. Dá vontade de derrapar? A derrapada é fácil de controlar?
4. A câmera ajuda a sentir velocidade, ou parece parada?
5. Bater num muro tem peso, ou o carro parece de plástico?
6. O que parece mais lento ou mais pesado do que o Burnout?

## Desempenho (a preencher)

Comando: `stat unit`, com o jogo rodando e o carro em velocidade.

| Máquina | Cenário | Frame | Game | Draw | GPU | Memória de vídeo |
|---|---|---|---|---|---|---|
| PC do Felipe (modelo da placa de vídeo e do processador a informar) | Editor, jogo em andamento (Play in Editor), mapa `VehicleBasic`, 1 carro, 47 atores | 10,04 ms | 8,68 ms | 0,00 ms | 8,86 ms | 5,62 GB de 10,96 GB |

Lido de uma captura de tela do `stat unit` em 13 out 2026. Outras linhas da
mesma tela, sem análise: RHIT 0,32 ms, Draws 456, Prims 122 mil, RenderRes 54,3%
(1661x996), Mem 4,26 GB. A linha "Input" mostrava 433,86 ms e não é confiável
para esta análise. Veja a leitura em `docs/desempenho.md`.

## Outros testes visuais pendentes

| Teste | Como fazer | Resultado |
|---|---|---|
| Cubo da Sprint 3 | Mapa `VehicleBasic`, World Settings, GameMode Override `DNTestGameMode`, Alt+P. W e S andam, A e D giram, Espaço soma 10 | |
| Carro novo da Sprint 4 | GameMode Override `DNTestCarGameMode`, Alt+P. Depois edite uma linha de `Data\TestCars.csv`, salve e jogue de novo | |
