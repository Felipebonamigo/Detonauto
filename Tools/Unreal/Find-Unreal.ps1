# Funções compartilhadas pelos scripts de Tools/Unreal. Não é para rodar sozinho.
# Descobre a pasta da engine sem caminho fixo: registro do Windows primeiro,
# depois o LauncherInstalled.dat do Epic Games Launcher.

function Get-ProjectEngineVersion([string]$Uproject) {
    $json = Get-Content -LiteralPath $Uproject -Raw | ConvertFrom-Json
    $assoc = [string]$json.EngineAssociation
    # Só vale versão do tipo "5.8". GUID ou vazio: o chamador decide o que fazer.
    if ($assoc -match '^\d+\.\d+$') { return $assoc }
    return $null
}

function Find-UnrealRoot([string]$Version) {
    if (-not $Version) { throw "Versão da engine desconhecida. Informe -EngineVersion (ex.: 5.8)." }

    $key = "HKLM:\SOFTWARE\EpicGames\Unreal Engine\$Version"
    if (Test-Path $key) {
        $dir = (Get-ItemProperty $key).InstalledDirectory
        if ($dir -and (Test-Path -LiteralPath $dir)) { return $dir }
    }

    $dat = Join-Path $env:ProgramData 'Epic\UnrealEngineLauncher\LauncherInstalled.dat'
    if (Test-Path $dat) {
        $item = (Get-Content -LiteralPath $dat -Raw | ConvertFrom-Json).InstallationList |
            Where-Object { $_.AppName -eq "UE_$Version" } | Select-Object -First 1
        if ($item -and (Test-Path -LiteralPath $item.InstallLocation)) { return $item.InstallLocation }
    }

    throw "Unreal Engine $Version não encontrada (registro e LauncherInstalled.dat)."
}

# Erros conhecidos e inofensivos que os scripts tiram das linhas "Error:" que mostram. Lista explícita: nada além disto some.
#  - Python dos plugins experimentais Engine/Plugins/Experimental/Toolsets/* (ConversationToolset, StateTreeToolset, NiagaraToolsets,
#    EditorToolset, AnimationAssistantToolset...) e Engine/Plugins/Experimental/ToolsetRegistry, que vêm com o AllToolsets do Unreal
#    MCP. No -game o init_unreal.py deles falha com "AttributeError: module 'unreal' has no attribute 'AgentSkill'" (ou
#    'ToolsetDefinition', 'PythonTestRunner'): essas classes só existem com o servidor do editor. Não dá para tirar sem desligar o MCP
#    (decisão de 14 out 2026). Some o BLOCO de traceback inteiro só quando o primeiro arquivo dele é o init_unreal.py de um desses
#    plugins E a última linha é esse AttributeError.
$KnownToolsetAttributes = @('AgentSkill', 'ToolsetDefinition', 'PythonTestRunner')

function Remove-KnownToolsetTracebacks([string[]]$Lines) {
    $out = New-Object 'System.Collections.Generic.List[string]'
    $removed = 0
    $attr = "LogPython: Error: AttributeError: module 'unreal' has no attribute '(" + ($KnownToolsetAttributes -join '|') + ")'\s*$"
    $i = 0
    while ($i -lt $Lines.Count) {
        if ($Lines[$i] -notmatch 'LogPython: Error: Traceback \(most recent call last\):') { $out.Add($Lines[$i]); $i++; continue }
        $j = $i + 1
        while ($j -lt $Lines.Count -and $Lines[$j] -match 'LogPython: Error: ' -and $Lines[$j] -notmatch 'LogPython: Error: Traceback') { $j++ }
        $block = @($Lines[$i..($j - 1)])
        $firstFile = $block | Where-Object { $_ -match 'LogPython: Error:\s+File "' } | Select-Object -First 1
        if ($firstFile -match '/Engine/Plugins/Experimental/(Toolsets/[^/"]+|ToolsetRegistry)/Content/Python/init_unreal\.py"' -and $block[-1] -match $attr) { $removed++ }
        else { $out.AddRange([string[]]$block) }
        $i = $j
    }
    if ($removed -gt 0) { Write-Host "(filtro: $removed traceback(s) conhecido(s) dos Toolsets experimentais do MCP fora da lista de erros; ver Find-Unreal.ps1)" }
    $out.ToArray()  # sem a vírgula unária: as linhas precisam sair uma a uma para o Select-String
}

function Resolve-UnrealContext([string]$Uproject, [string]$EngineVersion) {
    if (-not (Test-Path -LiteralPath $Uproject -PathType Leaf)) { throw "Arquivo .uproject não existe: $Uproject" }
    $full = (Resolve-Path -LiteralPath $Uproject).Path
    if (-not $EngineVersion) { $EngineVersion = Get-ProjectEngineVersion $full }
    [pscustomobject]@{
        Uproject = $full
        Name     = [IO.Path]::GetFileNameWithoutExtension($full)
        Root     = Find-UnrealRoot $EngineVersion
    }
}
