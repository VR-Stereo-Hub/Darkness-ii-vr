# game-shot.ps1 - a screenshot of the game's D3D9 backbuffer through the mod's
# `shot` seam word (a BMP under %LOCALAPPDATA%\Darkness2VR\shots), so the
# harness can SEE the game even in exclusive fullscreen where PrintWindow is
# black. Prints the new file's path; -Out copies it somewhere (BMPs are
# game-derived content and never enter the repo).
#   .\tools\game-shot.ps1                 # shots\shot_NNN.bmp
#   .\tools\game-shot.ps1 -Tag menu       # shots\menu_NNN.bmp
#   .\tools\game-shot.ps1 -Tag menu -Out C:\tmp\menu.bmp
# NOTE: keep this file pure ASCII (PowerShell 5.1 misreads BOM-less UTF-8).
param([string]$Tag = "shot", [string]$Out = "", [int]$WaitSeconds = 8)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot "lib\game-path.ps1")
$dir = Get-D2ShotsDir
$before = @{}
if (Test-Path $dir) { Get-ChildItem $dir -Filter *.bmp | ForEach-Object { $before[$_.Name] = $true } }
& (Join-Path $PSScriptRoot "game-cmd.ps1") -TimeoutSec $WaitSeconds "shot $Tag" | Out-Null
$deadline = (Get-Date).AddSeconds($WaitSeconds)
while ((Get-Date) -lt $deadline) {
    if (Test-Path $dir) {
        $new = @(Get-ChildItem $dir -Filter "$Tag`_*.bmp" | Where-Object { -not $before.ContainsKey($_.Name) } | Sort-Object LastWriteTime -Descending)
        if ($new.Count -gt 0 -and $new[0].Length -gt 1024) {
            Start-Sleep -Milliseconds 200   # let the writer close the file
            if ($Out) { Copy-Item $new[0].FullName $Out -Force; Write-Output $Out } else { Write-Output $new[0].FullName }
            return
        }
    }
    Start-Sleep -Milliseconds 200
}
throw "no new shot appeared under $dir within $WaitSeconds s (check the log for 'shot:' lines)"
