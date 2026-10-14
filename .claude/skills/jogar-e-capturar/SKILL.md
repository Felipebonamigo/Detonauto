---
name: jogar-e-capturar
description: Abre o carro e a rodada de Crash do Detonauto para jogar, sondar e tirar captura de tela (vehicle-probe.ps1 com -Play, -Scenario, -Shot, -ShotDent, -Keys, -Map L_Lookdev, -AlvoPS5; crash-probe.ps1). Use para "jogar", "abrir o jogo", "tirar print/captura", "medir a derrapagem/curva/turbo", "testar o Crash" ou aprovar um efeito visual.
---
# Jogar, sondar e capturar

Só no PC do Felipe; a nuvem não abre o jogo. Compile antes (skill `verificar-no-pc`): os scripts abrem o
`UnrealEditor.exe -game` com a DLL já compilada. Editor aberto não impede rodar. Não use `car-probe.ps1`: é do protótipo
`ADNTestCar` no `Sandbox/`.

## 1. Carro: `vehicle-probe.ps1`
Por padrão, `ADNVehicle` sobre `/Engine/Maps/Entry` com piso, parede e luz em C++. `-Map /Game/Detonauto/Maps/L_Lookdev` abre a
cena de aparência (o carro nasce no PlayerStart com a tag `DNMapStart`, sem arena). Preset por `-Row` (padrão `arcade-3`;
`Padrao`, `arcade-1`, `arcade-2`, de `Data/Cars.csv`). Teclas do `-Play`: `docs/carro.md`, "Como jogar" (C troca a câmera).
```powershell
.\Tools\Unreal\vehicle-probe.ps1 -Uproject Detonauto.uproject -Play                       # Felipe joga; fecha a janela p/ sair
.\Tools\Unreal\vehicle-probe.ps1 -Uproject Detonauto.uproject -Map /Game/Detonauto/Maps/L_Lookdev -AlvoPS5 -Play
.\Tools\Unreal\vehicle-probe.ps1 -Uproject Detonauto.uproject -Row arcade-2 -Scenario Handbrake
.\Tools\Unreal\vehicle-probe.ps1 -Uproject Detonauto.uproject -Shot      # dn-hud.png; -ShotDent: 8 amassados, 4 capturas sem HUD
```
- `-Scenario`: `Sin` (padrão), `Accel`, `Turbo`, `Handbrake`, `Turn`, `Switch`, `Wall` (salva `dn-dent-parede.png`), `Steer`,
  `Boost`. Ajustes: `-HbKmh`, `-Throttle`, `-Steer` 0 a 1, `-WallKmh` (Wall e Steer), `-TurboOn` (Boost). Cenário roda em 640x360.
- `-ResX`/`-ResY` (1280x720) valem para `-Play`, `-Keys`, `-Shot`, `-ShotDent`. `-AlvoPS5` liga o perfil `DN_AlvoPS5`.
  `-Exec 't.MaxFPS 0','stat unit'` manda comandos de console. `-TimeoutSeconds` 240.
- `-GameMode DNCrashTestGameMode`: estrada de 600 m sem parede, tráfego 20. `-ExtraArgs '-DNTraffic=0'` sem tráfego,
  `'-DNNoAssist'` sem assistência, `'-DNNoDents'` sem amassado.
- Deu certo: saída 0, linhas `DNCarProbe:` e `ADNVehicle: preset '...' aplicado`, nenhuma `Error:`/`Fatal`. Física em passo
  livre: compare ordem de grandeza. Leitura: `docs/presets-carro.md`, `docs/lote-14.md`.

## 2. Teclas reais e captura: `-Keys`
Mesmo modo do `-Play` (janela, teclas pelo `PlayerController`, o caminho do teclado do Felipe), automático. É assim que se
captura "no `-Play`". Ângulos fixos da cena de aparência (`docs/lookdev.md`):
```powershell
.\Tools\Unreal\vehicle-probe.ps1 -Uproject Detonauto.uproject -Map /Game/Detonauto/Maps/L_Lookdev -ResX 1920 -ResY 1080 -AlvoPS5 -Keys "CAM135@4;SHOT@6;CAM90@6.5;SHOT@8.5;CAM180@9;SHOT@11;QUIT@12"
```
- `tecla@início[-fim]`, separados por `;`, o roteiro entre aspas; sem fim, a tecla solta 0,15 s depois. Nomes do Unreal: `W`,
  `A`, `S`, `D`, `SpaceBar`, `LeftShift`, `C`, `T`, `One`... (80 km/h na câmera de jogo: `"W@1-8;SHOT@3.4;QUIT@4.5"`).
- `SHOT@t` sem HUD, `SHOT@t-1` (fim > 0) com HUD; `CAM<graus>@t` câmera parada olhando o carro desse ângulo; `DEMO<n>@t`
  amassados de demonstração; `DENT<fx>:<fy>:<fz>@t` batida numa fração da caixa; `EXEC:<comando>@t` comando de console (sem
  `;` nem `@`); `QUIT@t` fecha.
- Termine com `QUIT` depois do último `SHOT`: sem ele o script espera o timeout, fecha o jogo, sai com erro e não lista as
  capturas. Não junte `-Play` com `-Keys`.

## 3. Crash: `crash-probe.ps1`
```powershell
.\Tools\Unreal\crash-probe.ps1 -Uproject Detonauto.uproject -Kmh 130 -Traffic 40
.\Tools\Unreal\crash-probe.ps1 -Uproject Detonauto.uproject -Play -Traffic 30
```
- `-Kmh` (100), `-Traffic` (20), `-Mode Rail|Physics` (Rail), `-Seed` (12345), `-TimeoutSeconds` (180). Sempre no
  `/Engine/Maps/Entry`; não aceita `-Row`, `-Map`, `-AlvoPS5` nem `-Keys` e não captura. Para capturar o Crash:
  `vehicle-probe.ps1 -GameMode DNCrashTestGameMode -Keys "...;SHOT@..;QUIT@.."`.
- Sem `-Play`, o jogador nasce a `-Kmh` e dirige sozinho. Só deu certo se aparecer `DNCrash: RESULTADO pontos=...
  destruidos=... medalha=... duracao_da_rodada=...`: a saída é 0 mesmo sem essa linha.
- Recordes nunca no save real: pasta temporária no automático, `Saved\SaveGames\DetonautoCrashTest.json` no `-Play`.

## 4. Onde ficam logs e capturas
- Logs: `Saved\Logs\vehicle-probe-<Row>-<play|keys|shot|shotdent|Cenário>.log`, `Saved\Logs\crash-probe-<Kmh>-<Traffic>-<Mode>.log`.
- Capturas: `Saved\Screenshots\WindowsEditor\`: `dn-play-<n>.png` (`-Keys`), `dn-hud.png`, `dn-dent-A-antes`, `A-depois`,
  `B-depois`, `B-antes` (`-ShotDent`), `dn-dent-parede.png`. O script lista também as de rodadas antigas; confira a data:
  `Get-ChildItem Saved\Screenshots -Recurse -Filter 'dn-*.png' | Sort-Object LastWriteTime | Select-Object LastWriteTime, FullName`.
- `Saved\` fica fora do Git. Para a nuvem ver, copie só as que provam para `docs/img/<assunto>/` (PNG e JPG vão pelo LFS;
  precedentes `docs/img/lookdev/`, `docs/img/prototipo-amassado/`). Na nuvem a imagem pode vir como ponteiro LFS de ~130 bytes:
  `git lfs pull --include="docs/img/<assunto>/*"` antes de ler.

## 5. Regra 6: efeito visual só se aprova por captura
1. Captura na cena de aparência (`L_Lookdev`, ângulos 135, 90, 180 e 80 km/h) **e** no modo em que o Felipe joga
   (`-Play`/`-Keys`), antes e depois. Teste verde não basta: no lote 14 o amassado rodava no log e não aparecia na tela.
2. Olhe cada captura com a ferramenta Read antes de afirmar qualquer coisa. Descreva o que se vê, não o que o código faz.
3. Depois de mudar material ou `.ini`, a primeira abertura compila shaders por minutos: captura e medição só valem depois.
4. Custo medido no perfil alvo PS5 (skill `fechar-lote`) e palavra final do Felipe jogando.
- Armadilhas: a câmera de perseguição nunca vê a frente nem o lado esquerdo (use `CAM<graus>`); a frente do `SK_SportsCar` é
  grade sem vértices; a câmera `motorista` mostra o chassi (sem interior); `alta-muito-longe` perdeu o carro a 200+ km/h.
