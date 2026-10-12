# Funções compartilhadas por build.ps1 e test.ps1. Não é para rodar sozinho.
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
