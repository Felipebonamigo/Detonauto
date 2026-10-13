# Protótipo medido do amassado visual (lote 10)

Pergunta: dá para amassar a carroceria como o Felipe pediu (estilo Wreckfest) dentro do orçamento de 60 fps? Técnica testada
primeiro: **deslocamento de vértices no material (World Position Offset)**, comandado por até 8 amassados por carro, que
vêm das regras do dano (`docs/regras-dano.md`, regra 3). Isto é **protótipo**, só no `Sandbox/VeiculoTeste` (fora do Git);
os arquivos de referência estão em [`Tools/Unreal/veiculo-teste/`](../Tools/Unreal/veiculo-teste/README.md). Nada aqui é
código final. **Não foi avaliado se ficou bonito**: a seção "O que as capturas mostram" só descreve o que se vê.

## Resumo

- **Funcionou:** o material desloca os vértices, os amassados aparecem na carroceria e respondem à batida simulada e à batida real
  contra a parede. As regras do `Core/DNDamage` (fusão, limite de 8, profundidade até 25 cm) alimentam o material sem alteração.
- **Custo medido** (1080p, RTX 4070 Ti, 12 carros parados, cada um com 8 amassados de profundidade máxima): **+0,25 ms de GPU**
  (5,42 para 5,67 ms, +4,6%) e **+0,48 ms no Game thread** (2,85 para 3,33 ms) contra os mesmos carros sem o material de
  amassado. Com 1 carro: +0,15 ms de GPU. O quadro ficou em ~8,2 a 8,5 ms em todos os casos (a diferença entre cenários é
  menor que o ruído, ~0,2 ms, estimado pela dispersão entre os cenários).
- **Não funcionou / risco:** a normal não é recalculada (o amassado é sombreado como superfície lisa), vidros, faróis e rodas são
  malhas separadas e **não amassam**, o interior da carroceria aparece escuro onde o vértice entra demais, e as bordas ficam
  facetadas onde a malha é esparsa. Detalhes e propostas abaixo.

## Como funciona

1. **Material-pai** `M_DNDentParent`, criado por script (texto, versionado): `Tools/Unreal/create_dent_material.py`, com o
   invólucro `Tools/Unreal/create_dent_material.ps1` (`UnrealEditor-Cmd -run=PythonScript`, só grava em `/Game/`, sempre recria).
   Tem um nó Custom (HLSL) com um laço de 8 amassados. Para cada vértice perto de um amassado (distância menor que o raio):
   `deslocamento = -normal_do_vértice x profundidade x suavização(1 - distância/raio)`, somado entre os amassados. A mesma
   máscara escurece a cor (parâmetro `DentDarkening`), para a marca de batida aparecer mesmo sem normal nova.
2. **Parâmetros** no espaço local do componente, em cm: `D<i>P` (posição, vetor), `D<i>R` (raio, escalar; raio 0 desliga o
   amassado) e `D<i>D` (profundidade, escalar), de `i = 0` a `7`. O raio e a profundidade não vão num vetor só porque um
   parâmetro vetorial entrega ao nó Custom apenas RGB (a 1ª versão errou aqui e o shader não compilou: `expected 32 elements,
   have 24`).
3. **No carro** (`ADNTestCar`): cria um material dinâmico por carro a partir do pai e o aplica nos dois slots da malha da carroceria
   (`SM_SportsCar`). A cada impacto, `FDNDamage::ApplyImpact` (cópia **idêntica** de `Core/DNDamage.*`, em
   `Sandbox/VeiculoTeste/Source/VeiculoTeste/Core/`) atualiza a lista de amassados e `FDNDentParams` (parte pura, com testes)
   a converte nos 24 parâmetros. Impacto real: `OnComponentHit` do corpo físico; a variação de velocidade é medida no próprio
   corpo (ver "Calibração da batida real", lote 11).
4. **Nanite:** a malha da carroceria é Nanite (`Nanite=1` no log). Sem a flag `used_with_nanite` a engine troca o material pelo padrão
   ("missing usage flag Nanite"); o script liga a flag e tenta definir o deslocamento máximo do WPO (40 cm).

## Como rodar

Feche o editor e compile (`Tools/Unreal/build.ps1 -Uproject Sandbox\VeiculoTeste\VeiculoTeste.uproject`). No jogo
(`DNTestCarGameMode`, mapa `VehicleBasic`), o amassado vem **ligado por padrão**:

| Tecla | Efeito |
|---|---|
| **T, Y, U** | Batida simulada na **frente** do carro: pequena (5 m/s, 4,5 cm), média (10 m/s, 12 cm), máxima (20 m/s, 25 cm) |
| **G, H, J** | O mesmo no **lado esquerdo** |
| **V** | Tira todos os amassados |
| (bater de verdade) | A batida real contra a parede ou outro carro também amassa; o HUD mostra a contagem e a maior batida |

Linha de comando: `-DNNoDents` desliga o amassado (medição de referência), `-DNDentMaterial` liga só o material, sem amassados
nem batida real, `-DNDents=8` liga o material e aplica 8 amassados de demonstração.

Scripts: `car-probe.ps1 ... -Scenario Wall` (reto a fundo contra a parede, loga as batidas reais) e `... -ShotDent` (as capturas
abaixo); `measure.ps1 -Scenario X-1,XM-1,XD-1,X-6,XM-6,XD-6,X-12,XM-12,XD-12` (a medição).

## A malha

Registrado no log ao ligar o amassado: `SM_SportsCar` tem **9.770 vértices e 11.172 triângulos**, 1 LOD, 2 materiais, Nanite
ligado; caixa local de ~470 x 228 x 123 cm. Não medi a densidade por região, só a contagem total. Um amassado de raio 30 a 55 cm
só é suave se houver vértices suficientes dentro do raio; nas capturas as bordas dos amassados aparecem **facetadas** e, onde a malha
tem muito detalhe (para-choque, grade, faróis), irregulares e pontiagudas. **Proposta:** contar os vértices por painel e, se os
painéis grandes (capô, teto, laterais) forem esparsos, subdividi-los (no Blender, `Tools/Blender/`) ou usar a tesselação/deslocamento
do Nanite; não testei nenhum dos dois.

## O que as capturas mostram

Capturas só da cena (sem HUD), carro parado, 8 amassados de profundidade máxima nos pontos: frente, traseira, lados esquerdo e
direito, teto e três quinas. Em [`docs/img/prototipo-amassado/`](img/prototipo-amassado/):

- **Vista da frente e lado esquerdo, antes** (`dn-dent-A-antes.png`): carroceria lisa, pintura laranja.
- **Vista da frente e lado esquerdo, depois** (`dn-dent-A-depois.png`): o capô e a lateral dianteira esquerda aparecem
  enrugados e com depressões; a região do para-choque e dos faróis está bem mais irregular, com pedaços pontiagudos. Os **faróis
  (malha separada) parecem ficar no lugar enquanto a carroceria ao redor deforma**.
- **Vista de trás e lado direito, depois** (`dn-dent-B-depois.png`): a tampa do porta-malas e o para-choque traseiro aparecem
  amassados e rasgados, com áreas escuras onde a face de dentro da carroceria fica visível; a porta direita tem depressões
  rasas.
- **Vista de trás e lado direito, antes** (`dn-dent-B-antes.png`): a mesma vista sem amassados, para comparar.
- **Batida real contra a parede** (`dn-dent-parede.png`, 640x360): um amassado no centro do para-choque dianteiro; nessa vista
  o estrago é pequeno e pouco visível.

Não há iluminação própria do amassado: as sombras suaves dentro da depressão vêm da cor escurecida e da normal original, não
de uma normal nova. Isso faz a depressão parecer mais "achatada" e de borda dura do que um amassado real.

## Custo medido

`Tools/Unreal/measure.ps1`, janela 1920x1080 (Standalone, sem editor), aquecimento 30 s e medição 60 s por cenário, VSync
desligado, `t.MaxFPS 0`, semente 12345, RTX 4070 Ti, Ryzen 7 5800X3D. Carros DN (`ADNTestCar`) parados, o jogador também parado
(`-DNStill`) para os extras ficarem na vista. Médias; p95 no relatório (`Saved/Measure/20261013-191755/`).

| Carros | Material | Frame (ms) | Game (ms) | Draw (ms) | GPU (ms) |
|---|---|---|---|---|---|
| 1 | referência (materiais originais) | 8,49 | 1,62 | 8,48 | 5,22 |
| 1 | amassado, sem amassados | 8,38 | 1,65 | 8,37 | 5,23 |
| 1 | amassado, **8 amassados** | 8,55 | 1,61 | 8,54 | 5,37 |
| 6 | referência | 8,41 | 2,19 | 8,35 | 5,33 |
| 6 | amassado, sem amassados | 8,42 | 2,48 | 8,41 | 5,42 |
| 6 | amassado, **8 amassados** | 8,25 | 2,46 | 8,23 | 5,45 |
| 12 | referência | 8,20 | 2,85 | 8,19 | 5,42 |
| 12 | amassado, sem amassados | 8,23 | 3,25 | 8,22 | 5,59 |
| 12 | amassado, **8 amassados** | 8,21 | 3,33 | 8,20 | 5,67 |

Leitura:

- **GPU:** o material de amassado custa +0,01 a +0,17 ms mesmo sem amassados (o laço roda para todo vértice) e os 8 amassados
  somam +0,03 a +0,14 ms. Pior caso medido sobre a referência: 12 carros, +0,25 ms (4,6%); com 1 carro, +0,15 ms (2,9%).
- **Game thread:** +0,03 a +0,40 ms sem amassados; é o custo de um material dinâmico por carro (cerca de 0,03 a 0,05 ms por carro), não
  dos amassados em si.
- **Frame:** sem diferença que se destaque do ruído. O tempo de quadro estava limitado por outra coisa (Draw ~ Frame, GPU 5,4 ms).
- **Contra o orçamento** (`docs/arquitetura.md`: 8 ms Game, 14 ms GPU): o pior caso é ~6% do Game e ~2% da GPU. É barato.

**O que esta medição não prova:** (a) é um PC bem mais forte que o alvo "nível PS5"; (b) os carros estavam **parados**: sombras em
cache não foram refeitas e o custo de redesenhar sombra com WPO em carros em movimento não foi medido; (c) 12 carros é o dado
pedido, mas a cena de Crash terá tráfego com muito mais veículos, que não terão 8 amassados cada ao mesmo tempo; (d) não investiguei
as opções do Nanite para WPO (a medição é no estado padrão); (e) são médias de uma execução por cenário.

## Calibração da batida real (lote 11)

Três mudanças, só no protótipo (`Sandbox/VeiculoTeste`, sem tocar no `Core`):

1. **Variação de velocidade medida no corpo.** O carro guarda a velocidade linear do corpo no início de cada quadro (antes do passo de
   física). O contato chega depois do passo, com a velocidade já alterada; `ΔV = |velocidade antes - velocidade agora|` em m/s
   (`FDNDentParams::DeltaVMsBetween`). Substitui `impulso / massa / 100`. Inclui o quique, como a definição do contrato
   ("variação de velocidade do veículo causada pelo impacto").
2. **Uma batida = uma janela de 0,15 s.** O primeiro contato abre a janela; durante ela só vale o **maior** ΔV (sem somar) e o ponto
   dele; ao fechar, o amassado é aplicado uma vez (`FDNImpactGrouper`). O log também mostra, só como informação, o ΔV da janela inteira
   (velocidade antes do 1º contato contra a de quando a janela fecha).
3. **Ponto do amassado no corpo.** O ponto de contato local sobe até 20 cm acima da base da caixa do corpo e entra na caixa
   (`FDNDentParams::ProjectToBody`): o contato com a parede caía a 1,9 cm do chão (Z local) e o amassado ia para o chão do
   para-choque; agora vai para Z local 17,6 (a base da caixa fica em -2,4), na frente (X 247,5, o máximo da caixa). A caixa é só uma
   aproximação da superfície do corpo.

**Medição na parede** (`car-probe.ps1 -Scenario Wall -WallKmh N`, carro segurando a velocidade, parede de ~50 m à frente; velocidade
real de aproximação lida do corpo; 1 execução por linha, salvo onde indicado):

| Preset | Velocidade ao bater (km/h) | ΔV da batida (m/s) | ΔV da janela inteira (m/s) | Amassado (profundidade) | Destruído |
|---|---|---|---|---|---|
| `arcade-1` | 42,4 e 42,3 (2 execuções) | 15,1 e 15,0 | 14,7 e 14,6 | 19,6 e 19,5 cm | não |
| `Padrao` | 41,6 | 13,2 | 13,1 | 16,9 cm | não |
| `arcade-1` | 59,5 | 21,1 | 21,0 | 25,0 cm (teto) | não |
| `Padrao` | 57,1 | 18,2 | 18,0 | 24,3 cm | não |
| `arcade-1` | 79,9 | 28,0 | 27,9 | 25,0 cm | não (dano da zona +0,87) |
| `arcade-1` | 119,3 a 120,0 (2 execuções) | 41,7 e 43,5 | 41,6 e 43,0 | 25,0 cm | **sim** |
| `Padrao` | 113,4 e 113,6 (2 execuções) | 37,1 e 41,1 | 36,9 e 41,0 | 25,0 cm | **sim** |

Leitura:

- **A 120 km/h o ΔV é ~42 m/s, mais que os ~30 m/s do contrato**, não menos. O carro destrói no primeiro toque.
- O ΔV vale **1,25 a 1,3 vezes a velocidade de aproximação** (120 km/h = 33,3 m/s dá 43,5; 80 km/h = 22,2 m/s dá 28,0; 60 km/h dá
  21,1; 42 km/h dá 15,1). Ou seja, o carro volta da parede com ~28% da velocidade (quique). Pelas regras do dano (zona satura em 32 m/s,
  ou seja, 2 + 30), **a partir de ~90 km/h de aproximação uma batida frontal destrói o carro** (32 / 1,28 = 25 m/s).
- **Cada batida frontal foi um contato só, num quadro só**, por isso o ΔV do quadro e o da janela inteira quase não diferem (a
  janela inteira é 0,2 a 3% menor). O risco de o agrupamento por maior valor subestimar uma batida que se espalhe por vários
  quadros não apareceu na parede; apareceria numa batida em ângulo ou arrastando.
- Os contatos seguintes (carro raspando e voltando) saem com 0,1 a 0,6 m/s e abaixo do limiar de 2 m/s: não amassam.
- A "variação de velocidade" depende do quique da física do carro (`Padrao` 113 km/h deu 37,1 e 41,1 em duas execuções: a
  dispersão é de ~10% entre execuções). Não ajustei nenhuma regra: os números ficam como medidos.

## Riscos

1. **Normais e iluminação.** O WPO move o vértice e não a normal. O amassado ganha forma mas é iluminado como se fosse liso; só a
   máscara de cor sugere profundidade. Uma normal recalculada (derivada do campo de deslocamento, ou um normal map dinâmico)
   é o próximo passo natural e não foi tentado.
2. **Vidros, faróis e rodas.** São malhas separadas (`SM_SportsCar_Glass`, `SM_SportsCar_Wheel`, faróis dentro da carroceria) sem o
   material: não amassam. O vidro fica flutuando sobre um capô amassado e o farol aparece solto (visível na captura). Teria que
   receber o mesmo material (o vidro é translúcido, outro tipo de material) ou ser solto/quebrado pelas regras de peças.
3. **Peças separadas.** Quando uma porta ou o capô se soltarem (regra 4 do dano) a peça precisa virar outro objeto: o amassado dela
   tem que ir junto (cada peça terá seu espaço local e seus amassados) ou ser "assado" na forma da peça no momento da soltura.
   Hoje a carroceria é uma malha só, então isso não aparece no protótipo.
4. **Face de dentro.** Quando o vértice entra mais que a espessura da carroceria, aparece a face de trás (escura) e o interior vazio.
   Com profundidade máxima de 25 cm isso acontece em painéis finos, como na traseira nas capturas.
5. **Malha esparsa.** Ver "A malha": o resultado depende da densidade de vértices; painéis planos amassam em facetas.
6. **Calibração do impacto.** Resolvida no lote 11 (seção abaixo): a variação de velocidade passou a ser medida no corpo, antes e
   depois do contato, em vez de `impulso / massa / 100` (que deu 45,1 m/s a ~120 km/h). Fica o que a medição mostra: o quique entra
   na conta e a variação passa de 1,25 vezes a velocidade de aproximação.
7. **Estado só na memória.** Os amassados não são salvos nem restaurados ao reaparecer o carro; a lista vive no `FDNDamageState`.
8. **Custo por carro.** Um material dinâmico por carro e 24 parâmetros reenviados a cada impacto. Se o tráfego tiver centenas de
   carros, o ideal é amassar só os carros perto da câmera e usar o material original nos outros.

## O que funcionou e o que não

| | Resultado |
|---|---|
| Criar o material por script Python do editor, versionado | **Funcionou**, depois de corrigir os parâmetros (vetor RGB) e a flag Nanite |
| Deslocar vértices por amassados vindos das regras do `Core` | **Funcionou** (capturas) |
| Custo em 60 fps | **Barato** no que foi medido (+0,25 ms de GPU no pior caso), com as limitações acima |
| Aparência | **Não avaliada.** As capturas mostram deformação visível, de bordas facetadas e sem normal própria |
| Vidros, faróis, rodas | **Não amassam** |
| Peças soltas e deformação por física | Fora deste lote, como combinado |

## Como repetir

```powershell
.\Tools\Unreal\create_dent_material.ps1 -Uproject Sandbox\VeiculoTeste\VeiculoTeste.uproject
.\Tools\Unreal\build.ps1 -Uproject Sandbox\VeiculoTeste\VeiculoTeste.uproject
.\Tools\Unreal\car-probe.ps1 -Uproject Sandbox\VeiculoTeste\VeiculoTeste.uproject -Row arcade-1 -ShotDent
.\Tools\Unreal\car-probe.ps1 -Uproject Sandbox\VeiculoTeste\VeiculoTeste.uproject -Row arcade-1 -Scenario Wall
.\Tools\Unreal\measure.ps1 -Uproject Sandbox\VeiculoTeste\VeiculoTeste.uproject -Scenario X-1,XM-1,XD-1,X-6,XM-6,XD-6,X-12,XM-12,XD-12
```
