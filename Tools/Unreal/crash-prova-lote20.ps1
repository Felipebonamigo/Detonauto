<#
.SYNOPSIS
    Prova da decisão 2 do lote 20: roda o cenário do Crash (padrão 130 km/h contra 40 carros) em Rail e em Physics, por semente, e imprime pontos, carros atingidos e duração.
.DESCRIPTION
    Os logs ficam em Saved\Prova-lote20 (um por modo e semente); o resumo por carro vem das linhas "DNCrashCarro" e "DNTraffic: batida em". Resultado e análise: docs/prova-crash-rail-physics.md.
.EXAMPLE
    .\Tools\Unreal\crash-prova-lote20.ps1 -Seeds 12345,777 -Kmh 100
#>
param([int[]]$Seeds = @(12345, 777, 4242), [int]$Kmh = 130, [int]$N = 40, [string]$Tag = 'C')
Set-Location C:\Users\Felip\source\repos\Detonauto\Detonauto
$exe = 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe'
$proj = (Resolve-Path Detonauto.uproject).Path
$dir = Join-Path (Get-Location).Path 'Saved\Prova-lote20'
New-Item -ItemType Directory -Force $dir | Out-Null
foreach ($seed in $Seeds) {
    foreach ($mode in 'Rail', 'Physics') {
        $log = Join-Path $dir "$Tag-$Kmh-$N-$mode-$seed.log"
        $a = @("`"$proj`"", '/Engine/Maps/Entry?game=/Script/Detonauto.DNCrashTestGameMode', '-game', '-windowed', '-ForceRes', '-novsync', '-nosplash', '-nosound',
            "-DNTrafficMode=$mode", "-DNSeed=$seed", "-abslog=`"$log`"", '-DNCrashScenario=Crash', "-DNCrashKmh=$Kmh", "-DNTraffic=$N", '-ExecCmds="t.MaxFPS 0"', '-ResX=640', '-ResY=360')
        $p = Start-Process $exe -ArgumentList $a -PassThru
        if (-not $p.WaitForExit(300000)) { $p.Kill(); Write-Output "TIMEOUT $mode $seed"; continue }
        $res = Select-String -Path $log -Pattern 'DNCrash: RESULTADO|DNCrashCarros' | ForEach-Object { $_.Line -replace '^.*(DNCrash: RESULTADO|DNCrashCarros:)', '$1' }
        $txt = $res -join ' || '
        Write-Output ("[$mode $seed] " + $txt.Substring(0, [Math]::Min(460, $txt.Length)))
    }
}
