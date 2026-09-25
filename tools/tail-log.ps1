# tail-log.ps1 - follows darkness2_vr.log live (it sits next to DarknessII.exe).
#   .\tools\tail-log.ps1                     # last 50 lines, then follow
#   .\tools\tail-log.ps1 -Grep "route:|canary"   # only matching lines
#   .\tools\tail-log.ps1 -Since 500          # start further back
#   .\tools\tail-log.ps1 -Once               # print the tail and return (no follow)
# Read-only.
# NOTE: keep this file pure ASCII (PowerShell 5.1 misreads BOM-less UTF-8).
param(
    [string]$GamePath = "",
    [string]$Grep = "",
    [int]$Since = 50,
    [switch]$Once
)
. (Join-Path $PSScriptRoot "lib\game-path.ps1")
$log = Get-D2LogPath $GamePath
if (-not (Test-Path $log)) {
    Write-Host "No log yet at $log - launch the game with the mod installed first. Waiting for it..."
    while (-not (Test-Path $log)) { Start-Sleep -Milliseconds 500 }
}
if ($Once) {
    $lines = Get-Content $log -Tail $Since
    if ($Grep) { $lines | Where-Object { $_ -match $Grep } } else { $lines }
    return
}
if ($Grep) { Get-Content $log -Wait -Tail $Since | Where-Object { $_ -match $Grep } }
else { Get-Content $log -Wait -Tail $Since }
