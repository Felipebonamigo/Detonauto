<#
.SYNOPSIS
    Cria o material-pai do protótipo de amassado (M_DNDentParent) sem abrir o editor.
    Roda Tools/Unreal/create_dent_material.py pelo commandlet de Python.
.EXAMPLE
    .\Tools\Unreal\create_dent_material.ps1 -Uproject Sandbox\VeiculoTeste\VeiculoTeste.uproject -Dest /Game/Sandbox_Teste
.NOTES
    Código de saída: 0 deu certo; 1 erro (mensagem no console e no relatório).
    Só grava pacotes em /Game/ do projeto (o .py e este script recusam qualquer outro destino) e o relatório em
    <projeto>\Saved\DentMaterialReport.txt. Sempre RECRIA o material (apaga o anterior com o mesmo nome).
    O que o material faz e como é alimentado: docs/prototipo-amassado.md.
#>
param(
    [Parameter(Mandatory)][string]$Uproject,
    [string]$Dest = '/Game/Sandbox_Teste',
    [string]$EngineVersion,
    [int]$TimeoutSeconds = 600,
    [switch]$InstancesOnly      # só refaz as instâncias e veste o carro (sem refazer o material-pai): mudança só de valores
)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'Find-Unreal.ps1')

# Primeira trava (a segunda está no .py): nada fora de /Game/.
if ($Dest -notmatch '^/Game(/[A-Za-z0-9_]+)+$') {
    Write-Host "ERRO: destino recusado '$Dest'. Só /Game/<pasta> do projeto é aceito."
    exit 1
}

try { $ctx = Resolve-UnrealContext $Uproject $EngineVersion } catch { Write-Error $_.Exception.Message; exit 1 }

$exe = Join-Path $ctx.Root 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$script = Join-Path $PSScriptRoot 'create_dent_material.py'
$projDir = Split-Path $ctx.Uproject
$log = Join-Path $projDir 'Saved\Logs\create-dent-material.log'
$report = Join-Path $projDir 'Saved\DentMaterialReport.txt'
if (Test-Path -LiteralPath $report) { Remove-Item -LiteralPath $report -Force }

# O commandlet trata a string de -Script= com escapes (\r viraria retorno de carro): use barras normais.
$pyArgs = "`"$($script.Replace('\', '/'))`" --dest $Dest"
if ($InstancesOnly) { $pyArgs += ' --instances-only' }

# Python embutido não pode gravar .pyc na instalação da engine (também travado dentro do .py).
$env:PYTHONDONTWRITEBYTECODE = '1'
$argList = @("`"$($ctx.Uproject)`"", '-run=PythonScript', "-Script=`"$pyArgs`"", '-unattended', '-nullrhi', '-nosplash', '-nosound', "-abslog=`"$log`"")
Write-Host "Criando M_DNDentParent em $Dest (limite ${TimeoutSeconds}s)"
$p = Start-Process $exe -ArgumentList $argList -PassThru -WindowStyle Hidden
if (-not $p.WaitForExit($TimeoutSeconds * 1000)) { $p.Kill(); Write-Host "TIMEOUT depois de ${TimeoutSeconds}s. Log: $log"; exit 1 }

if (Test-Path -LiteralPath $report) {
    Write-Host "--- relatório ($report) ---"
    Get-Content -LiteralPath $report -Encoding UTF8 | ForEach-Object { Write-Host $_ }
} else {
    Write-Host "Sem relatório. Veja o log: $log"
}
if ($p.ExitCode -ne 0) { Write-Host "FALHOU (código $($p.ExitCode)). Log: $log"; exit 1 }
Write-Host "OK"
exit 0
