<#
.SYNOPSIS
    Refaz a carroceria com os alvos de deformação do dano por zona (SK_DNSportsCarBody), sem abrir o editor.
.DESCRIPTION
    Três etapas, em ordem (contrato: docs/regras-dano-visual.md):
      1. Unreal (commandlet): exporta SM_SportsCar e SM_SportsCar_Glass para Sandbox\dano\;
      2. Blender: Tools\Blender\make_damage_morphs.py une as duas malhas, cria os 5 alvos e o osso e grava as imagens de
         conferência em docs\img\dano-zonas\;
      3. Unreal (commandlet): importa Sandbox\dano\SK_DNSportsCarBody.fbx em /Game/Detonauto/Vehicle/SportsCar e confere
         alvos e limites.
    Com -Parts (lote 18b, docs/regras-pecas.md), as etapas 2 e 3 são outras: Tools\Blender\split_car_parts.py separa as peças que
    soltam e os vidros (mapa em Tools\Blender\parts\SportsCar.json) para Sandbox\pecas\, com as imagens em docs\img\pecas\, e a
    importação põe a carroceria sem as peças (com os vãos) em SK_DNSportsCarBody e cada peça em /Game/Detonauto/Vehicle/SportsCar/Parts.
.EXAMPLE
    .\Tools\Unreal\damage-body.ps1 -Uproject Detonauto.uproject
    .\Tools\Unreal\damage-body.ps1 -Uproject Detonauto.uproject -Parts
.NOTES
    Código de saída: 0 deu certo; 1 erro. Relatório do Unreal em Saved\DamageBodyReport.txt. Feche o editor antes
    (o commandlet não grava sobre um asset aberto). Só grava em Sandbox\dano, docs\img\dano-zonas e na pasta do carro.
#>
param(
    [Parameter(Mandatory)][string]$Uproject,
    [string]$Blender = 'C:\Program Files\Blender Foundation\Blender 5.2\blender.exe',
    [int]$Seed = 17,
    [string]$Renders = 'docs\img\dano-zonas-17b',  # pasta das imagens de conferência (as do lote 17 ficam em docs\img\dano-zonas)
    [string]$EngineVersion,
    [int]$TimeoutSeconds = 900,
    [switch]$Parts                                   # lote 18b: split_car_parts.py e a importação das peças (Sandbox\pecas)
)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'Find-Unreal.ps1')

try { $ctx = Resolve-UnrealContext $Uproject $EngineVersion } catch { Write-Error $_.Exception.Message; exit 1 }
if (-not (Test-Path -LiteralPath $Blender)) { Write-Host "ERRO: Blender não encontrado em $Blender"; exit 1 }

$exe = Join-Path $ctx.Root 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$projDir = Split-Path $ctx.Uproject
$repo = Split-Path (Split-Path $PSScriptRoot)
$work = Join-Path $projDir 'Sandbox\dano'
$renders = Join-Path $repo $Renders
$report = Join-Path $projDir 'Saved\DamageBodyReport.txt'
$env:PYTHONDONTWRITEBYTECODE = '1'

$partsWork = Join-Path $projDir 'Sandbox\pecas'

function Invoke-UnrealStep([string]$Mode, [string]$Dir = $work) {
    if (Test-Path -LiteralPath $report) { Remove-Item -LiteralPath $report -Force }
    $log = Join-Path $projDir "Saved\Logs\damage-body-$Mode.log"
    # O commandlet trata a string de -Script= com escapes (\r viraria retorno de carro): use barras normais.
    $pyArgs = "`"$((Join-Path $PSScriptRoot 'damage_body.py').Replace('\', '/'))`" --mode $Mode --dir $($Dir.Replace('\', '/'))"
    $argList = @("`"$($ctx.Uproject)`"", '-run=PythonScript', "-Script=`"$pyArgs`"", '-unattended', '-nullrhi', '-nosplash', '-nosound', "-abslog=`"$log`"")
    Write-Host "Unreal: $Mode (limite ${TimeoutSeconds}s)"
    $p = Start-Process $exe -ArgumentList $argList -PassThru -WindowStyle Hidden
    if (-not $p.WaitForExit($TimeoutSeconds * 1000)) { $p.Kill(); Write-Host "TIMEOUT. Log: $log"; return $false }
    if (-not (Test-Path -LiteralPath $report)) { Write-Host "Sem relatório. Log: $log"; return $false }
    $lines = Get-Content -LiteralPath $report -Encoding UTF8
    $lines | ForEach-Object { Write-Host "  $_" }
    # o código do commandlet vem 1 com qualquer erro no log, até de plugin alheio: vale o relatório do script
    return -not ($lines -match '^ERRO:')
}

if (-not (Invoke-UnrealStep 'export')) { Write-Host 'FALHOU na exportação'; exit 1 }

if ($Parts) {
    # lote 18b: a carroceria sem as peças (com os vãos) e uma malha por peça e por vidro, todas com os mesmos 5 alvos
    Write-Host 'Blender: peças que soltam e vidros'
    $inputs = @('SM_SportsCar.fbx', 'SM_SportsCar_Glass.fbx') | ForEach-Object { Join-Path $work $_ }
    & $Blender -b --python-exit-code 1 -P (Join-Path $repo 'Tools\Blender\split_car_parts.py') -- `
        --inputs @inputs --map (Join-Path $repo 'Tools\Blender\parts\SportsCar.json') --wheels (Join-Path $work 'wheels.json') `
        --out-dir $partsWork --renders (Join-Path $repo 'docs\img\pecas') --seed $Seed 2>&1 |
        Where-Object { $_ -match '\[split_car_parts\]' } | ForEach-Object { Write-Host "  $_" }
    if ($LASTEXITCODE -ne 0) { Write-Host "FALHOU no Blender (código $LASTEXITCODE)"; exit 1 }
    if (-not (Invoke-UnrealStep 'import-parts' $partsWork)) { Write-Host 'FALHOU na importação das peças'; exit 1 }
    Write-Host 'OK'
    exit 0
}

Write-Host 'Blender: alvos de deformação'
$inputs = @('SM_SportsCar.fbx', 'SM_SportsCar_Glass.fbx') | ForEach-Object { Join-Path $work $_ }
& $Blender -b --python-exit-code 1 -P (Join-Path $repo 'Tools\Blender\make_damage_morphs.py') -- `
    --inputs @inputs --out (Join-Path $work 'SK_DNSportsCarBody.fbx') --wheels (Join-Path $work 'wheels.json') `
    --renders $renders --seed $Seed 2>&1 |
    Where-Object { $_ -match '\[make_damage_morphs\]' } | ForEach-Object { Write-Host "  $_" }
if ($LASTEXITCODE -ne 0) { Write-Host "FALHOU no Blender (código $LASTEXITCODE)"; exit 1 }

if (-not (Invoke-UnrealStep 'import')) { Write-Host 'FALHOU na importação'; exit 1 }
Write-Host 'OK'
exit 0
