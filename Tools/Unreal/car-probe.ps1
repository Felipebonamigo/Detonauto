<#
.SYNOPSIS
    Roda o carro de teste (ADNTestCar) sem intervenção e imprime o que o código mediu, ou tira uma foto com o HUD.
.DESCRIPTION
    Abre o jogo em janela com o DNTestCarGameMode e um preset de Data/TestCars.csv. Cenários (-Scenario):
      Sin       acelera e esterça em vai-e-vem (padrão); só confirma que anda e quanto custa o quadro
      Accel     acelerador a fundo em linha reta; mostra a velocidade a cada 0,2 s
      Nitro     igual ao Accel, com o nitro apertado de 1,2 s a 2,4 s (compare com o Accel)
      Handbrake acelera até 55 km/h, faz uma curva com o freio de mão apertado por 0,8 s e imprime o ângulo
                de deslizamento máximo e a guinada máxima (é o que separa um preset derrapador de um que não derrapa)
      Turn      a mesma curva SEM freio de mão (referência)
      Switch    troca de preset andando (arcade-2 em 1,4 s, Padrao em 2,2 s) e mostra que a velocidade se mantém
    Com -Shot, em vez do cenário: acelera, liga o nitro e salva uma captura de tela (com o HUD) em
    <projeto>\Saved\Screenshots\WindowsEditor\dn-hud.png.
    Documentação e interpretação dos números: docs\presets-carro.md.
.EXAMPLE
    .\Tools\Unreal\car-probe.ps1 -Uproject Sandbox\VeiculoTeste\VeiculoTeste.uproject -Row arcade-2 -Scenario Handbrake
    .\Tools\Unreal\car-probe.ps1 -Uproject Sandbox\VeiculoTeste\VeiculoTeste.uproject -Row arcade-3 -Shot
.NOTES
    Feche o editor do Unreal antes de COMPILAR (ele trava a DLL); para rodar este script o editor aberto não atrapalha,
    mas o jogo usa a DLL já compilada. Só grava dentro da pasta do projeto (Saved).
    Os números de uma execução variam um pouco (a física roda em passo livre): compare ordens de grandeza, não decimais.
    Código de saída: 0 se o jogo terminou e deixou log; 1 caso contrário.
#>
param(
    [Parameter(Mandatory)][string]$Uproject,
    [string]$Row = 'Padrao',
    [ValidateSet('Sin', 'Accel', 'Nitro', 'Handbrake', 'Turn', 'Switch')][string]$Scenario = 'Sin',
    [switch]$Shot,
    [string]$Map = '/Game/VehicleTemplate/Maps/VehicleBasic',
    [int]$TimeoutSeconds = 240,
    [string]$EngineVersion
)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'Find-Unreal.ps1')

try { $ctx = Resolve-UnrealContext $Uproject $EngineVersion } catch { Write-Error $_.Exception.Message; exit 1 }
$exe = Join-Path $ctx.Root 'Engine\Binaries\Win64\UnrealEditor.exe'
$projDir = Split-Path $ctx.Uproject
$tag = if ($Shot) { "$Row-shot" } else { "$Row-$Scenario" }
$log = Join-Path $projDir "Saved\Logs\car-probe-$tag.log"
New-Item -ItemType Directory -Force (Split-Path $log) | Out-Null
if (Test-Path $log) { Remove-Item $log }

$a = @("`"$($ctx.Uproject)`"", "$Map`?game=/Script/$($ctx.Name).DNTestCarGameMode", '-game', '-windowed', '-novsync', '-nosplash', '-nosound',
    "-DNCarRow=$Row", "-abslog=`"$log`"")
if ($Shot) { $a += @('-ResX=1280', '-ResY=720', '-ForceRes', '-DNCarShot') }
else { $a += @('-ResX=640', '-ResY=360', '-ForceRes', '-DNCarProbe', "-DNCarScenario=$Scenario") }

$p = Start-Process -FilePath $exe -ArgumentList $a -PassThru
if (-not $p.WaitForExit($TimeoutSeconds * 1000)) { Stop-Process -Id $p.Id; Write-Error "Passou de $TimeoutSeconds s e o jogo foi fechado."; exit 1 }
if (-not (Test-Path $log)) { Write-Error 'O jogo não deixou log.'; exit 1 }

Select-String -Path $log -Pattern 'DNTestCar|DNCarProbe|DNCarShot|Error:|Fatal' |
    ForEach-Object { $_.Line -replace '^\[[^\]]*\]\[[^\]]*\]', '' -replace '^LogVeiculoTeste: Display: ', '' }
if ($Shot) { Get-ChildItem (Join-Path $projDir 'Saved\Screenshots') -Recurse -Filter 'dn-hud.png' -ErrorAction SilentlyContinue | ForEach-Object { "Captura: $($_.FullName)" } }
exit 0
