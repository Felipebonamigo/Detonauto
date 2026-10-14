# Estado atual do projeto

Documento de passagem. Uma sessão nova do Claude (local ou na nuvem) lê este
arquivo, o `CLAUDE.md` e o `docs/plano-visual.md` e consegue continuar sem o
histórico das conversas. Atualizar a cada lote fechado.

Última revisão: 14 out 2026.

## Como o trabalho está dividido

- **Sessão da nuvem:** desenho, contratos em `docs/`, revisão do código no
  GitHub, cronograma e conversa com o Felipe.
- **Sessão local (PC do Felipe):** compila, roda testes, joga cenários
  automáticos, escreve C++ e scripts, faz as capturas.
- **Felipe:** joga e decide sensação, visual e escopo.
- Fluxo de cada lote: contrato com números em `docs/` → sessão local implementa
  com testes primeiro e prova por mutação → commits pequenos na `main` → a nuvem
  revisa e aprova → o Felipe joga quando houver o que sentir.

## O que existe (origin/main)

| Área | Onde | Estado |
|---|---|---|
| Regras puras (turbo, Crash, takedown, medalhas, dano, tráfego, globo, progresso, save, roda visual, parâmetros do amassado) | `Source/Detonauto/Core/` | Testadas; mais de 500 testes de Automation |
| Dados de ajuste em CSV com validador e teste de deriva | `Data/` | Cidades, cruzamentos, tráfego, turbo, Crash, takedown, dano, carros |
| Subsistemas de dados e de save | `Core/DNGameDataSubsystem`, `Core/DNRecordsSubsystem` | Testados sem mundo |
| Carro jogável | `Vehicle/ADNVehicle` | Presets por dados, turbo real, freio de mão com rampa, amassado por material, HUD de depuração |
| Tráfego e rodada de Crash | `Traffic/`, `Vehicle/DNCrashTestGameMode` | Lote 13 parcial: código na main, medição ainda não rodou |
| Mapa de teste | `ADNVehicleArena`, criado por código no `/Engine/Maps/Entry` | Feio de propósito; **não serve para julgar visual** |
| Arte | `Content/Detonauto/Vehicle/SportsCar` | Carro do template da Epic, provisório |

## Lotes

| Lote | Assunto | Estado |
|---|---|---|
| 1–7 | Regras puras, dados, validadores, save, subsistemas e componentes | Aprovados |
| 8 | Presets arcade no Sandbox | Aprovado; substituído pelo lote 12 |
| 9 | Regras do dano | Aprovado |
| 10–11 | Protótipo do amassado e calibração da batida | Aprovados; técnica revista em `docs/plano-visual.md` |
| 12 | Carro no projeto principal | Aprovado |
| 13 | Tráfego real e rodada de Crash | Parcial; falta a medição |
| 14 | Bugs do 1º playtest (rodas, amassado, curva) e ajustes (derrapagem, turbo, câmeras) | B1–B3 na main; J1–J4 em andamento |

## Primeiro playtest (14 out)

Notas em `docs/notas-playtest-1.md`. Resumo: arcade-3 é o mais próximo do gosto
do Felipe; presets 0 e 1 duros; arcade-2 derrapa sozinho; turbo fraco demais;
rodas estavam de lado e borradas; amassado não aparecia; câmera mais longe é
melhor e ele quer cinco modos.

## Armadilhas já descobertas

- `bUseUnity = false` no `Detonauto.Build.cs`; o agrupamento unity fazia funções
  de teste colidirem.
- Nunca usar `[..]` no padrão de `AddExpectedError`: vira classe de caracteres e
  engole falhas.
- `FString ==` ignora maiúsculas e minúsculas; use `Equals(..., ESearchCase::CaseSensitive)` quando importar.
- `FJsonObject` junta chaves que diferem só na caixa; para detectar repetição,
  ler em fluxo com `TJsonReader`.
- Chaos: o atrito da roda é multiplicado pelo do material físico do piso
  (o template usa 1,0; o padrão da engine é 0,7). Qualquer ajuste de derrapagem
  depende do piso.
- Chaos: `SetMassOverrideInKg` não vale; a massa vem do `Mass` do componente.
- Chaos: `RollbarScaling` só é lido ao criar o veículo; trocar preset andando
  exige `RecreatePhysicsState` e zerar a marcha alvo.
- Aderência traseira tem "precipício": produto aderência × freio de mão perto de
  0,30 muda de 12° para 40° de deslize; passos de 0,02.
- Material em malha Nanite precisa da flag de uso com Nanite, senão a engine
  troca pelo material padrão.
- Parâmetro vetorial entrega só RGB ao nó Custom do material.
- Teste verde não prova efeito visível: todo efeito visual precisa de captura no
  modo `-Play` (ver `docs/plano-visual.md`).

## Pendências do Felipe

- Instalar o **Blender** (necessário para o dano por zona e para preparar carros).
- Busca de "Detonauto" no INPI, classes 9 e 41.
- Verify da engine no Epic Launcher (40 arquivos regravados no início).
- Backup em HD externo.
- Próximo playtest quando o lote 14 fechar.

## Próximo passo

1. Fechar o lote 14 (J1–J4) e o lote 13 (medição).
2. Começar o plano visual pela base de renderização e pela cena de aparência
   (`docs/plano-visual.md`, passos 1 e 2).
