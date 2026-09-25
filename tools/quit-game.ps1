# quit-game.ps1 - close the game cleanly: the `quit` seam word posts WM_CLOSE to
# the game window; if the process is still alive after the timeout, Stop-Process.
# The mod notes the teardown so faults on the exit path are one line, no dump.
#   .\tools\quit-game.ps1
#   .\tools\quit-game.ps1 -TimeoutSec 20
# NOTE: keep this file pure ASCII (PowerShell 5.1 misreads BOM-less UTF-8).
param([int]$TimeoutSec = 15, [switch]$Force)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot "lib\game-path.ps1")
$p = Get-D2Process
if (-not $p) { Write-Output "$script:D2Proc is not running."; return }
if (-not $Force) {
    try { & (Join-Path $PSScriptRoot "game-cmd.ps1") -TimeoutSec 5 "quit" | Out-Null } catch { Write-Output "quit: no ack ($($_.Exception.Message)); falling back to Stop-Process" }
    $deadline = (Get-Date).AddSeconds($TimeoutSec)
    while ((Get-Date) -lt $deadline) {
        if (-not (Get-D2Process)) { Write-Output "$script:D2Proc exited cleanly after WM_CLOSE."; return }
        Start-Sleep -Milliseconds 500
    }
    Write-Output "$script:D2Proc still alive after $TimeoutSec s - Stop-Process"
}
$p = Get-D2Process
if ($p) { Stop-Process -Id $p.Id -Force; Start-Sleep -Seconds 1; Write-Output "$script:D2Proc stopped (pid $($p.Id))." }
