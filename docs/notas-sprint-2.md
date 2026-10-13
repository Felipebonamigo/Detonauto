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

## Sensação de dirigir (respondido em 13 out 2026)

Carro testado: o do template (`VeiculoTeste`), sem os ajustes do Detonauto. O
Felipe notou que parece a mesma versão do teste anterior.

1. **Mais pesada que a de um Burnout.** O carro parece duro.
2. **Curva:** responde quase perfeitamente.
3. **Derrapada:** o carro não derrapa; parece freio de mão duro, sem derrapagem.
4. **Câmera:** em posição ruim; deveria ficar um pouco mais longe do carro.
5. **Batida:** nenhuma reação, muito duro, o carro não quebra. Referência
   sugerida pelo Felipe: algo parecido com o Wreckfest (deformação visível).
6. **Geral:** o carro parece pesado, o nitro não foi encontrado (o template não
   tem), muito duro. Parece mais simulador do que arcade.

### O que isso muda no plano

| Achado | Leitura | Ação |
|---|---|---|
| Duro, pesado, simulador | Suspensão, massa e aderência do template são de simulação | Lote 8: presets de ajuste arcade em `TestCars.csv` |
| Sem derrapagem | Aderência lateral alta demais, freio de mão sem efeito | Lote 8: aderência traseira e freio de mão ajustáveis |
| Câmera perto | Distância e altura da câmera do template | Lote 8: câmera ajustável por dados |
| Sem nitro | O template não tem; o turbo do Detonauto só existe como regra e componente | Lote 8: tecla de turbo ligada ao componente |
| Carro não quebra | Dano e deformação são o maior risco técnico e de desempenho | Sprint de dano: protótipo medido (deformação por malha, estilo Wreckfest) antes de aprovar |
| Curva boa | Direção do template serve de base | Manter a resposta de curva ao ajustar o resto |

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
