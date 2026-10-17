<#
.SYNOPSIS
    Mede o custo da corrida (percurso 1 com prédios, rivais e HUD) no perfil "alvo PS5": Frame, Game, Draw e GPU pelo CSV Profiler.
.DESCRIPTION
    Joga o ADNRaceGameMode em piloto automático (-DNRaceAuto) com o perfil DN_AlvoPS5 em 1920x1080 (Tools/Unreal/vehicle-probe.ps1),
    grava o CSV entre o aquecimento e o fim da janela e imprime média e p95 de cada tempo. Cenários:
      sem-rivais   -DNRivals=0: só o jogador
      com-rivais   os 3 rivais de Data/Rivals.csv
    A contagem de largada dura 3 s; a medição vai de -Warmup a -Warmup + -Measure segundos (com o carro andando nas retas e nas curvas).
    Protocolo: docs/desempenho.md.
.EXAMPLE
    .\Tools\Unreal\race-measure.ps1 -Uproject Detonauto.uproject
.NOTES
    Feche os outros Unreal antes (o script mede assim mesmo, mas descarta a rodada: a GPU é dividida). A primeira execução depois de mudar
    material ou configuração é mais lenta (shaders): rode duas vezes. Só lê e grava dentro da pasta do projeto.
    Código de saída: 0 se todos os cenários geraram CSV válido; 1 caso contrário.
#>
param(
    [Parameter(Mandatory)][string]$Uproject,
    [ValidateSet('sem-rivais', 'com-rivais')][string[]]$Scenario = @('sem-rivais', 'com-rivais'),
    [int]$Warmup = 10,
    [int]$Measure = 30,
    [string[]]$ExtraArgs = @(),
    [string]$EngineVersion
)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'Find-Unreal.ps1')
try { $ctx = Resolve-UnrealContext $Uproject $EngineVersion } catch { Write-Error $_.Exception.Message; exit 1 }
$projDir = Split-Path $ctx.Uproject
$csvDir = Join-Path $projDir 'Saved\Profiling\CSV'

$others = @(Get-Process UnrealEditor, UnrealEditor-Cmd -ErrorAction SilentlyContinue)
if ($others.Count -gt 0) { Write-Host "AVISO: há $($others.Count) Unreal aberto(s) ($(($others | ForEach-Object MainWindowTitle | Where-Object { $_ }) -join '; ')): os números ficam distorcidos." }
$gpu = (Get-CimInstance Win32_VideoController | Select-Object -First 1).Name
$cpu = (Get-CimInstance Win32_Processor | Select-Object -First 1).Name.Trim()
Write-Host "Máquina: $gpu | $cpu | janela 1920x1080 | perfil DN_AlvoPS5 | aquecimento $Warmup s, medição $Measure s"

function Get-Stats([double[]]$v) {
    $s = $v | Sort-Object
    [pscustomobject]@{ Mean = ($v | Measure-Object -Average).Average; P95 = $s[[math]::Min($s.Count - 1, [math]::Floor($s.Count * 0.95))]; N = $v.Count }
}

$failed = 0
foreach ($name in $Scenario) {
    $start = $Warmup
    $stop = $Warmup + $Measure
    $rivals = if ($name -eq 'sem-rivais') { '-DNRivals=0' } else { '-DNRivals=3' }
    $keys = "EXEC:csvprofile start@$start;EXEC:csvprofile stop@$stop;QUIT@$($stop + 2)"
    $busyBefore = @(Get-Process UnrealEditor, UnrealEditor-Cmd -ErrorAction SilentlyContinue).Count
    $before = @(Get-ChildItem $csvDir -Filter *.csv -ErrorAction SilentlyContinue | ForEach-Object FullName)
    & (Join-Path $PSScriptRoot 'vehicle-probe.ps1') -Uproject $ctx.Uproject -GameMode DNRaceGameMode -ResX 1920 -ResY 1080 -AlvoPS5 `
        -Exec 't.MaxFPS 0' -Keys $keys -TimeoutSeconds ($stop + 240) -ExtraArgs (@('-DNRaceAuto', $rivals, '-DNRaceMaxS=300') + $ExtraArgs) | Out-Null
    $csv = Get-ChildItem $csvDir -Filter *.csv -ErrorAction SilentlyContinue | Where-Object { $before -notcontains $_.FullName } | Sort-Object LastWriteTime | Select-Object -Last 1
    Start-Sleep -Seconds 3
    $busyAfter = @(Get-Process UnrealEditor, UnrealEditor-Cmd -ErrorAction SilentlyContinue).Count
    if (-not $csv) { Write-Host "[$name] sem CSV"; $failed++; continue }
    $lines = [IO.File]::ReadAllLines($csv.FullName)
    $head = $lines[0].Split(',')
    $inv = [Globalization.CultureInfo]::InvariantCulture
    $num = { param($col)
        $i = [array]::IndexOf($head, $col); $f = [array]::IndexOf($head, 'FrameTime'); $v = 0.0
        [double[]]($lines | Select-Object -Skip 1 | ForEach-Object { $c = $_.Split(','); if ($c.Count -gt $i -and [double]::TryParse($c[$f], [Globalization.NumberStyles]::Float, $inv, [ref]$v)) { [double]::Parse($c[$i], $inv) } }) }
    $r = [pscustomobject]@{ Frame = Get-Stats (& $num 'FrameTime'); Game = Get-Stats (& $num 'GameThreadTime'); Draw = Get-Stats (& $num 'RenderThreadTime'); Gpu = Get-Stats (& $num 'GPUTime') }
    Write-Host ("[{0}] Frame {1:N2} / {2:N2} | Game {3:N2} / {4:N2} | Draw {5:N2} / {6:N2} | GPU {7:N2} / {8:N2} ms (média / p95, {9} quadros) | {10}" -f `
        $name, $r.Frame.Mean, $r.Frame.P95, $r.Game.Mean, $r.Game.P95, $r.Draw.Mean, $r.Draw.P95, $r.Gpu.Mean, $r.Gpu.P95, $r.Frame.N, $csv.Name)
    if ($busyBefore -gt 0 -or $busyAfter -gt 0) {
        Write-Host "[$name] DESCARTADA: havia outro Unreal aberto ($busyBefore antes, $busyAfter depois). Repita com o PC livre."
        $failed++
        continue
    }
    $fps = $r.Frame.N / ($stop - $start)
    if ([math]::Abs($fps - 60) -lt 2 -and $r.Frame.Mean -gt 16.0) {
        Write-Host "[$name] DESCARTADA: travada em ~60 fps ($([math]::Round($fps, 1)) quadros/s). A janela do jogo perdeu o foco? Repita sem mexer no PC."
        $failed++
    }
}
if ($failed -gt 0) { exit 1 }
exit 0
