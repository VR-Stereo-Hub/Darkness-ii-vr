# uninstall.ps1 - removes the mod module from the game folder; restores a
# backed-up d3d9.dll if one exists. Touches only files we put there; leaves
# darkness2_vr.ini and the logs in place.
# NOTE: keep this file pure ASCII (PowerShell 5.1 misreads BOM-less UTF-8).
param([string]$GamePath = "")

$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot "lib\game-path.ps1")
$GamePath = Get-D2GamePath $GamePath
if (Get-D2Process) { throw "REFUSING: $script:D2Proc is running." }

$proxy = Join-Path $GamePath "d3d9.dll"
$backup = Join-Path $GamePath "d3d9.dll.d2vr-backup"
if (Test-Path $proxy) {
    if (Get-D2DllBuildId $proxy) { Remove-Item $proxy -Force; Write-Host "Removed d3d9.dll (our proxy)" }
    else { Write-Host "d3d9.dll present but it is not our proxy - refusing to delete it." }
}
if (Test-Path $backup) { Move-Item $backup $proxy -Force; Write-Host "Restored the original d3d9.dll from backup" }
Write-Host "Done. darkness2_vr.ini and the logs were left in place."
