<#
.SYNOPSIS
    Cria a cena de aparência /Game/Detonauto/Maps/L_Lookdev (mapa, materiais e texturas CC0 importadas) sem abrir o editor.
    Roda Tools/Unreal/create_lookdev.py pelo commandlet de Python.
.EXAMPLE
    .\Tools\Unreal\create_lookdev.ps1 -Uproject Detonauto.uproject
.NOTES
    As texturas de origem (.jpg do Poly Haven, CC0) ficam em Sandbox\texturas_cc0, fora do Git; o script importa e o projeto guarda só
    os .uasset. Só grava pacotes em /Game/Detonauto (o .py recusa qualquer outro destino) e o relatório em
    <projeto>\Saved\LookdevReport.txt. Sempre REFAZ o mapa e os materiais nos mesmos pacotes. Feche o editor antes.
    Código de saída: 0 deu certo; 1 erro (mensagem no console e no relatório). O que a cena tem: docs/plano-visual.md.
#>
param(
    [Parameter(Mandatory)][string]$Uproject,
    [string]$Textures,
    [string]$Models,           # modelos CC0 do mobiliário de rua (padrão: Sandbox\modelos_cc0; sem a pasta, a cena sai sem eles)
    [string[]]$Reimport = @(), # ids do mobiliário para importar de novo por cima (ex.: jacaranda_tree_stump,jacaranda_tree_top)
    [string]$EngineVersion,
    [int]$TimeoutSeconds = 1200
)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'Find-Unreal.ps1')

try { $ctx = Resolve-UnrealContext $Uproject $EngineVersion } catch { Write-Error $_.Exception.Message; exit 1 }
$projDir = Split-Path $ctx.Uproject
if (-not $Textures) { $Textures = Join-Path $projDir 'Sandbox\texturas_cc0' }
if (-not (Test-Path -LiteralPath $Textures)) { Write-Host "ERRO: pasta de texturas não encontrada: $Textures"; exit 1 }

$exe = Join-Path $ctx.Root 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$script = Join-Path $PSScriptRoot 'create_lookdev.py'
$log = Join-Path $projDir 'Saved\Logs\create-lookdev.log'
$report = Join-Path $projDir 'Saved\LookdevReport.txt'
if (Test-Path -LiteralPath $report) { Remove-Item -LiteralPath $report -Force }

# O commandlet trata a string de -Script= com escapes (\r viraria retorno de carro): use barras normais.
if (-not $Models) { $Models = Join-Path $projDir 'Sandbox\modelos_cc0' }
$pyArgs = "`"$($script.Replace('\', '/'))`" --textures `"$($Textures.Replace('\', '/'))`""
if (Test-Path -LiteralPath $Models) { $pyArgs += " --models `"$($Models.Replace('\', '/'))`"" }
if ($Reimport.Count -gt 0) { $pyArgs += " --reimport $($Reimport -join ',')" }

# Python embutido não pode gravar .pyc na instalação da engine (também travado dentro do .py).
$env:PYTHONDONTWRITEBYTECODE = '1'
$argList = @("`"$($ctx.Uproject)`"", '-run=PythonScript', "-Script=`"$pyArgs`"", '-unattended', '-nullrhi', '-nosplash', '-nosound', "-abslog=`"$log`"")
Write-Host "Criando L_Lookdev (limite ${TimeoutSeconds}s)"
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
