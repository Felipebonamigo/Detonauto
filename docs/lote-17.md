# Lote 17: dano visual, camada 1 (deformação grande por zona)

Contrato: [`regras-dano-visual.md`](regras-dano-visual.md). Feito em 14 out 2026 no carro do template, na sessão local.

## O que existe

- **Regra pura** `Core/DNDamageMorph` (teste `Detonauto.Dano.AlvosDeDeformacao`, M1 a M5): o peso do alvo de cada zona sobe até
  `pow(dano da zona, MorphCurveExponent)`, no máximo `MorphRisePerSecond x Dt` por quadro, e nunca desce. Os dois parâmetros estão em
  `Data/Damage.csv` (0,7 e 10/s), com validação ((0, 4] e (0, 100]) e teste de deriva.
- **Carroceria** `SK_DNSportsCarBody` (malha com 1 osso, `SKEL_DNSportsCarBody`), com os alvos `DN_Front`, `DN_Rear`, `DN_Left`,
  `DN_Right` e `DN_Roof`. Os faróis (`SM_SportsCar_Glass`) estão unidos à lataria para andarem juntos com ela.
- **Carro:** `ADNVehicle` usa a carroceria nova (sem ela, volta à malha estática de antes) e aplica os pesos a cada quadro. O amassado
  local do material (lote 10) continua por cima, só nos slots com o material de amassado. `ClearDents` (tecla V) zera os alvos.
- **Para capturas:** `DNZone <zona> <dano>` no console (ou `EXEC:DNZone 0 0.6@t` no `-DNKeys`), zona 0 frente, 1 traseira,
  2 esquerda, 3 direita, 4 teto. Decimal com ponto: `0,6` vira 0.
- **Para medir:** `-DNParkedCars=N` estaciona N carros do jogador à frente, com as 5 zonas em `-DNParkedDamage` (padrão 1).

## Como refazer a carroceria

```powershell
.\Tools\Unreal\damage-body.ps1 -Uproject Detonauto.uproject
```

Com o editor fechado. Três etapas: o Unreal exporta as duas malhas e `wheels.json` (eixos e raio da roda, dos ossos `Phys_Wheel_*`);
o Blender (`Tools/Blender/make_damage_morphs.py`) une, cria os alvos e grava as imagens de `docs/img/dano-zonas/`; o Unreal importa,
põe os mesmos materiais, liga o uso de alvos de deformação nos materiais-base e confere alvos e limites (diferença de até 2 cm).
Mesma semente, mesmo resultado (`-Seed`, padrão 17).

## Como a forma é feita

Campo espacial suave, igual para todas as cascas da malha (163 cascas no carro do template): vértices que coincidem andam juntos e não
abrem fresta. O script confere o determinante do jacobiano do campo numa grade (abaixo de 0 a malha dobraria sobre si e mostraria faces
de dentro) e recusa o alvo se dobrar. Valores medidos na última geração:

| Alvo | Maior deslocamento | Menor determinante |
|---|---|---|
| DN_Front | 42,5 cm | 0,27 |
| DN_Rear | 32,4 cm | 0,35 |
| DN_Left | 24,3 cm | 0,57 |
| DN_Right | 20,5 cm | 0,61 |
| DN_Roof | 13,5 cm | 0,73 |

**Mudança no contrato (frente e traseira).** Com o recuo do contrato (18% e 14% do comprimento na ponta, caindo a zero a 35%) a
lataria passava para trás das rodas: na captura, as rodas da frente ficavam soltas na frente do carro. Agora o amassado forte fica no
balanço: a ponta recua 45% (frente) e 40% (traseira) da distância até o começo do para-lama, medido pelas rodas, e uma cauda leve de
2,5% do comprimento vai até os 35% do contrato. As outras zonas seguem os valores do contrato. Também mudou: a ondulação só tira até
25% da forma (as profundidades são o máximo); com ±25% a frente dobrava sobre si (determinante −0,03, pego pela conferência).

## Provas

- Suíte 635/635. Mutação da regra e dos dados: 18 de 19 pegos (16 de código e 3 de CSV). Sobreviveu "Dt <= 0 não barrado", equivalente
  (o `Max` com o peso anterior já impede descer). A troca dos nomes Left/Right sobrevivia: ganhou teste dos 5 nomes, visto falhar.
- Capturas na cena de aparência, perfil alvo PS5, 1920x1080, roteiro de teclas reais (`-Keys`), nos 4 quartos (135, 225, 315 e 45 graus),
  intacto e com as 5 zonas a 0,3, 0,6 e 1,0: [`lookdev-niveis.jpg`](img/dano-zonas/lookdev-niveis.jpg); de perto em 1,0:
  [`lookdev-1.0-frente-esquerda.jpg`](img/dano-zonas/lookdev-1.0-frente-esquerda.jpg) e
  [`lookdev-1.0-traseira-direita.jpg`](img/dano-zonas/lookdev-1.0-traseira-direita.jpg). Nenhuma fresta nem face de dentro visível.
- Batida real na parede (`vehicle-probe.ps1 -Scenario Wall -WallKmh 50|100 -ResX 1920 -ResY 1080`): 16,1 m/s, frente com dano 0,47
  ([`parede-50kmh.jpg`](img/dano-zonas/parede-50kmh.jpg)); 34,6 m/s, frente em 1,0 e carro destruído
  ([`parede-100kmh.jpg`](img/dano-zonas/parede-100kmh.jpg)). A arena de teste é escura; o visual se julga nas capturas da cena.
- Custo: [`desempenho.md`](desempenho.md), seção do lote 17. 6 carros amassados contra 6 intactos: diferença dentro do ruído.
  Captura dos 6: [`lookdev-6-amassados.jpg`](img/dano-zonas/lookdev-6-amassados.jpg).
- Imagens do Blender (forma pura, sem pintura): `intacto.png` e `DN_*.png` em `docs/img/dano-zonas/`.

## Leitura honesta do visual

- Lê como batida: a frente encurta e enruga, as portas afundam com dobras que a luz mostra, a traseira esmaga, o teto baixa.
- **Limite do carro do template:** a malha é leve (6,5 mil vértices na lataria). As dobras saem macias, sem vinco vivo, e em 1,0 a
  lataria parece um pouco "derretida" de perto. O mesmo script num carro mais denso (City Sample) deve dar vinco mais nítido; refazer lá
  quando o carro for escolhido, como o contrato pede.
- Em 1,0 na frente, a borda do para-lama encosta no pneu. Fica para a camada da roda empurrada (fora deste lote).
- A forma é a mesma em todo carro e toda batida (um alvo por zona). A variação vem do amassado local por cima e, depois, das peças
  soltas e do vidro quebrado (camadas 2 e 3).

## Ficou de fora

- O Felipe olhar e dizer se parece batida de verdade (regra do contrato).
- Carros do tráfego ainda são cubos: não amassam.
- `-game` com o editor binário mostra erros de Python dos plugins `NiagaraToolsets` e `StateTreeToolset` (`module 'unreal' has no
  attribute 'AgentSkill'`), da pasta `Engine/Plugins/Experimental/Toolsets` (a mesma família do `AllToolsets` ligado com o MCP). Não
  atrapalham o jogo, mas sujam o critério "nenhuma linha `Error:`" das rotinas de captura.
