<#
.SYNOPSIS
    Importa uma pasta de modelos (.fbx/.obj) como Static Mesh e cria um material por modelo,
    sem abrir o editor. Roda Tools/Unreal/import_models.py pelo commandlet de Python.
.EXAMPLE
    .\Tools\Unreal\import_models.ps1 -Uproject Sandbox\VeiculoTeste\VeiculoTeste.uproject `
        -Src Sandbox\VeiculoTeste\Data\TestModels -Dest /Game/Sandbox_Teste
.NOTES
    Código de saída: 0 deu certo; 1 erro (mensagem no console e no relatório).
    Só grava pacotes em /Game/ do projeto (o .py e este script recusam qualquer outro destino)
    e o relatório em <projeto>\Saved\ImportReport.txt. Não é recursivo.
#>
param(
    [Parameter(Mandatory)][string]$Uproject,
    [Parameter(Mandatory)][string]$Src,
    [Parameter(Mandatory)][string]$Dest,           # ex.: /Game/Sandbox_Teste
    [string]$BaseMaterial,                         # ex.: /Game/Sandbox_Teste/M_Meu (padrão: cria M_DNBase)
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

try {
    $ctx = Resolve-UnrealContext $Uproject $EngineVersion
} catch {
    Write-Error $_.Exception.Message
    exit 1
}

$exe = Join-Path $ctx.Root 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$script = Join-Path $PSScriptRoot 'import_models.py'
$projDir = Split-Path $ctx.Uproject
$log = Join-Path $projDir 'Saved\Logs\import-models.log'
$report = Join-Path $projDir 'Saved\ImportReport.txt'
if (Test-Path -LiteralPath $report) { Remove-Item -LiteralPath $report -Force }

$srcFull = if (Test-Path -LiteralPath $Src) { (Resolve-Path -LiteralPath $Src).Path } else { [IO.Path]::GetFullPath((Join-Path (Get-Location).Path $Src)) }
# O commandlet trata a string de -Script= com escapes (\r viraria retorno de carro): use barras normais.
$scriptArg = $script.Replace('\', '/')
$srcArg = $srcFull.Replace('\', '/')
$pyArgs = "`"$scriptArg`" --src `"$srcArg`" --dest $Dest"
if ($BaseMaterial) { $pyArgs += " --base-material $BaseMaterial" }

# Python embutido não pode gravar .pyc na instalação da engine (também travado dentro do .py).
$env:PYTHONDONTWRITEBYTECODE = '1'
$argList = @(
    "`"$($ctx.Uproject)`"", '-run=PythonScript', "-Script=`"$pyArgs`"",
    '-unattended', '-nullrhi', '-nosplash', '-nosound', "-abslog=`"$log`""
)
Write-Host "Importando '$srcFull' para $Dest (limite ${TimeoutSeconds}s)"
$p = Start-Process $exe -ArgumentList $argList -PassThru -WindowStyle Hidden
if (-not $p.WaitForExit($TimeoutSeconds * 1000)) {
    $p.Kill()
    Write-Host "TIMEOUT depois de ${TimeoutSeconds}s. Log: $log"
    exit 1
}

if (Test-Path -LiteralPath $report) {
    Write-Host "--- relatório ($report) ---"
    Get-Content -LiteralPath $report -Encoding UTF8 | ForEach-Object { Write-Host $_ }
} else {
    Write-Host "Sem relatório. Veja o log: $log"
}

if ($p.ExitCode -ne 0) {
    Write-Host "FALHOU (código $($p.ExitCode))."
    exit 1
}
Write-Host "OK"
exit 0
