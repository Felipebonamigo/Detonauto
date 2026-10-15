# Lote 18b: peças que soltam e vidros que quebram

Contrato: [`regras-pecas.md`](regras-pecas.md). Feito na sessão local em 15 out 2026. As regras puras e os dados entraram antes
(b2866d9 e 0500234, testes `Detonauto.Dano.PecasDoCarro` e `Detonauto.Dados`); este documento cobre as malhas, o motor e a
aprovação.

## O que existe

- **Malhas, no Blender** (`Tools/Blender/split_car_parts.py`, mapa `Tools/Blender/parts/SportsCar.json`):
  1. os vãos saem da malha grossa, antes de subdividir (feitos da subdividida, o vão em leque cobria a roda);
  2. a carroceria é subdividida e as normais de cada canto vão para o atributo `dn_normal`. Montada pelo bmesh, a peça perde as normais
     personalizadas; o atributo é copiado 1 para 1 e reaplicado. A transferência pela face mais próxima pegava a casca errada e
     manchava o para-brisa;
  3. cada peça sai por caixa e planos do mapa (porta, para-lama, para-choque, capô, tampa) ou por ilha de malha (faróis e lanternas,
     pelo material); a peça é fechada e o buraco dela na carroceria vira vão escuro;
  4. cada peça ganha os mesmos 5 alvos de deformação da carroceria, pelo mesmo campo do `make_damage_morphs.py` e com os picos do carro
     inteiro (presa, ela amassa igual, sem fresta);
  5. sai em FBX e numa imagem de conferência.

  São 19 malhas em `Content/Detonauto/Vehicle/SportsCar/Parts/`: aerofólio, 2 faróis, 2 lanternas, para-brisa, vidro traseiro, 2 vidros
  laterais, capô, tampa, 2 portas, 2 para-choques e 4 para-lamas. Elas somam 35,8 MB. A carroceria sem as peças, com os vãos, foi de
  15,4 para 19,7 MB, por causa da subdivisão antes do corte.
- **Importação** (`Tools/Unreal/damage-body.ps1 -Parts`, modo `import-parts` do `damage_body.py`): as peças entram como
  `SK_DNSportsCar_<Nome>`. O FBX descarta o alvo que não desloca nenhum vértice (peça longe de uma zona), então só a carroceria exige
  os 5.
- **Peça no motor** (`Vehicle/ADNCarPart`):
  - **Presa:** a malha vai presa ao chassi, sem colisão; o carro copia nela os pesos dos alvos.
  - **Pendurada:** uma caixa física (85% da malha) ligada ao chassi por `UPhysicsConstraintComponent`. O giro fica só em volta da
    dobradiça, de um lado só: twist limitado à metade de `HingeMaxDeg`, deslocado de meio ângulo. Capô e tampa têm uma mola no meio da
    faixa e abrem 27 graus; sem ela, a gravidade fechava. A despenetração vai até 100 cm/s e a peça ignora o canal `DNDebris`: antes
    disso, o carro parado era empurrado a 6–7 km/h. Ela só raspa contra `WorldStatic` voltado para cima (`OnPartScrape`).
  - **Solta:** a restrição quebra e a peça sai com a velocidade do ponto do carro, mais `LaunchMs` para fora do carro e para cima
    (P5, abaixo). Usa CCD, para de simular depois de 1 s parada e acorda quando o carro a varre. `EnforceBudget` (o `MaxActive` 24)
    congela as soltas mais antigas.
  - A câmera atravessa a peça (C5, abaixo).
  - Reiniciar a rodada devolve tudo.
- **Vidros** (no `ADNVehicle`):
  - **Trinca:** no material do amassado (`GLASS_CRACK_HLSL` em `create_dent_material.py`). Tem linhas radiais que serpenteiam, pedaços
    de anel em polígono (cordas entre duas radiais), teia densa até 18 cm do centro e o miolo esmagado, com linhas de ~0,8 cm. O alcance
    é de 20 + 140 × trinca cm. O centro é o ponto da batida mais perto da caixa do vidro, recuado 70% para dentro dela
    (`GlassCrackInset`): a borda de baixo do para-brisa fica embaixo do capô.
  - **Estouro:** o vidro some e solta 40 cacos (`ADNGlassShards`: cubos instanciados sem física, 0,9 s, quicando num chão achado por
    traço).
  - **Faróis e lanternas:** a lente do template é translúcida e não tem emissão. "A luz apaga" virou lente fosca (opacidade de 0,03 a
    0,55 com a trinca) e, depois, estouro com cacos.
- **Desgaste:** o primer das lascas foi para 0,35 e o miolo de metal nu cresceu (`create_dent_material.py`).
- **Quem tem peças:** todo `ADNVehicle` (o jogador e os estacionados das medições). O tráfego (`ADNTrafficVehicle`, a caixa) segue com a
  regra 4 do `regras-dano.md`.

## Ferramentas de teste

- `EXEC:DNZone <zona> <dano>` no roteiro `-DNKeys` leva uma zona a um dano sem batida, e as peças e os vidros reagem (zonas: 0 frente,
  1 traseira, 2 esquerda, 3 direita, 4 teto).
- `-DNNoParts`: a carroceria inteira de antes, para comparar.
- `DNShowGlass 0|1` no console esconde os vidros.
- `-DNParkedDamageAtS=S` (com `-DNParkedCars`): o dano dos estacionados entra S segundos depois, com as peças voando dentro da janela
  de medição.
- Log: `DNCarPart: <peça> pendurada (borda ..., até N graus)`, `... pendurada há 1 s: aberta N graus`, `... solta a N m/s`,
  `... solta, 0,25 s depois: andou ...` e `... solta, dormiu: andou ...` (no espaço do carro), `OnPartScrape ...` e
  `DNGlass: <vidro> estourou`.

## Aprovação (os 8 itens do contrato)

Capturas com `-DNNoHud`, 1920x1080, perfil `DN_AlvoPS5`, `L_Lookdev`.

1. **Conferência no Blender:** o carro remontado nos estados 0 e 1,0 e a carroceria sem as peças, de 4 ângulos:
   [`blender-conferencia.jpg`](img/pecas/blender-conferencia.jpg). As imagens soltas ficaram em `Sandbox/pecas-conferencia`.
2. **Parede a 100 km/h com a câmera de batida** (`-DNCruiseKmh=100 -DNWallAheadM=120`): dispara com 34,6 m/s a 102 km/h.
   - Soltam o para-choque dianteiro, o capô e os dois para-lamas da frente, de 7,0 a 9,3 m/s.
   - Os dois faróis estouram (trinca 1,00) e o para-brisa trinca.
   - Aos 0,25 s de jogo as peças estão 60 a 78 cm para fora do carro e 25 a 46 cm acima.
   - Sequência de 0,6 a 2,6 s reais: [`parede-100kmh-sequencia.jpg`](img/pecas/parede-100kmh-sequencia.jpg). O capô sobe sobre a frente
     na câmera lenta e as peças caem dos dois lados na perseguição.
   - O aerofólio não sai, porque a traseira não bateu. Com `DNZone 1 0.7`, sai junto com a tampa e o para-choque traseiro.
   - Trinca de perto: [`trinca-para-brisa.jpg`](img/pecas/trinca-para-brisa.jpg).
3. **Para-choque arrastando a 60 km/h** (`DNZone 0 0.45` andando): pendura, cai 16 cm até o chão e raspa.
   - Log: `OnPartScrape BumperFront` com 1, 45 e 89 contatos a 64, 62 e 57 km/h.
   - Imagem de três quartos e de lado: [`para-choque-raspando-60kmh.jpg`](img/pecas/para-choque-raspando-60kmh.jpg). De lado ele
     quase não aparece; de três quartos, a ponta solta no chão aparece.
4. **Batida lateral** (`DNZone 2 0.65`, depois `DNZone 2 0.85`; ainda não há cenário de parede em T):
   - na primeira, a porta esquerda pendura e abre 64 graus em 1 s, e o vidro lateral estoura com cacos;
   - na segunda, saem a porta e os dois para-lamas esquerdos.
   - [`lateral-porta-e-vidro.jpg`](img/pecas/lateral-porta-e-vidro.jpg).
5. **Capô pendurado a 80 km/h, de lado:** fica firme a 27 graus, sem atravessar o para-brisa:
   [`capo-pendurado-80kmh.jpg`](img/pecas/capo-pendurado-80kmh.jpg).
6. **Nenhum vão transparente:** as 4 zonas em 1,0 (18 peças e vidros fora), parado, 6 ângulos de perto. Nenhum fundo aparece através do
   carro: [`sem-pecas-seis-angulos.jpg`](img/pecas/sem-pecas-seis-angulos.jpg).
7. **Custo:** tabela em [`desempenho.md`](desempenho.md), seção do 18b. Com 7 carros parados, as peças presas custam +0,79 ms de Frame
   (+0,63 de GPU e +1,1 de Game). 36 peças penduradas somam +0,46 ms de Game. As 24 voando ao mesmo tempo somam +0,5 ms de Game na
   média e +1,2 no p95, sem mudar a GPU.
8. **O Felipe joga** e diz se os pedaços saindo "vendem" a batida: pendente.

Suíte: 710 de 710.

## Achados na aprovação

- **Câmera dentro do carro aos 1,65 s:** a esfera da câmera de batida sai do meio do carro e parava na peça pendurada ou voando ao lado,
  a 10% do caminho (`a esfera parou em 'DNCarPart_6' (Box) a 10% do caminho`). Agora a peça ignora o canal da câmera
  (`ADNCarPart::MakePartDebris`, teste C5 em `Detonauto.Mobiliario.Colisao`).
- **Peças sumindo dentro da carroceria (P5):** o empurrão "para longe do obstáculo" jogava as peças da frente para trás, através do carro
  (o destroço ignora o carro): aos 0,25 s elas tinham andado 80 cm para trás e 15 cm para cima, dentro dele. Agora o empurrão vai para
  fora do carro pelo lado da peça, sem a parte que entra no obstáculo; a peça no eixo da batida sai de lado
  (`FDNCarParts::LaunchHeading`, 6 testes).
- **Trinca fraca:** com linhas de 0,5 cm, ela sumia a 3 m e as radiais saíam tracejadas (o desvio mudava a cada anel). O centro também
  caía embaixo do capô. Agora as linhas têm 0,8 cm, a radial serpenteia sem quebrar, os anéis são cordas, há teia no centro e o centro
  fica recuado 70%.

## O que mudou no contrato

- **Cacos sem Niagara** até o lote 19: `ADNGlassShards`, que não depende de asset feito à mão. No 19 vira um sistema Niagara.
- **Farol apagado** virou lente fosca e estouro, porque a lente do template não tem luz.
- **Empurrão da peça solta:** para fora do carro (P5), não mais "na direção da batida".
- **Capô e tampa** abrem 27 graus e ficam (mola no meio da faixa); a porta abre até ~64 dos 70.
- **Vãos:** casca escura feita da malha grossa, sem bloco de motor.
- **Trinca procedural**, sem textura.
- **Retrovisor:** o template não tem (nenhum nas capturas de lado).
- **Roda solta** (opcional): não entrou.

## Limites conhecidos

- As peças só soltam quando a janela de 0,15 s do dano fecha: 0,6 s reais na câmera lenta.
- Na câmera lenta a peça se afasta devagar, uns 80 cm por segundo real com `LaunchMs` 4. Se o Felipe achar pouco, a alavanca é o
  `LaunchMs` em `Data/CarParts.csv`.
- De perto, o vão é escuro e facetado.
- **Custo de Game das peças presas:** +0,16 ms por carro, de 27 componentes de malha com esqueleto (peças e vidros). Antes de pôr peças
  no tráfego (lote 20), medir o tick deles desligado ou a pose do chassi como líder (`SetLeaderPoseComponent`).
- O capô pendurado não balança com o vento: não há força aerodinâmica.
- A lataria toda em 1,0 fica quase só vão e chassi: é o pior caso, não a batida comum.
