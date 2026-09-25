# status-dump.ps1 - ask the running mod for its status and print it.
# Sends `status` through the command seam, waits for the acknowledgement, then
# pretty-prints %LOCALAPPDATA%\Darkness2VR\status.json.
#   .\tools\status-dump.ps1            # full JSON
#   .\tools\status-dump.ps1 -Raw       # the file as written
#   .\tools\status-dump.ps1 -NoAsk     # just read the file the 1 Hz writer left
# NOTE: keep this file pure ASCII (PowerShell 5.1 misreads BOM-less UTF-8).
param([switch]$Raw, [switch]$NoAsk, [int]$WaitSeconds = 6)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot "lib\game-path.ps1")
$status = Get-D2StatusPath
if (-not $NoAsk) { & (Join-Path $PSScriptRoot "game-cmd.ps1") -TimeoutSec $WaitSeconds "status" | Out-Null }
if (-not (Test-Path $status)) { throw "no status.json at $status - is the game running with the mod?" }
if ($Raw) { Get-Content $status -Raw; return }
$s = Get-Content $status -Raw | ConvertFrom-Json
$s | ConvertTo-Json -Depth 6
