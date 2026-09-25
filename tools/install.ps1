# install.ps1 - copies the built mod module (d3d9.dll) next to DarknessII.exe.
#
# Refuses while the game runs (the DLL is mapped; a half-copied proxy is worse
# than no install). Backs up a pre-existing d3d9.dll that is not ours exactly
# once. Writes the default ini when none exists, then DIFFS THE FULL INSTALLED
# INI against tests\golden\darkness2_vr.ini (the defaults) and against the ini
# archived by the previous install, so every run's settings are on the record
# before the launch. "zero-setting diff" is a result and is printed as such.
#   .\tools\install.ps1 -Release
#   .\tools\install.ps1 -Release -Set "Canary.Cold=1","Canary.Tick=1"   # flip ini keys for this run
# NOTE: keep this file pure ASCII (PowerShell 5.1 misreads BOM-less UTF-8).
param(
    [switch]$Release,
    [string]$GamePath = "",
    [string[]]$Set = @(),      # Section.Key=Value edits applied to the installed ini
    [switch]$ResetIni          # replace the installed ini with the golden defaults first
)

$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot "lib\game-path.ps1")
$repo = Split-Path -Parent $PSScriptRoot
$config = if ($Release) { "RelWithDebInfo" } else { "Debug" }
$outDir = Join-Path $repo "build\src\$config"
$GamePath = Get-D2GamePath $GamePath

if (Get-D2Process) { throw "REFUSING to install: $script:D2Proc is running. Quit it first (tools\quit-game.ps1)." }

$proxy = Join-Path $outDir "d3d9.dll"
if (-not (Test-Path $proxy)) { throw "Build output missing: $proxy - run tools\build.ps1 first." }
Assert-D2X86Dll $proxy

# Back up a foreign d3d9.dll exactly once (ours is recognised by the banner string it carries).
$existing = Join-Path $GamePath "d3d9.dll"
$backup = Join-Path $GamePath "d3d9.dll.d2vr-backup"
if ((Test-Path $existing) -and -not (Test-Path $backup)) {
    if (-not (Get-D2DllBuildId $existing)) {
        Copy-Item $existing $backup
        Write-Host "Backed up a foreign d3d9.dll -> d3d9.dll.d2vr-backup"
    }
}

Copy-Item $proxy $existing -Force

# --- the ini: golden defaults, the archived previous, the edits, the diff -----
$ini = Get-D2IniPath $GamePath
$golden = Join-Path $repo "tests\golden\darkness2_vr.ini"
$archiveDir = Join-Path $repo "build\ini-archive"
if (-not (Test-Path $archiveDir)) { New-Item -ItemType Directory -Force $archiveDir | Out-Null }
$previous = Join-Path $archiveDir "previous.ini"

function Read-IniSettings([string]$path) {
    $map = @{}
    if (-not (Test-Path $path)) { return $map }
    $section = ""
    foreach ($line in Get-Content $path) {
        $t = $line.Trim()
        if (-not $t -or $t.StartsWith(";") -or $t.StartsWith("#")) { continue }
        if ($t -match '^\[(.+)\]$') { $section = $Matches[1]; continue }
        if ($t -match '^([^=]+)=(.*)$') { $map["$section.$($Matches[1].Trim())"] = $Matches[2].Trim() }
    }
    return $map
}
function Diff-Ini([hashtable]$a, [hashtable]$b, [string]$labelA, [string]$labelB) {
    $lines = @()
    foreach ($k in ($a.Keys + $b.Keys | Sort-Object -Unique)) {
        $va = if ($a.ContainsKey($k)) { $a[$k] } else { "<absent>" }
        $vb = if ($b.ContainsKey($k)) { $b[$k] } else { "<absent>" }
        if ($va -ne $vb) { $lines += ("  {0,-28} {1}: {2,-12} {3}: {4}" -f $k, $labelA, $va, $labelB, $vb) }
    }
    return $lines
}

if ($ResetIni -or -not (Test-Path $ini)) {
    if (Test-Path $golden) { Copy-Item $golden $ini -Force; Write-Host "ini: wrote the golden defaults to $ini" }
    else { Write-Host "ini: no golden at $golden; the mod writes its default ini on first launch" }
}
foreach ($edit in $Set) {
    if ($edit -notmatch '^([^.]+)\.([^=]+)=(.*)$') { throw "-Set entries look like Section.Key=Value, not '$edit'" }
    $sec, $key, $val = $Matches[1], $Matches[2], $Matches[3]
    $text = if (Test-Path $ini) { Get-Content $ini -Raw } else { "" }
    if ($text -match "(?ms)^\[$([regex]::Escape($sec))\]") {
        $pattern = "(?m)^(\[$([regex]::Escape($sec))\](?:\r?\n(?!\[).*)*?\r?\n)$([regex]::Escape($key))=.*$"
        if ($text -match "(?m)^\[$([regex]::Escape($sec))\][\s\S]*?^$([regex]::Escape($key))=") {
            $text = [regex]::Replace($text, "(?m)(^\[$([regex]::Escape($sec))\][\s\S]*?^)$([regex]::Escape($key))=[^\r\n]*", ('${1}' + "$key=$val"), 1)
        } else {
            $text = [regex]::Replace($text, "(?m)^\[$([regex]::Escape($sec))\][^\r\n]*", ('$0' + "`r`n$key=$val"), 1)
        }
    } else {
        $text = $text.TrimEnd() + "`r`n`r`n[$sec]`r`n$key=$val`r`n"
    }
    [System.IO.File]::WriteAllText($ini, $text)
    Write-Host "ini: set [$sec] $key=$val"
}

$now = Read-IniSettings $ini
$gold = Read-IniSettings $golden
$prev = Read-IniSettings $previous
Write-Host "ini: $ini ($($now.Count) settings)"
$d1 = Diff-Ini $gold $now "golden" "installed"
if ($d1.Count -eq 0) { Write-Host "ini vs golden defaults: zero-setting diff" } else { Write-Host "ini vs golden defaults: $($d1.Count) setting(s) differ"; $d1 | ForEach-Object { Write-Host $_ } }
if (Test-Path $previous) {
    $d2 = Diff-Ini $prev $now "previous" "installed"
    if ($d2.Count -eq 0) { Write-Host "ini vs previous install: zero-setting diff" } else { Write-Host "ini vs previous install: $($d2.Count) setting(s) changed"; $d2 | ForEach-Object { Write-Host $_ } }
} else { Write-Host "ini vs previous install: no previous archived ini" }
if (Test-Path $ini) { Copy-Item $ini $previous -Force }

Write-Host "Installed $config build to $GamePath"
Write-Host ("  d3d9.dll  {0}  sha256 {1}" -f (Get-Item $proxy).LastWriteTime, (Get-FileHash $proxy -Algorithm SHA256).Hash.Substring(0, 16))
if (-not $Release) {
    Write-Host ""
    Write-Host "  *** This is the UNOPTIMISED Debug build (/Od, runtime checks, the debug CRT). ***" -ForegroundColor Yellow
    Write-Host "  *** Fine for the harness. NOT for any timing number: build.ps1 -Release.       ***" -ForegroundColor Yellow
    Write-Host ""
}
Write-Host "Log: $(Get-D2LogPath $GamePath)   harness files: $(Get-D2DataDir)"
