# launch-game.ps1 - launch The Darkness II through Steam, with the checks that
# keep a test run honest. USE THIS instead of a bare Start-Process steam://.
#
# Every guard throws:
#   1. The game is already running.
#   2. Steam is not running: it is started and waited for (the exe delay-loads
#      Tools\steam_api.dll and expects the client). A login or Steam Guard
#      prompt is the user's click; this script says so and waits.
#   3. A stale command.txt is cleared (the mod ignores one older than its
#      process anyway, but the evidence should be clean).
# Before launching, the previous log is archived to build\logs\<stamp>\ so
# rotation is not the only copy of the evidence.
#   .\tools\launch-game.ps1                 # launch, wait for the window
#   .\tools\launch-game.ps1 -WaitBanner     # also wait for the mod's banner in a NEW log
#   .\tools\launch-game.ps1 -PreflightOnly  # guards only
# NOTE: keep this file pure ASCII (PowerShell 5.1 misreads BOM-less UTF-8).
[CmdletBinding()]
param(
    [switch]$PreflightOnly,
    [switch]$WaitBanner,
    [int]$WaitSeconds = 90,
    [int]$SteamWaitSeconds = 120,
    [string]$GamePath = ""
)

$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot "lib\game-path.ps1")
$repo = Split-Path -Parent $PSScriptRoot
$appId = $script:D2AppId
$gameDir = Get-D2GamePath $GamePath
$log = Get-D2LogPath $GamePath

# --- guard 1: already running ------------------------------------------------
if (Get-D2Process) {
    if ($PreflightOnly) { throw "REFUSING: $script:D2Proc is already running." }
    Write-Output "$script:D2Proc is already running (pid $((Get-D2Process).Id)) - nothing to do."
    return
}

# --- guard 2: stale command file ---------------------------------------------
$cmd = Get-D2CmdPath
if (Test-Path $cmd) {
    $raw = Get-Content $cmd -Raw
    $stale = if ($null -eq $raw) { "" } else { $raw.Trim() }
    Remove-Item $cmd -Force
    if ($stale) { Write-Output "cleared a stale command.txt: $stale" }
}

if ($PreflightOnly) { Write-Output "preflight ok - guards passed, command.txt clear, nothing launched"; return }

# --- archive the previous log ------------------------------------------------
if (Test-Path $log) {
    $stamp = Get-Date -Format "yyyyMMdd-HHmmss"
    $dest = Join-Path $repo "build\logs\$stamp"
    New-Item -ItemType Directory -Force $dest | Out-Null
    Copy-Item $log (Join-Path $dest "darkness2_vr.log") -Force
    $crash = Get-D2CrashPath $GamePath
    if (Test-Path $crash) { Copy-Item $crash $dest -Force }
    $status = Get-D2StatusPath
    if (Test-Path $status) { Copy-Item $status $dest -Force }
    Write-Output "archived the previous log to $dest"
}
$logBefore = if (Test-Path $log) { (Get-Item $log).LastWriteTimeUtc } else { [datetime]::MinValue }

# --- Steam -------------------------------------------------------------------
if (-not (Get-Process steam -ErrorAction SilentlyContinue)) {
    $steamExe = Join-Path (Get-D2SteamPath) "steam.exe"
    Write-Output "Steam is not running - starting $steamExe (a login or Steam Guard prompt, if any, is yours to answer)"
    Start-Process $steamExe
    $deadline = (Get-Date).AddSeconds($SteamWaitSeconds)
    while ((Get-Date) -lt $deadline) {
        Start-Sleep -Seconds 2
        $s = Get-Process steam -ErrorAction SilentlyContinue
        if ($s -and $s.MainWindowHandle -ne 0) { break }
    }
    if (-not (Get-Process steam -ErrorAction SilentlyContinue)) { throw "Steam did not start within $SteamWaitSeconds s." }
    Start-Sleep -Seconds 5   # let the client finish logging in before it is asked to run a game
}

# --- launch ------------------------------------------------------------------
Write-Output "launching The Darkness II (appid $appId) from $gameDir ..."
Start-Process "steam://rungameid/$appId"

$p = $null
for ($i = 0; $i -lt $WaitSeconds; $i++) {
    Start-Sleep -Seconds 1
    $p = Get-D2Process
    if ($p -and $p.MainWindowHandle -ne 0) { break }
}
if (-not $p) { throw "$script:D2Proc did not appear within $WaitSeconds s." }
Write-Output "$script:D2Proc up (pid $($p.Id), window $($p.MainWindowHandle)). Log: $log"

if ($WaitBanner) {
    $deadline = (Get-Date).AddSeconds(60)
    while ((Get-Date) -lt $deadline) {
        if ((Test-Path $log) -and (Get-Item $log).LastWriteTimeUtc -gt $logBefore) {
            $banner = Select-String -Path $log -Pattern 'Darkness II VR proxy loaded' | Select-Object -First 1
            if ($banner) { Write-Output $banner.Line.Trim(); return }
        }
        Start-Sleep -Milliseconds 500
    }
    throw "no new banner in $log within 60 s - the module did not load, or the log went elsewhere"
}
