# module-census.ps1 - the modules of the RUNNING game, from outside the process:
# the external corroboration of R0's route (which d3d9.dll is mapped, from
# where, and whether the system one sits beside it as our backend), and of the
# Steam overlay being present. Read-only.
#
# A 64-bit PowerShell cannot enumerate a 32-bit process's modules (it sees only
# the WOW64 shims; measured 2026-09-25: 7 of 60), so this script re-runs itself
# under the 32-bit PowerShell when needed.
#   .\tools\module-census.ps1              # the interesting ones
#   .\tools\module-census.ps1 -All         # every module
# NOTE: keep this file pure ASCII (PowerShell 5.1 misreads BOM-less UTF-8).
param([switch]$All)
$ErrorActionPreference = 'Stop'
if ([Environment]::Is64BitProcess) {
    $ps32 = Join-Path $env:WINDIR "SysWOW64\WindowsPowerShell\v1.0\powershell.exe"
    if (-not (Test-Path $ps32)) { throw "32-bit PowerShell not found at $ps32" }
    $args32 = @("-NoProfile", "-ExecutionPolicy", "Bypass", "-File", $MyInvocation.MyCommand.Path)
    if ($All) { $args32 += "-All" }
    & $ps32 @args32
    exit $LASTEXITCODE
}
. (Join-Path $PSScriptRoot "lib\game-path.ps1")
$p = Get-D2Process
if (-not $p) { throw "$script:D2Proc is not running" }
$mods = @($p.Modules)
"pid $($p.Id): $($mods.Count) modules (enumerated from a 32-bit PowerShell)"
$want = 'd3d9|GameOverlayRenderer|steam_api|steamclient|DarknessII|BinkW32|PhysX|xinput|dinput8|nvapi|dbghelp|X3DAudio'
foreach ($m in $mods) {
    if ($All -or $m.ModuleName -match $want) {
        "  {0,-28} 0x{1:X8}  {2}" -f $m.ModuleName, $m.BaseAddress.ToInt64(), $m.FileName
    }
}
$d3d9 = @($mods | Where-Object { $_.ModuleName -ieq 'd3d9.dll' })
""
"d3d9.dll instances: $($d3d9.Count)"
foreach ($m in $d3d9) { "  " + $m.FileName }
$gameDir = Get-D2GamePath
$ours = @($d3d9 | Where-Object { $_.FileName -like "$gameDir*" })
$sys = @($d3d9 | Where-Object { $_.FileName -notlike "$gameDir*" })
if ($ours.Count -ge 1) { "VERDICT: the app-dir d3d9.dll IS mapped in the game (route (a))$(if ($sys.Count -ge 1) { ', with the system d3d9.dll beside it as the backend' })" }
else { "VERDICT: no app-dir d3d9.dll in the process - the proxy was NOT chosen" }
if ($mods | Where-Object { $_.ModuleName -ieq 'GameOverlayRenderer.dll' }) { "Steam overlay: GameOverlayRenderer.dll is loaded" } else { "Steam overlay: GameOverlayRenderer.dll is NOT loaded" }
