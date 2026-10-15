# Lote 19: efeitos de batida (Niagara)

Contrato: [`regras-efeitos.md`](regras-efeitos.md). Em andamento desde 15 out 2026. Feito na sessão local: as notas da revisão do
18b (trinca e vão), as regras puras, o CSV, a ligação no C++ e os 9 sistemas Niagara (seção "Montagem pelo MCP", no fim). **Falta o custo**
com 6 carros e o Felipe jogar.

## O que existe

- **Notas da revisão do 18b:**
  - para-brisa recortado na grade (dois planos em V no `SportsCar.json`);
  - trinca fina na rugosidade e na normal;
  - vão com o `MI_DNVao`, metal escuro com ruído pela posição e normais suaves.
  - Capturas: [`trinca-lote19-antes-depois.jpg`](img/pecas/trinca-lote19-antes-depois.jpg) e [`vao-lote19.jpg`](img/pecas/vao-lote19.jpg).
- **Regras** (`Core/DNEffects.h`, testes `Detonauto.Efeitos`, 17):
  - quando e quanto de cada efeito (E2 a E8);
  - pool por efeito (E9);
  - partículas vivas contra o teto (E10);
  - escala da fumaça contínua (E11).
  - Mutação: 20 de 20 pegos (um derrubou o teste).
- **Dados:** `Data/Effects.csv` e `Data/EffectsSettings.csv`, com validador, teste de deriva e o subsistema (`Detonauto.Dados.Efeitos`).
- **Disparos curtos** (`Effects/UDNEffectsSubsystem`, um por mundo): pool, teto de GPU, parâmetros do CSV e log `DNFx`.
  - **impacto:** no contato do carro (`ADNVehicle::OnCarHit`), uma rajada a cada 0,15 s; faísca com Δv ≥ 4 e lascas com Δv ≥ 12;
  - **raspão:** pelo `OnPartScrape` da peça pendurada, uma rajada a cada 0,08 s com as faíscas do intervalo;
  - **cacos:** quando um vidro estoura, pela área da caixa dele. Sem o `NS_DNGlassShards`, o carro usa os cacos antigos (`ADNGlassShards`);
  - **mobiliário:** pelo `UDNBreakableSubsystem::OnBroken`; pó na base do poste e da árvore, folhas na copa da árvore.
- **Efeitos contínuos** (`Vehicle/UDNCarEffectsComponent`, um por carro):
  - fumaça em cada pneu, pelo deslizamento da roda do Chaos (radianos para graus) e pela roda patinando;
  - fumaça do motor (clara e depois escura) e fogo, pelo dano da frente, só subindo;
  - luz pontual do fogo, sem sombra e tremulante, que já funciona sem o sistema;
  - vaga por efeito (até `PoolMax` carros) e escala do teto de sprites.
- **Reinício** (tecla R): apaga disparos, fumaça, fogo e luz, e zera pools e teto.

Verificado no motor, pelo log, com os sistemas ainda ausentes (`[sem asset]`):
- **Muro a 100 km/h:**
  - `SparkImpact 120` e `PlasticChips 30` no contato;
  - pó do poste (31), pó e folhas da árvore (24 e 32);
  - o motor indo direto a fogo (frente 1,0);
  - a R apagando tudo.
- **Para-choque raspando a 60 km/h:** `SparkScrape 26` na primeira rajada, depois 10 a 13.
- **Freio de mão a 133 km/h:** a fumaça de pneu liga e só desliga a 10 km/h.

## Contrato entre o C++ e os sistemas Niagara

Cada efeito é um sistema em `/Game/Detonauto/Effects/NS_DN<Efeito>`, com o nome de `EDNEffect`:

| Sistema | Tipo | Parâmetros de usuário que o C++ preenche |
|---|---|---|
| `NS_DNSparkImpact` | rajada | `Count` (int), `LifeMin`, `LifeMax`, `SpeedMin`, `SpeedMax` (cm/s), `Direction` (eixo do cone: a normal da superfície), `Velocity` (cm/s do carro) |
| `NS_DNSparkScrape` | rajada | os mesmos; `Direction` aponta para trás do carro, rente ao chão |
| `NS_DNPlasticChips` | rajada | os mesmos |
| `NS_DNGlassShards` | rajada | os mesmos, mais `Extent` (cm: meia caixa de onde nascem os cubos) e `Extra` (brilhos: 20) |
| `NS_DNPropDust` | rajada | `Count`, vida, velocidade, `Extent` (a caixa da peça) |
| `NS_DNPropLeaves` | rajada | os mesmos |
| `NS_DNTireSmoke` | contínuo | `Rate` (0 a 1, já com a escala do teto), `SpawnPerS` (sprites/s na taxa 1), vida, velocidade, `FadeNear` e `FadeFar` (cm) |
| `NS_DNEngineSmoke` | contínuo | os mesmos, mais `Dark` (0 clara, 1 escura) |
| `NS_DNEngineFire` | contínuo | `Rate`, `SpawnPerS`, vida, velocidade |

- Rajada: o C++ cria o sistema no ponto, preenche os parâmetros e ativa; o sistema emite `Count` de uma vez e termina sozinho.
- Contínuo: o C++ mantém o componente e muda `Rate` a cada quadro; com `Rate` 0 nada nasce, e o C++ desativa abaixo de 0,02.
- Os números vêm do CSV: ajustar é mudar o CSV, não o asset. O que não está no CSV (cor, textura, tamanho do sprite, curva de
  opacidade) fica no asset e vai para o passo a passo abaixo.
- Tudo roda com a escala de tempo do mundo (câmera lenta junto).

## Passo a passo para a sessão nova (Niagara pelo MCP)

1. Ler `docs/estado-atual.md`, `docs/regras-efeitos.md` e este arquivo.
2. Conferir que nenhum jogo do Detonauto está aberto (o Felipe pode estar jogando o playtest 2) e abrir o editor do Detonauto.
3. Conferir o servidor: `claude mcp list` mostra `unreal-mcp-detonauto` conectado (porta 8010). A sessão precisa ter começado com o
   editor já aberto.
4. Partir dos modelos da engine: `DirectionalBurst` ou `DirectionalBurstLightweight` para faísca e lascas, `UpwardMeshBurst` para
   cacos, `Fountain` e `HangingParticulates` para fumaça e pó. Criar em `/Game/Detonauto/Effects` os nove sistemas da tabela.
   Expor os parâmetros de usuário com os nomes exatos e ligá-los aos módulos: spawn burst = `Count`, spawn rate =
   `Rate x SpawnPerS`, vida, velocidade, cone e herança de velocidade.
5. Faísca: sprite esticado pela velocidade (alinhado ao movimento), emissivo laranja-branco, gravidade, colisão barata com a
   profundidade da cena, em GPU.
6. Fumaça: poucos sprites grandes, crescendo, iluminação barata, sem reflexo de Lumen na transparência, sumindo entre `FadeFar` e
   `FadeNear` da câmera. `Dark` escurece a cor do motor.
7. Texturas: as do Starter Content ou do Content Examples (licença só para Unreal), migradas só o que for usado, com linha em
   `docs/licencas.csv`. Se não estiverem no PC, perguntar ao Felipe antes de baixar.
8. Anotar aqui, por sistema, o que foi feito pelo MCP (módulos, valores que não vêm do CSV). O que der por Python vira script.
9. Capturas da aprovação (o contrato, itens 1 a 6), custo com 6 carros (item 7) e commit.

## Para o Felipe julgar

- Na largada com o pé no fundo, a roda patina e solta fumaça de ~20 a ~77 km/h. Se for demais: a taxa da roda patinando (`Extra` da
  `fumaca-pneu`, 0,6) ou a velocidade mínima (`MinSpeedKmh`, 20) no CSV.

## Montagem pelo MCP (15 out 2026, sessão local)

Esta sessão não carregou o servidor MCP (ele só existe com o editor aberto no início). Contorno: cliente HTTP direto em
`http://127.0.0.1:8010/mcp` (`Tools/Unreal/niagara-mcp/`: `mcp.sh` abre a sessão, `c.py` chama `call_tool`, `ns.py` e `fam.py` têm os
auxiliares do Niagara). O C++ e o CSV não mudaram. Tudo em `Content/Detonauto/Effects/`.

**Estado: os 9 sistemas existem, compilam sem erro, carregam no jogo e aparecem nas capturas. Falta o custo e o julgamento do Felipe.**

| Sistema | Visto na captura | Observação |
|---|---|---|
| `NS_DNSparkImpact` | sim (muro a 100 km/h) | riscos laranja no focinho e no chão |
| `NS_DNSparkScrape` | só pelo log (`SparkScrape 16/19/7`) | falta uma captura lateral boa do para-choque |
| `NS_DNPlasticChips` | não distingui | pequenos e escuros; ver "Para o Felipe julgar" |
| `NS_DNGlassShards` | sim (pontos brancos) | brilhos (`Extra`) ainda não ligados |
| `NS_DNPropDust` | pouco (poste, rente ao chão) | fraco; aumentar se o Felipe quiser |
| `NS_DNPropLeaves` | pouco (árvore) | folhas 12 a 20 cm, conferir |
| `NS_DNTireSmoke` | sim (rastros atrás das rodas traseiras) | |
| `NS_DNEngineSmoke` | sim (0,6 clara; 0,8 rala, depois escurecida sem nova captura) | rever a escura |
| `NS_DNEngineFire` | sim (0,95, bolas de fogo subindo) | parece bola, não chama; rever |

Capturas (sol por trás, 1920, sem HUD, exceto onde dito): `docs/img/efeitos-lote19/`
(`muro-100kmh-faiscas-cacos`, `motor-06-08-095`, `fumaca-pneu`, `poste-quebrando`, `arvore-quebrando`, `faiscas-no-prop`).
Lascas e cacos da primeira ao muro estão em quadros pequenos da folha `muro-100kmh-faiscas-cacos`.

**O que não está feito:**
- **Custo** com 6 carros no `DN_AlvoPS5` (item 7 do contrato). O `lookdev-measure.ps1` recusa com outro Unreal aberto e o Felipe estava
  com o projeto Zombie aberto. Rodar quando o PC estiver livre: `-DNParkedCars=6` com `-DNParkedDamage=1` e `-DNParkedDamageAtS`.
  Os efeitos ainda não têm número.
- Captura do **raspão** do para-choque (só o log) e das **lascas** isoladas.
- Texturas do Starter Content / Content Examples: não estão no PC. Usei a `T_soft_smoke` da própria engine (cópia
  `T_DNSoftSmoke`, linha em `licencas.csv`). Perguntar ao Felipe se quer baixar algo melhor.
- `Velocity` (herança de velocidade do carro) e `Extra` (brilhos do vidro) declarados e sem módulo ligado.
- O aviso do contrato "faísca com colisão barata pela profundidade da cena" não foi feito (sem colisão).

**Achados que o próximo montador precisa saber:**
- **Exposição física de dia:** cor de sprite com valores perto de 1 sai preta. Faíscas e fogo usam dezenas de milhares
  (`60000,20000,3000`), fumaça cinza clara 16000 a 23000. Mudou a aparência de "nada" para "visível".
- **Espaço do cone:** o C++ gira o sistema para a direção e o cone aplicava a rotação de novo. `Cone Axis Coordinate Space = World`.
- **Escala do sprite:** o módulo `ScaleSpriteSize` do modelo nasce em tamanho 0 e cresce; tirei dos contínuos (a fumaça virava
  pontinhos). Tamanho fixo: pneu 50 a 110 cm, fumaça do motor 30 a 60, fogo 12 a 24.
- **Material `M_DNSoftParticle`:** translucente, sem luz, cor da partícula, opacidade = canal R da textura de fumaça x máscara radial
  x alfa da partícula, com `DepthFade`. Sem a máscara radial os sprites saem como quadrados.
- O módulo `ScaleAttributesByCameraDistance` (fade perto da câmera) vem com `Min Scale = 1`; ficou em 0.
- Fumaça de pneu: no `stat niagara` aparecia com partículas e sem sprite até corrigir o tamanho (item acima).
- `GetSystemCompileState` trava o MCP depois de um recarregamento; usei `GetStackIssues` e o log do jogo.
- A câmera de batida do `-DNCrashShots` sai com HUD; sem HUD só pelo roteiro `SHOT@t` (tempo do jogo, passo fixo `-UseFixedTimeStep -FPS=60`).
- Para estados do motor sem bater: `EXEC:DNZone 0 0.6@t` (e 0.8, 0.95) no roteiro de teclas.
- O editor do Detonauto foi fechado de fora 3 vezes (`CloseEditor()`, saída limpa) junto com a abertura de outro projeto (Zombie). Não
  é falha do MCP.

**Para o Felipe julgar:** tamanho e densidade das faíscas, se o fogo parece fogo, a cor da fumaça escura do motor, se o pó do poste
e as folhas da árvore aparecem o bastante, e a fumaça de pneu na largada (ver "Para o Felipe julgar" mais acima).
