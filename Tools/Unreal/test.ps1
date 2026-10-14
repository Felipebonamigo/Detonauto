<#
.SYNOPSIS
    Roda os testes de Automation do projeto sem janela e resume aprovados/reprovados.
.EXAMPLE
    .\Tools\Unreal\test.ps1 -Uproject Sandbox\VeiculoTeste\VeiculoTeste.uproject
    .\Tools\Unreal\test.ps1 -Uproject X.uproject -Filter Detonauto.Pontuacao -TimeoutSeconds 900
.NOTES
    Código de saída: 0 tudo passou; 1 algum teste falhou, nenhum teste rodou,
    timeout ou erro do script. Grava só em <projeto>\Saved.
    Compile antes (build.ps1): o editor sem janela carrega os módulos já compilados.
    O filtro padrão é o nome do projeto: os testes do jogo devem se chamar
    "<NomeDoProjeto>.Algo". Sem isso o Unreal rodaria também os testes da engine.
#>
param(
    [Parameter(Mandatory)][string]$Uproject,
    [string]$Filter,                  # prefixo dos testes; padrão: nome do projeto
    [string]$EngineVersion,
    [int]$TimeoutSeconds = 600
)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'Find-Unreal.ps1')

try {
    $ctx = Resolve-UnrealContext $Uproject $EngineVersion
} catch {
    Write-Error $_.Exception.Message
    exit 1
}
if (-not $Filter) { $Filter = $ctx.Name }

$exe = Join-Path $ctx.Root 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$projDir = Split-Path $ctx.Uproject
$report = Join-Path $projDir 'Saved\AutomationReport'
$log = Join-Path $projDir 'Saved\Logs\test-automation.log'
if (Test-Path -LiteralPath $report) { Remove-Item -LiteralPath $report -Recurse -Force }

$cmds = "Automation RunTests $Filter; Quit"
$argList = @(
    "`"$($ctx.Uproject)`"", "-ExecCmds=`"$cmds`"",
    '-unattended', '-nullrhi', '-nosplash', '-nosound', '-NoSteam',
    "-ReportExportPath=`"$report`"", "-abslog=`"$log`"", '-stdout'
)
Write-Host "Rodando: Automation RunTests $Filter (limite ${TimeoutSeconds}s)"
$p = Start-Process $exe -ArgumentList $argList -PassThru -WindowStyle Hidden
if (-not $p.WaitForExit($TimeoutSeconds * 1000)) {
    $p.Kill()
    Write-Host "TIMEOUT depois de ${TimeoutSeconds}s. Log: $log"
    exit 1
}

$index = Join-Path $report 'index.json'
if (-not (Test-Path -LiteralPath $index)) {
    Write-Host "Sem relatório em $report. Veja o log: $log"
    exit 1
}
$r = Get-Content -LiteralPath $index -Raw -Encoding UTF8 | ConvertFrom-Json
$passed = [int]$r.succeeded
$failed = [int]$r.failed
$notRun = [int]$r.notRun
$total = @($r.tests).Count

Write-Host ""
Write-Host "RESUMO: $passed aprovados, $failed reprovados, $notRun não rodaram, $total no total"
foreach ($t in $r.tests) {
    $estado = if ($t.state -eq 'Success') { 'OK    ' } elseif ($t.state -eq 'Fail') { 'FALHOU' } else { $t.state }
    Write-Host "  [$estado] $($t.fullTestPath)"
    if ($t.state -eq 'Fail') {
        foreach ($e in $t.entries | Where-Object { $_.event.type -eq 'Error' }) { Write-Host "           $($e.event.message)" }
    }
}

if ($total -eq 0) { Write-Host "Nenhum teste encontrado para o filtro '$Filter'."; exit 1 }
if ($failed -gt 0 -or $notRun -gt 0) { exit 1 }
exit 0
