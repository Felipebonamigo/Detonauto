# Regras do dano (parte pura)

Contrato para `Source/Detonauto/Core/DNDamage.h/.cpp` (sem UObject, sem mundo,
só `CoreMinimal`). Decide **quanto** dano um impacto causa e **o que** se solta.
Como isso aparece na tela (amassado no material, malha, peças físicas) é outro
lote, depois de medir (`docs/arquitetura.md`, pergunta 2). Referência de
sensação pedida pelo Felipe: batida com reação visível, estilo Wreckfest.

Todos os números abaixo são **valores iniciais a calibrar**; ficam em
`Data/Damage.csv` (uma linha, id `padrao`, nomes de coluna iguais aos campos de
`FDNDamageConfig`), com validador e teste de deriva como os outros CSVs.

## Entrada de um impacto

`FDNImpact`: `DeltaVMs` (variação de velocidade do veículo causada pelo impacto,
m/s, >= 0; quem chama calcula como impulso normal / massa, para o resultado não
depender da massa), `Zone` (`EDNDamageZone`: Front, Rear, Left, Right, Roof,
Under) e `LocalPositionCm` (ponto no espaço do carro, para o amassado). NaN,
infinito ou negativo em `DeltaVMs`: ignorado, sem dano.

## Regras

1. **Limiar:** `DeltaVMs < DentMinDeltaVMs` (2,0) não causa dano algum. `>=`
   causa (inclusivo).
2. **Dano da zona:** `ZoneDamage[Zone] += (DeltaVMs - DentMinDeltaVMs) /
   ZoneDestroyDeltaVMs` com `ZoneDestroyDeltaVMs = 30,0`; saturado em 1,0.
3. **Amassado:** gera um registro `{LocalPositionCm, DepthCm, RadiusCm}` com
   `DepthCm = min(DentMaxDepthCm (25,0), (DeltaVMs - DentMinDeltaVMs) *
   DentDepthPerMs (1,5))` e `RadiusCm = DentBaseRadiusCm (30,0) + DepthCm`.
   Guarda no máximo `MaxDents` (8) por veículo. Amassado a menos de metade do
   raio de um existente **funde** nele: mantém o centro do mais fundo, soma
   profundidade (saturada em `DentMaxDepthCm`) e raio é o maior dos dois. Com o
   limite cheio e sem fusão possível, substitui o **mais raso** (empate: o mais
   antigo).
4. **Peças:** cada peça (`EDNDamagePart`: Hood, Trunk, DoorLeft, DoorRight,
   BumperFront, BumperRear, GlassFront) pertence a uma zona (tabela fixa no
   código: Hood/BumperFront/GlassFront=Front, Trunk/BumperRear=Rear,
   DoorLeft=Left, DoorRight=Right). Uma peça **solta** (uma única vez) quando,
   após o impacto, vale uma de: `DeltaVMs >= PartDetachDeltaVMs` (12,0) e a
   peça é da zona atingida; ou `ZoneDamage` da zona `>= PartDetachZoneDamage`
   (0,6). Peça já solta nunca solta de novo. O resultado lista as peças soltas
   **por este impacto**, em ordem fixa do enum.
5. **Veículo destruído:** `TotalDamage = média de ZoneDamage das 5 zonas
   (Front, Rear, Left, Right, Roof)` (Under não conta); destruído quando
   `TotalDamage >= DestroyedTotalDamage` (0,5) **ou** qualquer das 5 zonas
   (Front, Rear, Left, Right, Roof) chegar a 1,0; bater de lado ou no teto até
   a carcaça também destrói (decisão confirmada; a Under continua fora).
   Destruído é permanente e o primeiro impacto que o causa devolve
   `bJustDestroyed = true` (uma vez).
6. Depois de destruído continua acumulando dano e amassados (para a carcaça),
   mas nunca devolve `bJustDestroyed` de novo.

## Pool de peças soltas (limite de corpos físicos)

`FDNPartPool::Plan(MaxActive, Active, Requested)`: quantas peças novas podem
virar corpo físico agora, e quantas das mais antigas devem ser **recicladas**.
`MaxActive` (padrão 24, em `Damage.csv`): se `Active + Requested > MaxActive`,
recicla `Active + Requested - MaxActive` (as mais antigas), nunca mais do que
`Active`, e se `Requested > MaxActive` aceita só `MaxActive`. Entradas negativas
valem 0.

## Saída

`FDNDamageResult`: `AddedZoneDamage` (float), `DentAdded` (bool) e o amassado
(se houve), `DetachedParts` (lista), `bJustDestroyed`.

## Dados e testes

- `Data/Damage.csv` com os 10 números acima (nomes em inglês), validador:
  tudo finito; limites `> 0`; `PartDetachZoneDamage` e `DestroyedTotalDamage`
  em (0,1]; `MaxDents` e `MaxActive` inteiros `>= 1`.
- Bordas: 1,99 / 2,0 / 2,01 m/s; saturação em 1,0; fusão a exatamente metade
  do raio (conta como fusão, inclusivo); dano em zona Under (não entra no
  total); NaN; peça solta duas vezes; destruído uma vez; pool com
  `Requested` maior que `MaxActive`.
- Vermelho por mutação como nos lotes anteriores. Se algo estiver ambíguo ou
  contraditório, pergunte antes de codar.

## Fora de escopo

Como o amassado é desenhado, malhas, material, peças como atores, partículas,
fogo, explosão. Isso depende de protótipo medido (Sprints 12 e 13).

## Decisões de implementação (lote 9)

O contrato acima não cobria; ficaram assim em `Core/DNDamage.h` e nos testes (`Core/Tests/DNDamage.spec.cpp`):

- **D1.** No limiar exato (`DeltaVMs == DentMinDeltaVMs`) o impacto causa dano de valor 0 e um amassado de profundidade 0
  (o contrato diz "gera um registro" e "2,0 causa"). Esse amassado ocupa uma vaga e pode ser substituído por ser o mais raso.
- **D2.** `AddedZoneDamage` é o aumento real da zona, depois da saturação em 1,0 (não o valor bruto).
- **D3.** A fusão compara a distância com a metade do raio do amassado **existente** (inclusivo, como pedem as bordas; o texto
  da regra 3 diz "menos de metade", e as bordas dizem que exatamente a metade funde: valeu a das bordas). Havendo vários ao
  alcance, funde no **mais próximo**, empate no mais antigo. Depois de fundir não se procura outra fusão.
- **D4.** `LocalPositionCm` com NaN ou infinito: o amassado é ignorado e o dano segue.
- **D5.** Zona fora do enum: o impacto é ignorado.
- **D6.** As peças são avaliadas em todas as zonas: a regra do dano acumulado vale para as peças de qualquer zona.
- **D7.** Configuração inválida não quebra: divisores `<= 0` viram um mínimo minúsculo, profundidades e raios ficam em `>= 0`
  e as contagens em pelo menos 1. Quem recusa é o validador do CSV.
- **D8.** `FDNDamageResult::Dent` é o amassado novo como calculado, antes de uma eventual fusão.
- **D9.** Com o limite cheio e sem fusão, o novo substitui o mais raso mesmo que seja mais raso que ele (o contrato é literal).
- **D10.** `Damage.csv` tem 10 números (`DentMinDeltaVMs`, `ZoneDestroyDeltaVMs`, `DentMaxDepthCm`, `DentDepthPerMs`,
  `DentBaseRadiusCm`, `MaxDents`, `PartDetachDeltaVMs`, `PartDetachZoneDamage`, `DestroyedTotalDamage`, `MaxActive`). O
  texto antigo dizia 14; foi erro de contagem do contrato, corrigido. A fração 0,5 da fusão, o 1,0 da zona e as 5 zonas
  do total ficam fixos no código (`DNDamage.cpp`), comentados.
- **D11.** Validação do CSV além do contrato: nenhuma relação entre campos (por exemplo `PartDetachDeltaVMs >= DentMinDeltaVMs`).

**Regra 5 (destruído), decisão confirmada:** destruído quando o total das 5 zonas chega a 0,5 ou qualquer das zonas Front,
Rear, Left, Right e Roof chega a 1,0. A Under sozinha não destrói. A versão anterior só destruía por Front ou Rear e deixava
uma zona lateral saturada sobreviver (total 0,2); foi corrigida (D12 em `Core/DNDamage.h`).

## Referência de calibração (lote 11)

Medida no protótipo do amassado (`docs/prototipo-amassado.md`), carro batendo de frente numa parede, com a variação de velocidade
medida no corpo (velocidade antes menos depois, em m/s). **Nenhum número de `Data/Damage.csv` foi alterado por causa disto**:
`ZoneDestroyDeltaVMs` e os demais ficam como estão até o Felipe sentir o amassado e o peso do carro. Para mudar a partir de que
velocidade o carro é destruído, basta trocar `ZoneDestroyDeltaVMs` (hoje 30).

| Velocidade ao bater (km/h) | ΔV (m/s) | Amassado (cm) | Destruído |
|---|---|---|---|
| 42 | 15,1 | 19,6 | não |
| 60 | 21,1 | 25,0 | não |
| 80 | 28,0 | 25,0 | não (dano da zona +0,87) |
| ~120 | 41,7 a 43,5 | 25,0 | sim |

- O ΔV é de **1,25 a 1,3 vezes a velocidade de aproximação** (o carro volta da parede com ~28% da velocidade; o quique faz parte da
  "variação de velocidade do veículo"). Dispersão de ~10% entre execuções no mesmo cenário.
- Com os valores atuais (zona satura em 2 + 30 = 32 m/s), uma batida **frontal na parede** destrói a partir de ~90 km/h de
  aproximação. Contra outro carro em movimento ou em ângulo o número será outro; não foi medido.
- Os números são de um carro (`arcade-1`, 1200 kg, e `Padrao`, 1500 kg) e de uma parede; servem de referência, não de especificação.
