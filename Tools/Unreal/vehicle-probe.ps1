<#
.SYNOPSIS
    Joga o carro do projeto (ADNVehicle) em um mapa vazio da engine com piso e parede criados em C++: para jogar (-Play) ou para
    rodar um cenário sem intervenção e imprimir o que o código mediu, ou tirar uma foto com o HUD.
.DESCRIPTION
    Abre o jogo em janela com o ADNVehicleTestGameMode (Source/Detonauto/Vehicle) sobre /Engine/Maps/Entry, um mapa vazio que já vem
    na engine: nenhum .umap do projeto e nenhum Blueprint. O preset vem de Data/Cars.csv.
    -Play    para o Felipe jogar: W/S acelera e freia, A/D esterça, Espaço = freio de mão, Shift = turbo, 1/2/3/0 = presets
             (arcade-1/2/3/Padrao), T/Y/U = batida simulada na frente, G/H/J = no lado esquerdo, V = limpa os amassados.
    Cenários (-Scenario), sem intervenção:
      Sin       acelera e esterça em vai-e-vem (padrão); só confirma que anda
      Accel     acelerador a fundo em linha reta; mostra a velocidade a cada 0,2 s
      Turbo     igual ao Accel, com o turbo apertado de 1,2 s a 2,4 s (compare com o Accel)
      Handbrake acelera até 55 km/h, faz uma curva com o freio de mão apertado por 0,8 s e imprime o ângulo de deslizamento máximo
      Turn      a mesma curva SEM freio de mão (referência)
      Switch    troca de preset andando (arcade-2 em 1,4 s, Padrao em 2,2 s) e mostra que a velocidade se mantém
      Wall      reto contra a parede (a fundo, ou segurando -WallKmh): mostra as batidas reais e os amassados, salva dn-dent-parede.png
    Com -ShotDent: carro parado, 8 amassados de demonstração e quatro capturas só da cena em <projeto>\Saved\Screenshots\WindowsEditor.
    Com -Shot: acelera, liga o turbo e salva uma captura com o HUD (dn-hud.png).
    Interpretação dos números: docs\presets-carro.md.
.EXAMPLE
    .\Tools\Unreal\vehicle-probe.ps1 -Uproject Detonauto.uproject -Play
    .\Tools\Unreal\vehicle-probe.ps1 -Uproject Detonauto.uproject -Row arcade-2 -Scenario Handbrake
    .\Tools\Unreal\vehicle-probe.ps1 -Uproject Detonauto.uproject -Row arcade-3 -Shot
.NOTES
    Feche o editor do Unreal antes de COMPILAR (ele trava a DLL); para rodar este script o editor aberto não atrapalha, mas o jogo
    usa a DLL já compilada. Só grava dentro da pasta do projeto (Saved). Os números variam um pouco entre execuções (a física roda
    em passo livre): compare ordens de grandeza, não decimais. Código de saída: 0 se o jogo terminou e deixou log; 1 caso contrário.
#>
param(
    [Parameter(Mandatory)][string]$Uproject,
    [string]$Row = 'Padrao',
    [ValidateSet('Sin', 'Accel', 'Turbo', 'Handbrake', 'Turn', 'Switch', 'Wall')][string]$Scenario = 'Sin',
    [switch]$Play,
    [switch]$Shot,
    [switch]$ShotDent,
    [int]$WallKmh = 0,                   # cenário Wall: segura esta velocidade na aproximação (0 = acelerador a fundo)
    [string]$Map = '/Engine/Maps/Entry',
    [int]$TimeoutSeconds = 240,
    [string]$EngineVersion
)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'Find-Unreal.ps1')

try { $ctx = Resolve-UnrealContext $Uproject $EngineVersion } catch { Write-Error $_.Exception.Message; exit 1 }
$exe = Join-Path $ctx.Root 'Engine\Binaries\Win64\UnrealEditor.exe'
$projDir = Split-Path $ctx.Uproject
$tag = if ($Play) { "$Row-play" } elseif ($ShotDent) { "$Row-shotdent" } elseif ($Shot) { "$Row-shot" } else { "$Row-$Scenario" }
$log = Join-Path $projDir "Saved\Logs\vehicle-probe-$tag.log"
New-Item -ItemType Directory -Force (Split-Path $log) | Out-Null
if (Test-Path $log) { Remove-Item $log }

$a = @("`"$($ctx.Uproject)`"", "$Map`?game=/Script/$($ctx.Name).DNVehicleTestGameMode", '-game', '-windowed', '-novsync', '-nosplash', '-nosound',
    "-DNCarRow=$Row", "-abslog=`"$log`"")
if ($Play) { $a += @('-ResX=1280', '-ResY=720', '-ForceRes') }
elseif ($ShotDent) { $a += @('-ResX=1280', '-ResY=720', '-ForceRes', '-DNShotDent') }
elseif ($Shot) { $a += @('-ResX=1280', '-ResY=720', '-ForceRes', '-DNCarShot') }
else {
    $a += @('-ResX=640', '-ResY=360', '-ForceRes', '-DNCarProbe', "-DNCarScenario=$Scenario")
    if ($WallKmh -gt 0) { $a += "-DNWallKmh=$WallKmh" }
}

$p = Start-Process -FilePath $exe -ArgumentList $a -PassThru
if ($Play) { Write-Host 'Jogo aberto. Feche a janela para terminar.'; $p.WaitForExit(); exit 0 }
if (-not $p.WaitForExit($TimeoutSeconds * 1000)) { Stop-Process -Id $p.Id; Write-Error "Passou de $TimeoutSeconds s e o jogo foi fechado."; exit 1 }
if (-not (Test-Path $log)) { Write-Error 'O jogo não deixou log.'; exit 1 }

Select-String -Path $log -Pattern 'ADNVehicle|DNCarProbe|DNCarShot|DNDent|LogDetonauto|Error:|Fatal' |
    ForEach-Object { $_.Line -replace '^\[[^\]]*\]\[[^\]]*\]', '' -replace '^LogDetonauto: Display: ', '' }
if ($Shot -or $ShotDent -or $Scenario -eq 'Wall') { Get-ChildItem (Join-Path $projDir 'Saved\Screenshots') -Recurse -Filter 'dn-*.png' -ErrorAction SilentlyContinue | ForEach-Object { "Captura: $($_.FullName)" } }
exit 0
