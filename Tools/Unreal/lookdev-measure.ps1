<#
.SYNOPSIS
    Mede o custo da cena de aparência (L_Lookdev) no perfil "alvo PS5": Frame, Game, Draw e GPU pelo CSV Profiler da engine.
.DESCRIPTION
    Joga o carro no /Game/Detonauto/Maps/L_Lookdev (Tools/Unreal/vehicle-probe.ps1 -AlvoPS5), espera o aquecimento, grava o CSV durante
    a medição e imprime média e p95 de cada tempo. Cenários:
      parado    carro parado na largada, câmera de jogo (perseguição) olhando a rua contra o sol
      andando   acelerador no fundo desde 2 s, medição de 6 a 14 s (ainda na rua): a cena passa pela câmera (sombras e Lumen atualizando)
    Janela 1920x1080 (resolução interna 1080p no perfil), VSync desligado e t.MaxFPS 0. Perfil e protocolo: docs/desempenho.md.
.EXAMPLE
    .\Tools\Unreal\lookdev-measure.ps1 -Uproject Detonauto.uproject
.NOTES
    Feche os outros Unreal antes (o script mede assim mesmo, mas avisa: os números ficam distorcidos). Rode uma vez antes para compilar
    os shaders (a primeira execução depois de mudar material ou configuração é mais lenta). Só lê e grava dentro da pasta do projeto.
    Código de saída: 0 se todos os cenários geraram CSV; 1 caso contrário.
#>
param(
    [Parameter(Mandatory)][string]$Uproject,
    [ValidateSet('parado', 'andando')][string[]]$Scenario = @('parado', 'andando'),
    [int]$Warmup = 15,
    [int]$Measure = 10,
    [string[]]$ExtraArgs = @(),         # argumentos extras para o jogo (ex.: -DNParkedCars=6, os 6 carros amassados do lote 17)
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
Write-Host "Máquina: $gpu | $cpu | janela 1920x1080 | perfil DN_AlvoPS5 | aquecimento $Warmup s, medição $Measure s | extras: $($ExtraArgs -join ' ')"

function Get-Stats([double[]]$v) {
    $s = $v | Sort-Object
    [pscustomobject]@{ Mean = ($v | Measure-Object -Average).Average; P95 = $s[[math]::Min($s.Count - 1, [math]::Floor($s.Count * 0.95))]; N = $v.Count }
}

$failed = 0
$rows = @()
foreach ($name in $Scenario) {
    # andando: o carro sai da largada aos 2 s e a medição vai de 6 a 14 s, antes de ele passar do fim da rua (a ~200 km/h)
    $start, $stop = if ($name -eq 'andando') { 6, 14 } else { $Warmup, ($Warmup + $Measure) }
    $keys = "EXEC:csvprofile start@$start;EXEC:csvprofile stop@$stop;QUIT@$($stop + 2)"
    if ($name -eq 'andando') { $keys = "W@2-$($stop + 1);" + $keys }
    $before = @(Get-ChildItem $csvDir -Filter *.csv -ErrorAction SilentlyContinue | ForEach-Object FullName)
    & (Join-Path $PSScriptRoot 'vehicle-probe.ps1') -Uproject $ctx.Uproject -Map /Game/Detonauto/Maps/L_Lookdev -ResX 1920 -ResY 1080 -AlvoPS5 `
        -Exec 't.MaxFPS 0' -Keys $keys -TimeoutSeconds ($stop + 240) -ExtraArgs $ExtraArgs | Out-Null
    $csv = Get-ChildItem $csvDir -Filter *.csv -ErrorAction SilentlyContinue | Where-Object { $before -notcontains $_.FullName } | Sort-Object LastWriteTime | Select-Object -Last 1
    if (-not $csv) { Write-Host "[$name] sem CSV"; $failed++; continue }
    # Leitura direta (o Import-Csv recusa: o CSV do Unreal pode repetir nomes de coluna). O arquivo termina com linhas de eventos e o
    # cabeçalho repetido: só entram as linhas cujo FrameTime é número.
    $lines = [IO.File]::ReadAllLines($csv.FullName)
    $head = $lines[0].Split(',')
    $inv = [Globalization.CultureInfo]::InvariantCulture
    $num = { param($col)
        $i = [array]::IndexOf($head, $col); $f = [array]::IndexOf($head, 'FrameTime'); $v = 0.0
        [double[]]($lines | Select-Object -Skip 1 | ForEach-Object { $c = $_.Split(','); if ($c.Count -gt $i -and [double]::TryParse($c[$f], [Globalization.NumberStyles]::Float, $inv, [ref]$v)) { [double]::Parse($c[$i], $inv) } }) }
    $r = [pscustomobject]@{ Name = $name; Frame = Get-Stats (& $num 'FrameTime'); Game = Get-Stats (& $num 'GameThreadTime')
        Draw = Get-Stats (& $num 'RenderThreadTime'); Gpu = Get-Stats (& $num 'GPUTime') }
    $rows += $r
    Write-Host ("[{0}] Frame {1:N2} / {2:N2} | Game {3:N2} / {4:N2} | Draw {5:N2} / {6:N2} | GPU {7:N2} / {8:N2} ms (média / p95, {9} quadros) | {10}" -f `
        $name, $r.Frame.Mean, $r.Frame.P95, $r.Game.Mean, $r.Game.P95, $r.Draw.Mean, $r.Draw.P95, $r.Gpu.Mean, $r.Gpu.P95, $r.Frame.N, $csv.Name)
    # Janela sem foco: o driver ou o Windows limitam o jogo a 60 fps mesmo com t.MaxFPS 0, a placa baixa o clock e o tempo de GPU
    # parece maior. Medição assim não vale (medido em 14 out 2026: o mesmo cenário deu 5,1 ms com foco e 16,9 ms travado em 60).
    $fps = $r.Frame.N / ($stop - $start)
    if ([math]::Abs($fps - 60) -lt 2 -and $r.Frame.Mean -gt 16.0) {
        Write-Host "[$name] DESCARTADA: travada em ~60 fps ($([math]::Round($fps, 1)) quadros/s). A janela do jogo perdeu o foco? Repita sem mexer no PC."
        $failed++
    }
}
if ($failed -gt 0) { exit 1 }
exit 0
