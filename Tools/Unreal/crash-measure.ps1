<#
.SYNOPSIS
    Mede a primeira fatia jogável do Crash (lote 13) sem intervenção: batidas contra o tráfego (pontos, destruídos, medalha,
    duração, quantos carros acordaram) e o custo do tráfego (Game/Draw/GPU) com os carros em trilhos contra todos em Physics.
.DESCRIPTION
    Abre o jogo em janela com o ADNCrashTestGameMode (Source/Detonauto/Vehicle) sobre /Engine/Maps/Entry. Cenários:
      C-<km/h>-<n>[-Physics]  o jogador entra na estrada a <km/h> e dirige sozinho contra <n> carros de tráfego; ao fim da rodada
                              imprime pontos, destruídos, medalha, duração e quantos carros acordaram. O registro de recordes usa
                              uma pasta temporária (nunca o save real).
      P-<n>[-Physics]         o jogador fica parado ao lado da estrada; o CSV Profiler mede o Frame/Game/Draw/GPU com <n> carros
                              em trilhos (ou todos Physics, mantendo a velocidade). P-0 é a referência sem tráfego.
    Padrão: C-100-20, C-100-40, C-130-20, C-130-40, C-130-40-Physics, P-0, P-40, P-40-Physics. Relatório e tabela em
    <projeto>\Saved\Measure\<data-hora>.
.EXAMPLE
    .\Tools\Unreal\crash-measure.ps1 -Uproject Detonauto.uproject
    .\Tools\Unreal\crash-measure.ps1 -Uproject Detonauto.uproject -Scenario C-130-40,P-40
.NOTES
    Feche o editor do Unreal antes (ele distorce a medição). Só grava dentro da pasta do projeto (Saved). Os números variam um pouco
    entre execuções (a física roda em passo livre). Código de saída: 0 se todos os cenários geraram dados; 1 caso contrário.
#>
param(
    [Parameter(Mandatory)][string]$Uproject,
    [string[]]$Scenario = @('C-100-20', 'C-100-40', 'C-130-20', 'C-130-40', 'C-130-40-Physics', 'P-0', 'P-40', 'P-40-Physics'),
    [int]$ResX = 1920,
    [int]$ResY = 1080,
    [int]$Warmup = 8,
    [int]$Measure = 10,
    [int]$Seed = 12345,
    [string]$OutDir,
    [string]$EngineVersion
)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'Find-Unreal.ps1')
try { $ctx = Resolve-UnrealContext $Uproject $EngineVersion } catch { Write-Error $_.Exception.Message; exit 1 }
$exe = Join-Path $ctx.Root 'Engine\Binaries\Win64\UnrealEditor.exe'
$projDir = Split-Path $ctx.Uproject
$csvDir = Join-Path $projDir 'Saved\Profiling\CSV'
if (-not $OutDir) { $OutDir = Join-Path $projDir ('Saved\Measure\' + (Get-Date -Format 'yyyyMMdd-HHmmss')) }
$OutDir = if ([IO.Path]::IsPathRooted($OutDir)) { [IO.Path]::GetFullPath($OutDir) } else { [IO.Path]::GetFullPath((Join-Path (Get-Location).Path $OutDir)) }
if (-not ($OutDir + '\').StartsWith(($projDir + '\'), [StringComparison]::OrdinalIgnoreCase)) { Write-Error "-OutDir recusado: fora da pasta do projeto."; exit 1 }
New-Item -ItemType Directory -Force $OutDir | Out-Null
if (Get-Process UnrealEditor, UnrealEditor-Cmd -ErrorAction SilentlyContinue) { Write-Error 'Há um UnrealEditor aberto. Feche antes de medir.'; exit 1 }

$gpu = (Get-CimInstance Win32_VideoController | Select-Object -First 1).Name
$cpu = Get-CimInstance Win32_Processor | Select-Object -First 1
$machine = "$gpu | $($cpu.Name.Trim()) ($($cpu.NumberOfCores) núcleos)"

function Get-Stats([double[]]$v) {
    $s = $v | Sort-Object
    [pscustomobject]@{ Mean = ($v | Measure-Object -Average).Average; P95 = $s[[math]::Min($s.Count - 1, [math]::Floor($s.Count * 0.95))]; Max = $s[-1]; N = $v.Count }
}
function Read-Csv([string]$path) {
    $lines = [IO.File]::ReadAllLines($path)
    $head = $lines[0].Split(',')
    $col = @{}
    foreach ($n in 'FrameTime', 'GameThreadTime', 'RenderThreadTime', 'GPUTime') { $i = [array]::IndexOf($head, $n); if ($i -lt 0) { throw "Coluna $n não existe no CSV" }; $col[$n] = $i }
    $data = @{ FrameTime = @(); GameThreadTime = @(); RenderThreadTime = @(); GPUTime = @() }
    for ($k = 1; $k -lt $lines.Count; $k++) {
        $l = $lines[$k]
        if ($l.StartsWith('EVENTS') -or $l.StartsWith('[')) { continue }
        $v = $l.Split(',')
        foreach ($n in $col.Keys) { $data[$n] += [double]::Parse($v[$col[$n]], [Globalization.CultureInfo]::InvariantCulture) }
    }
    $data
}

$report = @("Medição do Crash (lote 13) - $(Get-Date -Format 'dd/MM/yyyy HH:mm')", "Máquina: $machine", "Janela ${ResX}x${ResY}, VSync desligado, t.MaxFPS 0 | perf: aquecimento ${Warmup} s, medição ${Measure} s | semente ${Seed}", '')
$crashRows = @(); $perfRows = @(); $failed = 0
foreach ($name in $Scenario) {
    $log = Join-Path $OutDir "$name.log"
    $isPerf = $name.StartsWith('P-')
    $parts = $name.Split('-')
    $physics = $name.EndsWith('-Physics')
    $mode = if ($physics) { 'Physics' } else { 'Rail' }
    if ($isPerf) { $n = [int]$parts[1]; $args2 = @("-DNCrashScenario=Traffic", "-DNTraffic=$n", "-DNWarmup=$Warmup", "-DNMeasure=$Measure", '-ExecCmds="t.MaxFPS 0"', "-ResX=$ResX", "-ResY=$ResY") }
    else { $kmh = [int]$parts[1]; $n = [int]$parts[2]; $args2 = @("-DNCrashScenario=Crash", "-DNCrashKmh=$kmh", "-DNTraffic=$n", '-ExecCmds="t.MaxFPS 0"', '-ResX=640', '-ResY=360') }
    $argList = @("`"$($ctx.Uproject)`"", "/Engine/Maps/Entry`?game=/Script/$($ctx.Name).DNCrashTestGameMode", '-game', '-windowed', '-ForceRes', '-novsync', '-nosplash', '-nosound',
        "-DNTrafficMode=$mode", "-DNSeed=$Seed", "-abslog=`"$log`"") + $args2
    $before = @(Get-ChildItem $csvDir -Filter *.csv -ErrorAction SilentlyContinue | ForEach-Object FullName)
    Write-Host "[$name] ..."
    $p = Start-Process $exe -ArgumentList $argList -PassThru
    if (-not $p.WaitForExit(($Warmup + $Measure + 240) * 1000)) { $p.Kill(); Write-Host "[$name] TIMEOUT"; $failed++; $report += "[$name] TIMEOUT"; continue }

    if ($isPerf) {
        $csv = Get-ChildItem $csvDir -Filter *.csv -ErrorAction SilentlyContinue | Where-Object { $before -notcontains $_.FullName } | Sort-Object LastWriteTime | Select-Object -Last 1
        if (-not $csv -or $csv.Length -lt 1000) { Write-Host "[$name] sem CSV (veja $log)"; $failed++; $report += "[$name] sem CSV"; continue }
        Copy-Item $csv.FullName (Join-Path $OutDir "$name.csv")
        $d = Read-Csv $csv.FullName
        $r = [pscustomobject]@{ Name = $name; N = $n; Mode = $mode; Frame = Get-Stats $d.FrameTime; Game = Get-Stats $d.GameThreadTime; Draw = Get-Stats $d.RenderThreadTime; Gpu = Get-Stats $d.GPUTime
            Line = (Select-String -Path $log -Pattern 'DNCrash: TRAFEGO fim' | Select-Object -Last 1 | ForEach-Object { $_.Line -replace '^.*DNCrash: ', '' }) }
        $perfRows += $r
        $report += "== $name ($n carros, $mode)"; $report += "   $($r.Line)"
        foreach ($k in 'Frame', 'Game', 'Draw', 'Gpu') { $x = $r.$k; $report += ("   {0,-5} média {1,7:N2} ms | p95 {2,7:N2} ms | máx {3,7:N2} ms   (n={4})" -f $k, $x.Mean, $x.P95, $x.Max, $x.N) }
        $report += ''
        Write-Host ("[{0}] Frame {1:N2} | Game {2:N2} | Draw {3:N2} | GPU {4:N2} ms" -f $name, $r.Frame.Mean, $r.Game.Mean, $r.Draw.Mean, $r.Gpu.Mean)
    }
    else {
        $line = Select-String -Path $log -Pattern 'DNCrash: RESULTADO' | Select-Object -Last 1
        if (-not $line) { Write-Host "[$name] sem resultado (veja $log)"; $failed++; $report += "[$name] sem resultado"; continue }
        $text = $line.Line -replace '^.*DNCrash: RESULTADO ', ''
        $crashRows += [pscustomobject]@{ Name = $name; Kmh = $kmh; N = $n; Mode = $mode; Text = $text }
        $report += "== $name"; $report += "   $text"; $report += ''
        Write-Host "[$name] $text"
    }
}

$table = @()
if ($crashRows) {
    $table += '| Cenário | Km/h | Tráfego | Modo | Pontos | Destruídos | Medalha | Duração da rodada | Acordaram | Batidas em Rail antes de acordar |', '|---|---|---|---|---|---|---|---|---|---|'
    foreach ($c in $crashRows) {
        $g = { param($k) if ($c.Text -match "$k=(\S+)") { $Matches[1] } else { '?' } }
        $table += "| $($c.Name) | $($c.Kmh) | $($c.N) | $($c.Mode) | $(& $g 'pontos') | $(& $g 'destruidos') | $(& $g 'medalha') | $(& $g 'duracao_da_rodada') s | $(& $g 'acordaram') de $(& $g 'nasceram') | $(& $g 'batidas_em_rail_antes_de_acordar') |"
    }
    $table += ''
}
if ($perfRows) {
    $table += '| Cenário | Carros | Modo | Frame média / p95 | Game média / p95 | Draw média / p95 | GPU média / p95 |', '|---|---|---|---|---|---|---|'
    $base = $perfRows | Where-Object { $_.N -eq 0 } | Select-Object -First 1
    foreach ($r in $perfRows) {
        $cell = { param($s) ('{0:N2} / {1:N2} ms' -f $s.Mean, $s.P95) }
        $table += "| $($r.Name) | $($r.N) | $($r.Mode) | $(& $cell $r.Frame) | $(& $cell $r.Game) | $(& $cell $r.Draw) | $(& $cell $r.Gpu) |"
    }
    if ($base) {
        $table += ''
        foreach ($r in ($perfRows | Where-Object { $_.N -gt 0 })) {
            $table += ("Custo por carro de tráfego em Game ({0}, {1} carros): {2:N3} ms ((Game com tráfego - Game sem tráfego) / carros)." -f $r.Mode, $r.N, (($r.Game.Mean - $base.Game.Mean) / $r.N))
        }
    }
}
$report += '== TABELA'; $report += $table
[IO.File]::WriteAllLines((Join-Path $OutDir 'relatorio.txt'), $report, (New-Object Text.UTF8Encoding($true)))
[IO.File]::WriteAllLines((Join-Path $OutDir 'tabela.md'), $table, (New-Object Text.UTF8Encoding($false)))
Write-Host ''; $table | ForEach-Object { Write-Host $_ }
Write-Host "Relatório: $(Join-Path $OutDir 'relatorio.txt')"
if ($failed -gt 0) { exit 1 }
