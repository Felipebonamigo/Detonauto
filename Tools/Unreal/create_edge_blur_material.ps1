<#
.SYNOPSIS
    Cria o material de pós-processamento do desfoque nas bordas (M_DNEdgeBlur, lote 18) sem abrir o editor.
    Roda Tools/Unreal/create_edge_blur_material.py pelo commandlet de Python.
.EXAMPLE
    .\Tools\Unreal\create_edge_blur_material.ps1 -Uproject Detonauto.uproject
.NOTES
    Código de saída: 0 deu certo; 1 erro (mensagem no console e no relatório).
    Só grava pacotes em /Game/ do projeto (o .py e este script recusam qualquer outro destino) e o relatório em
    <projeto>\Saved\EdgeBlurMaterialReport.txt. Rodar de novo refaz o material no mesmo pacote.
    O que o material faz: docstring do .py e docs/regras-camera.md.
#>
param(
    [Parameter(Mandatory)][string]$Uproject,
    [string]$Dest = '/Game/Detonauto/Camera',
    [string]$EngineVersion,
    [int]$TimeoutSeconds = 600
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
$script = Join-Path $PSScriptRoot 'create_edge_blur_material.py'
$projDir = Split-Path $ctx.Uproject
$log = Join-Path $projDir 'Saved\Logs\create-edge-blur-material.log'
$report = Join-Path $projDir 'Saved\EdgeBlurMaterialReport.txt'
if (Test-Path -LiteralPath $report) { Remove-Item -LiteralPath $report -Force }

# O commandlet trata a string de -Script= com escapes (\r viraria retorno de carro): use barras normais.
$pyArgs = "`"$($script.Replace('\', '/'))`" --dest $Dest"

# Python embutido não pode gravar .pyc na instalação da engine (também travado dentro do .py).
$env:PYTHONDONTWRITEBYTECODE = '1'
$argList = @("`"$($ctx.Uproject)`"", '-run=PythonScript', "-Script=`"$pyArgs`"", '-unattended', '-nullrhi', '-nosplash', '-nosound', "-abslog=`"$log`"")
Write-Host "Criando M_DNEdgeBlur em $Dest (limite ${TimeoutSeconds}s)"
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
