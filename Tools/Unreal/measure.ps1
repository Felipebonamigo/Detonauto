<#
.SYNOPSIS
    Mede o tempo de quadro do jogo em modo Standalone (janela, sem editor) com carga crescente.
.DESCRIPTION
    Para cada cenário, abre o jogo com o GameMode de medição (DNMeasureGameMode, em C++ no
    projeto), espera o aquecimento, liga o CSV Profiler do Unreal, mede, desliga e sai. Depois lê
    o CSV e calcula média, p95 e máximo de Frame, Game, Draw e GPU. Escreve um relatório em texto e
    uma tabela em Markdown.

    Frame = FrameTime, Game = GameThreadTime, Draw = RenderThreadTime, GPU = GPUTime (colunas do
    CSV Profiler; em ms). "Draw" é o tempo da thread de render, o mesmo do "Draw" do stat unit.

    REQUISITO: o projeto precisa ter o GameMode de medição (classe ADNMeasureGameMode, arquivos
    DNMeasureGameMode.h/.cpp). Há uma cópia de referência em Tools\Unreal\measure-gamemode\, com um
    README que explica de que ela depende e como copiá-la para o projeto.

    ATENÇÃO: os carros extras são SUBSTITUTOS para estimar custo (corpos físicos iguais ao do
    jogador). Não são o tráfego real nem o dano real do jogo.
.EXAMPLE
    .\Tools\Unreal\measure.ps1 -Uproject Sandbox\VeiculoTeste\VeiculoTeste.uproject
    .\Tools\Unreal\measure.ps1 -Uproject X.uproject -Scenario T-1,T-41 -Warmup 10 -Measure 20
.NOTES
    Feche o editor do Unreal antes (ele trava arquivos e distorce a medição). Janelas do jogo vão
    abrir por alguns minutos e o PC fica ocupado. Só grava dentro da pasta do projeto (Saved).
    Código de saída: 0 se todos os cenários geraram dados; 1 caso contrário.
#>
param(
    [Parameter(Mandatory)][string]$Uproject,
    [string[]]$Scenario,                     # padrão: todos. Nomes: ver $All abaixo
    [string]$Map = '/Game/VehicleTemplate/Maps/VehicleBasic',
    [int]$ResX = 1920,
    [int]$ResY = 1080,
    [int]$Warmup = 30,
    [int]$Measure = 60,
    [int]$Seed = 12345,
    [string]$OutDir,                         # padrão: <projeto>\Saved\Measure\<data-hora>
    [string]$EngineVersion
)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'Find-Unreal.ps1')

# Nome -> carro (Template = BP_SportsCar_Pawn do template; DN = ADNTestCar), extras, movimento, escala de render
$All = [ordered]@{
    'T-1'     = @{ Car = 'Template'; Extra = 0;  Move = $false; Sp = 0  }
    'T-21'    = @{ Car = 'Template'; Extra = 20; Move = $false; Sp = 0  }
    'T-41'    = @{ Car = 'Template'; Extra = 40; Move = $false; Sp = 0  }
    'T-41M'   = @{ Car = 'Template'; Extra = 40; Move = $true;  Sp = 0  }
    'T-41@54' = @{ Car = 'Template'; Extra = 40; Move = $false; Sp = 54 }   # mesma carga em 54% de escala de render
    'D-1'     = @{ Car = 'DN';       Extra = 0;  Move = $false; Sp = 0  }
    'D-21'    = @{ Car = 'DN';       Extra = 20; Move = $false; Sp = 0  }
    'D-41'    = @{ Car = 'DN';       Extra = 40; Move = $false; Sp = 0  }
    'D-41M'   = @{ Car = 'DN';       Extra = 40; Move = $true;  Sp = 0  }
    # Protótipo de amassado (lote 10): carros DN com o jogador parado e os extras na vista. Dent: none = materiais originais;
    # mat = material de amassado SEM amassados; dents = material com 8 amassados de profundidade máxima em cada carro.
    'X-1'     = @{ Car = 'DN'; Extra = 0;  Move = $false; Sp = 0; Dent = 'none';  Still = $true }
    'X-6'     = @{ Car = 'DN'; Extra = 5;  Move = $false; Sp = 0; Dent = 'none';  Still = $true }
    'X-12'    = @{ Car = 'DN'; Extra = 11; Move = $false; Sp = 0; Dent = 'none';  Still = $true }
    'XM-1'    = @{ Car = 'DN'; Extra = 0;  Move = $false; Sp = 0; Dent = 'mat';   Still = $true }
    'XM-6'    = @{ Car = 'DN'; Extra = 5;  Move = $false; Sp = 0; Dent = 'mat';   Still = $true }
    'XM-12'   = @{ Car = 'DN'; Extra = 11; Move = $false; Sp = 0; Dent = 'mat';   Still = $true }
    'XD-1'    = @{ Car = 'DN'; Extra = 0;  Move = $false; Sp = 0; Dent = 'dents'; Still = $true }
    'XD-6'    = @{ Car = 'DN'; Extra = 5;  Move = $false; Sp = 0; Dent = 'dents'; Still = $true }
    'XD-12'   = @{ Car = 'DN'; Extra = 11; Move = $false; Sp = 0; Dent = 'dents'; Still = $true }
}
if (-not $Scenario) { $Scenario = @($All.Keys) }
foreach ($s in $Scenario) { if (-not $All.Contains($s)) { Write-Error "Cenário desconhecido: $s. Válidos: $($All.Keys -join ', ')"; exit 1 } }

try { $ctx = Resolve-UnrealContext $Uproject $EngineVersion } catch { Write-Error $_.Exception.Message; exit 1 }
$exe = Join-Path $ctx.Root 'Engine\Binaries\Win64\UnrealEditor.exe'
$projDir = Split-Path $ctx.Uproject
$csvDir = Join-Path $projDir 'Saved\Profiling\CSV'
if (-not $OutDir) { $OutDir = Join-Path $projDir ('Saved\Measure\' + (Get-Date -Format 'yyyyMMdd-HHmmss')) }
# Caminho SEMPRE absoluto (o Unreal resolveria um caminho relativo a partir da pasta da engine e gravaria lá)
# e SEMPRE dentro da pasta do projeto.
$OutDir = if ([IO.Path]::IsPathRooted($OutDir)) { [IO.Path]::GetFullPath($OutDir) } else { [IO.Path]::GetFullPath((Join-Path (Get-Location).Path $OutDir)) }
if (-not ($OutDir + '\').StartsWith(($projDir + '\'), [StringComparison]::OrdinalIgnoreCase)) {
    Write-Error "-OutDir recusado: '$OutDir' fica fora da pasta do projeto ($projDir)."; exit 1
}
New-Item -ItemType Directory -Force $OutDir | Out-Null

if (Get-Process UnrealEditor, UnrealEditor-Cmd -ErrorAction SilentlyContinue) {
    Write-Error "Há um UnrealEditor aberto. Feche antes de medir."; exit 1
}

# --- máquina (somente leitura) ---
$gpu = (Get-CimInstance Win32_VideoController | Select-Object -First 1).Name
$cpu = (Get-CimInstance Win32_Processor | Select-Object -First 1)
$ram = [math]::Round((Get-CimInstance Win32_PhysicalMemory | Measure-Object Capacity -Sum).Sum / 1GB)
$machine = "$gpu | $($cpu.Name.Trim()) ($($cpu.NumberOfCores) núcleos) | ${ram} GB RAM"

function Get-Stats([double[]]$v) {
    $s = $v | Sort-Object
    [pscustomobject]@{
        Mean = ($v | Measure-Object -Average).Average
        P95  = $s[[math]::Min($s.Count - 1, [math]::Floor($s.Count * 0.95))]
        Max  = $s[-1]
        N    = $v.Count
    }
}

function Read-Csv([string]$path) {
    $lines = [IO.File]::ReadAllLines($path)
    $head = $lines[0].Split(',')
    $col = @{}
    foreach ($n in 'FrameTime', 'GameThreadTime', 'RenderThreadTime', 'GPUTime') {
        $i = [array]::IndexOf($head, $n)
        if ($i -lt 0) { throw "Coluna $n não existe no CSV $path" }
        $col[$n] = $i
    }
    $data = @{ FrameTime = @(); GameThreadTime = @(); RenderThreadTime = @(); GPUTime = @() }
    for ($k = 1; $k -lt $lines.Count; $k++) {
        $l = $lines[$k]
        if ($l.StartsWith('EVENTS') -or $l.StartsWith('[')) { continue }   # cabeçalho repetido e metadados no fim
        $v = $l.Split(',')
        foreach ($n in $col.Keys) { $data[$n] += [double]::Parse($v[$col[$n]], [Globalization.CultureInfo]::InvariantCulture) }
    }
    $data
}

$results = @()
$failed = 0
$report = @()
$report += "Medição de desempenho - $(Get-Date -Format 'dd/MM/yyyy HH:mm')"
$report += "Máquina: $machine"
$report += "Resolução da janela: ${ResX}x${ResY} (janela, Standalone, sem editor) | qualidade: sg.* da configuração do projeto (sem alteração) | VSync desligado, t.MaxFPS 0"
$report += "Aquecimento ${Warmup} s, medição ${Measure} s por cenário | semente ${Seed}"
$report += "Os carros extras são SUBSTITUTOS para estimar custo; não são o tráfego real nem o dano real."
$report += ""

foreach ($name in $Scenario) {
    $sc = $All[$name]
    $log = Join-Path $OutDir "$name.log"
    $before = @(Get-ChildItem $csvDir -Filter *.csv -ErrorAction SilentlyContinue | ForEach-Object FullName)

    $cmds = 't.MaxFPS 0'
    # -ExecCmds separa comandos com VÍRGULA (com ';' só o primeiro valia: bug que invalidou a 1ª rodada a 54%)
    if ($sc.Sp -gt 0) { $cmds += ",r.ScreenPercentage $($sc.Sp)" }
    $argList = @(
        "`"$($ctx.Uproject)`"", "$Map`?game=/Script/VeiculoTeste.DNMeasureGameMode",
        '-game', '-windowed', "-ResX=$ResX", "-ResY=$ResY", '-ForceRes', '-novsync', '-nosplash', '-nosound',
        "-ExecCmds=`"$cmds`"", "-DNCarClass=$($sc.Car)", "-DNExtra=$($sc.Extra)",
        "-DNWarmup=$Warmup", "-DNMeasure=$Measure", "-DNSeed=$Seed", "-abslog=`"$log`""
    )
    if ($sc.Move) { $argList += '-DNExtraMove' }
    if ($sc.Still) { $argList += '-DNStill' }
    # O carro DN agora liga o amassado por padrão (para o Felipe jogar); as medições de referência o desligam.
    if ($sc.Car -eq 'DN') {
        switch ($sc.Dent) {
            'mat'   { $argList += '-DNDentMaterial' }
            'dents' { $argList += '-DNDents=8' }
            default { $argList += '-DNNoDents' }
        }
    }

    Write-Host "[$name] carro=$($sc.Car) extras=$($sc.Extra) movendo=$($sc.Move) escala=$(if($sc.Sp){"$($sc.Sp)%"}else{'padrão'}) ..."
    $p = Start-Process $exe -ArgumentList $argList -PassThru
    $limit = ($Warmup + $Measure + 240) * 1000
    if (-not $p.WaitForExit($limit)) { $p.Kill(); Write-Host "[$name] TIMEOUT"; $failed++; $report += "[$name] TIMEOUT"; continue }

    $csv = Get-ChildItem $csvDir -Filter *.csv -ErrorAction SilentlyContinue | Where-Object { $before -notcontains $_.FullName } | Sort-Object LastWriteTime | Select-Object -Last 1
    if (-not $csv -or $csv.Length -lt 1000) { Write-Host "[$name] sem CSV (veja $log)"; $failed++; $report += "[$name] sem CSV"; continue }
    Copy-Item $csv.FullName (Join-Path $OutDir "$name.csv")

    $d = Read-Csv $csv.FullName
    $r = [pscustomobject]@{
        Name = $name; Car = $sc.Car; Extra = $sc.Extra; Move = $sc.Move; Sp = $sc.Sp; Dent = $sc.Dent
        Frame = Get-Stats $d.FrameTime; Game = Get-Stats $d.GameThreadTime
        Draw = Get-Stats $d.RenderThreadTime; Gpu = Get-Stats $d.GPUTime
    }
    $results += $r

    $logLines = Select-String -Path $log -CaseSensitive -Pattern 'DNMeasure:' | ForEach-Object { $_.Line -replace '^\[[^\]]*\]\[[^\]]*\]LogVeiculoTeste: Display: ', '' }
    $report += "== $name  (carro $($sc.Car), extras $($sc.Extra), movendo $($sc.Move))"
    $report += $logLines
    foreach ($k in 'Frame', 'Game', 'Draw', 'Gpu') {
        $x = $r.$k
        $report += ("   {0,-5} média {1,7:N2} ms | p95 {2,7:N2} ms | máx {3,7:N2} ms   (n={4})" -f $k, $x.Mean, $x.P95, $x.Max, $x.N)
    }
    $report += ""
    Write-Host ("[{0}] Frame {1:N2} | Game {2:N2} | Draw {3:N2} | GPU {4:N2} ms (médias)" -f $name, $r.Frame.Mean, $r.Game.Mean, $r.Draw.Mean, $r.Gpu.Mean)
}

# --- tabela Markdown ---
$table = @()
$table += '| Cenário | Extras | Frame média / p95 | Game média / p95 | Draw média / p95 | GPU média / p95 |'
$table += '|---|---|---|---|---|---|'
foreach ($r in $results) {
    $cell = { param($s) ('{0:N2} / {1:N2} ms' -f $s.Mean, $s.P95) }
    $label = "$($r.Name) ($($r.Car)$(if($r.Move){', em movimento'}elseif($r.Extra){', parados'})$(if($r.Sp){", escala $($r.Sp)%"})$(if($r.Dent){", amassado: $($r.Dent)"}))"
    $table += "| $label | $($r.Extra) | $(& $cell $r.Frame) | $(& $cell $r.Game) | $(& $cell $r.Draw) | $(& $cell $r.Gpu) |"
}
$report += "== TABELA"
$report += $table
$reportPath = Join-Path $OutDir 'relatorio.txt'
[IO.File]::WriteAllLines($reportPath, $report, (New-Object Text.UTF8Encoding($true)))
[IO.File]::WriteAllLines((Join-Path $OutDir 'tabela.md'), $table, (New-Object Text.UTF8Encoding($false)))
Write-Host ""
$table | ForEach-Object { Write-Host $_ }
Write-Host "Relatório: $reportPath"
if ($failed -gt 0 -or $results.Count -ne $Scenario.Count) { exit 1 }
exit 0
