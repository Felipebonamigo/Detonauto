<#
.SYNOPSIS
    Roda UMA vez a rodada de Crash de teste (ADNCrashTestGameMode) e imprime o resultado do log: pontos, destruídos, medalha,
    duração, quantos carros de tráfego acordaram. Para jogar, use -Play.
.EXAMPLE
    .\Tools\Unreal\crash-probe.ps1 -Uproject Detonauto.uproject -Kmh 130 -Traffic 40
    .\Tools\Unreal\crash-probe.ps1 -Uproject Detonauto.uproject -Play -Traffic 30
.NOTES
    Grava só dentro da pasta do projeto (Saved). O cenário usa uma pasta temporária para o registro de recordes (nunca o save real).
    Várias medidas (velocidades x quantidades, Rail x Physics, desempenho): Tools\Unreal\crash-measure.ps1.
#>
param(
    [Parameter(Mandatory)][string]$Uproject,
    [int]$Kmh = 100,
    [int]$Traffic = 20,
    [ValidateSet('Rail', 'Physics')][string]$Mode = 'Rail',
    [int]$Seed = 12345,
    [switch]$Play,
    [int]$TimeoutSeconds = 180,
    [string]$EngineVersion
)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'Find-Unreal.ps1')
try { $ctx = Resolve-UnrealContext $Uproject $EngineVersion } catch { Write-Error $_.Exception.Message; exit 1 }
$exe = Join-Path $ctx.Root 'Engine\Binaries\Win64\UnrealEditor.exe'
$projDir = Split-Path $ctx.Uproject
$log = Join-Path $projDir "Saved\Logs\crash-probe-$Kmh-$Traffic-$Mode.log"
New-Item -ItemType Directory -Force (Split-Path $log) | Out-Null
if (Test-Path $log) { Remove-Item $log }
$a = @("`"$($ctx.Uproject)`"", "/Engine/Maps/Entry`?game=/Script/$($ctx.Name).DNCrashTestGameMode", '-game', '-windowed', '-novsync', '-nosplash', '-nosound',
    "-DNTraffic=$Traffic", "-DNTrafficMode=$Mode", "-DNSeed=$Seed", "-abslog=`"$log`"")
if ($Play) { $a += @('-ResX=1280', '-ResY=720', '-ForceRes') }
else { $a += @('-ResX=640', '-ResY=360', '-ForceRes', '-DNCrashScenario=Crash', "-DNCrashKmh=$Kmh") }
$p = Start-Process -FilePath $exe -ArgumentList $a -PassThru
if ($Play) { Write-Host 'Jogo aberto. Feche a janela para terminar.'; $p.WaitForExit(); exit 0 }
if (-not $p.WaitForExit($TimeoutSeconds * 1000)) { Stop-Process -Id $p.Id; Write-Error "Passou de $TimeoutSeconds s."; exit 1 }
Remove-KnownToolsetTracebacks (Get-Content -LiteralPath $log -Encoding UTF8) | Select-String -Pattern 'DNCrash:|DNTraffic:|Error:|Fatal' | ForEach-Object { $_.Line -replace '^\[[^\]]*\]\[[^\]]*\]', '' -replace '^LogDetonauto: Display: ', '' }
exit 0
