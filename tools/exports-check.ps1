# exports-check.ps1 - the proxy's export table must be EXACTLY the system
# d3d9.dll's 23 entries: the same ordinals, undecorated names, NONAME for the six
# ordinal-only ones. MSVC decorates __stdcall exports as _Name@N unless
# src\proxy\d3d9.def names them; a wrong table means a missing-import failure
# for whoever imports by name, and a wrong ordinal breaks anyone importing by ordinal.
#   .\tools\exports-check.ps1 build\src\RelWithDebInfo\d3d9.dll
# The static IMPORT list is checked the same way against tests\golden\d3d9-imports.txt:
# the proxy must add no import that changes the game's DLL load order (every DLL
# in the golden is one DarknessII.exe imports itself).
# Exit 0 = both tables match their goldens; 1 otherwise.
# NOTE: keep this file pure ASCII (PowerShell 5.1 misreads BOM-less UTF-8).
param([Parameter(Mandatory)][string]$Dll)
$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
. (Join-Path $PSScriptRoot "lib\dumpbin.ps1")

$want = @(Get-Content (Join-Path $repo "tests\golden\d3d9-exports.txt") | Where-Object { $_ -and -not $_.StartsWith("#") } | ForEach-Object { $_.Trim() } | Sort-Object)
$have = @(Get-D2Exports $Dll | Sort-Object)
$missing = @($want | Where-Object { $have -notcontains $_ })
$extra = @($have | Where-Object { $want -notcontains $_ })
$decorated = @($have | Where-Object { $_ -match '\s_.*@\d+$' })
$ok = $true
if ($missing.Count -eq 0 -and $extra.Count -eq 0 -and $decorated.Count -eq 0) {
    "exports OK: $($have.Count) entries, ordinals and names match the system d3d9 table ($Dll)"
} else {
    $ok = $false
    if ($missing) { "MISSING: $($missing -join ', ')" }
    if ($extra) { "EXTRA: $($extra -join ', ')" }
    if ($decorated) { "DECORATED (the .def did not apply): $($decorated -join ', ')" }
}

$wantImp = @(Get-Content (Join-Path $repo "tests\golden\d3d9-imports.txt") | Where-Object { $_ -and -not $_.StartsWith("#") } | ForEach-Object { $_.Trim().ToUpper() } | Sort-Object)
$haveImp = @(Get-D2Imports $Dll)
$extraImp = @($haveImp | Where-Object { $wantImp -notcontains $_ })
$goneImp = @($wantImp | Where-Object { $haveImp -notcontains $_ })
if ($extraImp.Count -eq 0 -and $goneImp.Count -eq 0) {
    "imports OK: $($haveImp -join ', ') (all static imports of DarknessII.exe itself)"
} else {
    $ok = $false
    if ($extraImp) { "IMPORT NOT IN THE GOLDEN: $($extraImp -join ', ') - a new static import can change the game's DLL load order; LoadLibrary it, or argue the change in the PR and update tests\golden\d3d9-imports.txt" }
    if ($goneImp) { "IMPORT GONE: $($goneImp -join ', ') (update the golden if intended)" }
}
if ($ok) { exit 0 } else { exit 1 }
