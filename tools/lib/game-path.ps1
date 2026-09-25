# game-path.ps1 - where The Darkness II, its config and the mod's files live.
# Dot-source this from every script that touches the game:
#   . (Join-Path $PSScriptRoot "lib\game-path.ps1")
#
# Resolution order for the game folder (the one holding DarknessII.exe):
#   1. $env:D2VR_GAME_DIR               (explicit; a second install would set it)
#   2. Steam's libraryfolders.vdf       (every library, checked for appmanifest_67370.acf)
#   3. throw                            (the game is not installed here)
# Never hardcode a drive letter (docs/darkness2/TESTING.md).
# NOTE: keep this file pure ASCII (PowerShell 5.1 misreads BOM-less UTF-8).

$script:D2AppId = "67370"
$script:D2Exe = "DarknessII.exe"
$script:D2Proc = "DarknessII"

function Get-D2GamePath {
    param([string]$Override = "")
    if ($Override) {
        if (Test-Path (Join-Path $Override $script:D2Exe)) { return $Override }
        throw "$script:D2Exe not found in '$Override'."
    }
    if ($env:D2VR_GAME_DIR) {
        if (Test-Path (Join-Path $env:D2VR_GAME_DIR $script:D2Exe)) { return $env:D2VR_GAME_DIR }
        throw "D2VR_GAME_DIR is set to '$($env:D2VR_GAME_DIR)' but $script:D2Exe is not there."
    }
    $steam = Get-D2SteamPath
    $vdf = Join-Path $steam "steamapps\libraryfolders.vdf"
    $libs = @($steam)
    if (Test-Path $vdf) {
        foreach ($line in Get-Content $vdf) {
            if ($line -match '^\s*"path"\s+"(.+)"') { $libs += $Matches[1].Replace('\\', '\') }
        }
    }
    foreach ($lib in $libs | Select-Object -Unique) {
        $acf = Join-Path $lib "steamapps\appmanifest_$script:D2AppId.acf"
        if (-not (Test-Path $acf)) { continue }
        $dir = Join-Path $lib "steamapps\common\Darkness II"
        if (Test-Path (Join-Path $dir $script:D2Exe)) { return $dir }
    }
    throw "The Darkness II (Steam app $script:D2AppId) is not installed in any Steam library. Set D2VR_GAME_DIR to the folder holding $script:D2Exe."
}

function Get-D2SteamPath {
    $steam = $null
    try { $steam = (Get-ItemProperty "HKCU:\Software\Valve\Steam" -ErrorAction Stop).SteamPath } catch {}
    if (-not $steam) { $steam = "C:\Program Files (x86)\Steam" }
    return $steam.Replace('/', '\')
}

# The game's own config: %APPDATA%\DarknessII (obfuscated; the mod never writes it).
function Get-D2GameConfigDir { return Join-Path $env:APPDATA "DarknessII" }

# Harness and bulk files (command.txt, ack.txt, status.json, dumps, shots, xrsim).
function Get-D2DataDir {
    if ($env:D2VR_DATA_DIR) { return $env:D2VR_DATA_DIR }
    return Join-Path $env:LOCALAPPDATA "Darkness2VR"
}

# User-facing files stay next to the exe (see src/core/util/paths.h).
function Get-D2LogPath { param([string]$GamePath = "") return Join-Path (Get-D2GamePath $GamePath) "darkness2_vr.log" }
function Get-D2IniPath { param([string]$GamePath = "") return Join-Path (Get-D2GamePath $GamePath) "darkness2_vr.ini" }
function Get-D2CrashPath { param([string]$GamePath = "") return Join-Path (Get-D2GamePath $GamePath) "darkness2_vr_crash.txt" }
function Get-D2CmdPath { return Join-Path (Get-D2DataDir) "command.txt" }
function Get-D2AckPath { return Join-Path (Get-D2DataDir) "ack.txt" }
function Get-D2StatusPath { return Join-Path (Get-D2DataDir) "status.json" }
function Get-D2ShotsDir { return Join-Path (Get-D2DataDir) "shots" }
function Get-D2DumpsDir { return Join-Path (Get-D2DataDir) "dumps" }

# The live game process, if any (drops exited entries; prefers one with a window).
function Get-D2Process {
    $p = @(Get-Process $script:D2Proc -ErrorAction SilentlyContinue | Where-Object { -not $_.HasExited })
    if ($p.Count -gt 1) {
        $withWindow = @($p | Where-Object { $_.MainWindowHandle -ne [IntPtr]::Zero })
        if ($withWindow.Count -ge 1) { $p = $withWindow }
        $p = @($p | Sort-Object Id -Descending)
    }
    return ($p | Select-Object -First 1)
}

# Refuse a 64-bit DLL before it is copied next to a 32-bit game and silently ignored.
function Assert-D2X86Dll {
    param([Parameter(Mandatory)][string]$Path)
    $fs = [System.IO.File]::OpenRead($Path)
    try {
        $br = New-Object System.IO.BinaryReader($fs)
        $fs.Seek(0x3C, 'Begin') | Out-Null
        $peOffset = $br.ReadInt32()
        $fs.Seek($peOffset, 'Begin') | Out-Null
        if ($br.ReadUInt32() -ne 0x00004550) { throw "$Path is not a PE image." }
        $machine = $br.ReadUInt16()
    } finally { $fs.Close() }
    if ($machine -ne 0x014C) { throw ("$Path reports PE machine 0x{0:X4}, not 0x014C (x86)." -f $machine) }
}

# The build id stamped into a built d3d9.dll (the banner's second field), read
# from the DLL's own bytes so an installed DLL can be matched to a log.
function Get-D2DllBuildId {
    param([Parameter(Mandatory)][string]$Path)
    $text = [System.Text.Encoding]::ASCII.GetString([System.IO.File]::ReadAllBytes($Path))
    if ($text -match 'Darkness II VR proxy loaded \(darkness2vr ') { return "present" }
    return ""
}
