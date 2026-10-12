<#
.SYNOPSIS
    Compila o alvo Editor (Win64, Development) de um projeto Unreal.
.EXAMPLE
    .\Tools\Unreal\build.ps1 -Uproject Sandbox\VeiculoTeste\VeiculoTeste.uproject
.NOTES
    Código de saída: 0 compilou; diferente de 0 deu erro (o do UnrealBuildTool,
    ou 1 se o script falhou antes). Só grava dentro da pasta do projeto
    (Intermediate/Binaries) e em $env:TEMP.
#>
param(
    [Parameter(Mandatory)][string]$Uproject,
    [string]$EngineVersion,  # padrão: lida do .uproject (EngineAssociation)
    [string]$Configuration = 'Development'
)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'Find-Unreal.ps1')

try {
    $ctx = Resolve-UnrealContext $Uproject $EngineVersion
} catch {
    Write-Error $_.Exception.Message
    exit 1
}

$buildBat = Join-Path $ctx.Root 'Engine\Build\BatchFiles\Build.bat'
$target = "$($ctx.Name)Editor"
Write-Host "Engine: $($ctx.Root)"
Write-Host "Alvo:   $target Win64 $Configuration"

& $buildBat $target Win64 $Configuration "-Project=$($ctx.Uproject)" -WaitMutex -NoHotReloadFromIDE
$code = $LASTEXITCODE
if ($code -eq 0) { Write-Host "BUILD OK" } else { Write-Host "BUILD FALHOU (código $code)" }
exit $code
