# game-cmd.ps1 - write one or more commands to the mod's command seam
# (%LOCALAPPDATA%\Darkness2VR\command.txt, polled at 1 Hz from the Present
# hook; see src/core/framework/command.h for the vocabulary), then wait for the
# acknowledgement in ack.txt so the caller knows the batch landed.
#
# Uses WriteAllText, never Set-Content -Encoding utf8, whose BOM would corrupt
# the first token. Retries past the transient share-lock while the mod has the
# file open. Two quick writes would overwrite each other: one slot, 1 Hz, so
# this script waits for the ack before returning (-NoWait to skip).
#
# Usage: .\tools\game-cmd.ps1 "status"
#        .\tools\game-cmd.ps1 "key enter" "shot menu"
# NOTE: keep this file pure ASCII (PowerShell 5.1 misreads BOM-less UTF-8).
[CmdletBinding(PositionalBinding=$false)]
param(
    [switch]$NoWait,
    [int]$TimeoutSec = 6,
    [Parameter(ValueFromRemainingArguments=$true)][string[]]$Lines
)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot "lib\game-path.ps1")
if (-not $Lines -or $Lines.Count -eq 0) { throw "give at least one command line" }
$cmd = Get-D2CmdPath
$ack = Get-D2AckPath
$cmdDir = Split-Path -Parent $cmd
if (-not (Test-Path $cmdDir)) { New-Item -ItemType Directory -Force $cmdDir | Out-Null }
$before = if (Test-Path $ack) { (Get-Item $ack).LastWriteTimeUtc } else { [datetime]::MinValue }
$text = ($Lines -join "`n")
$written = $false
for ($i = 0; $i -lt 30; $i++) {
    try {
        [System.IO.File]::WriteAllText($cmd, $text + "`n")
        $written = $true
        break
    } catch { Start-Sleep -Milliseconds 200 }
}
if (-not $written) { throw "could not write command.txt after retries" }
if ($NoWait) { "wrote $($Lines.Count) command(s) to $cmd"; return }
$deadline = (Get-Date).AddSeconds($TimeoutSec)
while ((Get-Date) -lt $deadline) {
    if ((Test-Path $ack) -and (Get-Item $ack).LastWriteTimeUtc -gt $before) {
        "acked: $((Get-Content $ack -TotalCount 1)) - $($Lines -join '; ')"
        return
    }
    Start-Sleep -Milliseconds 150
}
throw "no ack within $TimeoutSec s for: $($Lines -join '; ') (is the game presenting? is the mod loaded?)"
