<#
.SYNOPSIS
    Leva o conjunto mínimo do carro esportivo do template (22 assets) do projeto de teste para Content/Detonauto/Vehicle/SportsCar
    do projeto principal, com os nomes do padrão do CLAUDE.md e as referências corrigidas. Não traz o pacote inteiro do template.
.DESCRIPTION
    1. copia os 22 arquivos .uasset de <ProjetoOrigem>\Content\Vehicles\SportsCar para a pasta de passagem Content\Vehicles\SportsCar
       do projeto principal (recusa se a passagem já existir);
    2. roda Tools/Unreal/migrate_sports_car.py pelo commandlet de Python: renomeia para /Game/Detonauto/Vehicle/SportsCar, apaga a
       passagem e confere o resultado.
    O que sobra no Git são só os 22 .uasset novos (Git LFS). Não usa o Migrate do editor nem grava nada na instalação da engine.
.EXAMPLE
    .\Tools\Unreal\migrate_sports_car.ps1 -Uproject Detonauto.uproject -SourceProject Sandbox\VeiculoTeste
.NOTES
    Feche os editores do Unreal dos dois projetos antes. Código de saída: 0 deu certo; 1 erro.
    Lista dos 22 assets e dos nomes novos: dentro do .py. Licença: docs/licencas.csv.
#>
param(
    [Parameter(Mandatory)][string]$Uproject,
    [Parameter(Mandatory)][string]$SourceProject,       # pasta do projeto de teste (a que tem Content\Vehicles\SportsCar)
    [string]$EngineVersion,
    [int]$TimeoutSeconds = 900
)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'Find-Unreal.ps1')

try { $ctx = Resolve-UnrealContext $Uproject $EngineVersion } catch { Write-Error $_.Exception.Message; exit 1 }
$projDir = Split-Path $ctx.Uproject
$py = Join-Path $PSScriptRoot 'migrate_sports_car.py'

# Lista dos arquivos = os mesmos do .py (fonte única: lê do .py)
$names = Select-String -Path $py -Pattern '^\s+\("([^"]+)",\s*"[^"]+",\s*"[^"]+"\),?$' | ForEach-Object { $_.Matches[0].Groups[1].Value }
if ($names.Count -ne 22) { Write-Host "ERRO: esperava 22 assets no .py, achei $($names.Count)."; exit 1 }

$srcRoot = Join-Path (Resolve-Path -LiteralPath $SourceProject).Path 'Content\Vehicles\SportsCar'
$dstRoot = Join-Path $projDir 'Content\Vehicles\SportsCar'
if (-not (Test-Path -LiteralPath $srcRoot)) { Write-Host "ERRO: não achei $srcRoot"; exit 1 }
if (Test-Path -LiteralPath (Join-Path $projDir 'Content\Vehicles')) { Write-Host "ERRO: a pasta de passagem $(Join-Path $projDir 'Content\Vehicles') já existe; apague-a antes (é só passagem)."; exit 1 }

foreach ($n in $names) {
    $from = Join-Path $srcRoot (($n -replace '/', '\') + '.uasset')
    $to = Join-Path $dstRoot (($n -replace '/', '\') + '.uasset')
    if (-not (Test-Path -LiteralPath $from)) { Write-Host "ERRO: falta $from"; exit 1 }
    New-Item -ItemType Directory -Force (Split-Path $to) | Out-Null
    Copy-Item -LiteralPath $from -Destination $to
}
Write-Host "Copiados $($names.Count) arquivos para a passagem $dstRoot"

$exe = Join-Path $ctx.Root 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$log = Join-Path $projDir 'Saved\Logs\migrate-sports-car.log'
$report = Join-Path $projDir 'Saved\MigrateSportsCarReport.txt'
if (Test-Path -LiteralPath $report) { Remove-Item -LiteralPath $report -Force }
$env:PYTHONDONTWRITEBYTECODE = '1'
$argList = @("`"$($ctx.Uproject)`"", '-run=PythonScript', "-Script=`"$($py.Replace('\', '/'))`"", '-unattended', '-nullrhi', '-nosplash', '-nosound', "-abslog=`"$log`"")
$p = Start-Process $exe -ArgumentList $argList -PassThru -WindowStyle Hidden
if (-not $p.WaitForExit($TimeoutSeconds * 1000)) { $p.Kill(); Write-Host "TIMEOUT depois de ${TimeoutSeconds}s. Log: $log"; exit 1 }
if (Test-Path -LiteralPath $report) { Get-Content -LiteralPath $report -Encoding UTF8 | ForEach-Object { Write-Host $_ } } else { Write-Host "Sem relatório. Veja o log: $log" }
if ($p.ExitCode -ne 0) { Write-Host "FALHOU (código $($p.ExitCode)). Log: $log"; exit 1 }
Write-Host "OK"
exit 0
