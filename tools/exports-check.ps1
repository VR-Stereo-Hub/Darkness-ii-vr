# exports-check.ps1 - the proxy's export table must be EXACTLY the system
# d3d9.dll's 23 entries: the same ordinals, undecorated names, NONAME for the six
# ordinal-only ones. MSVC decorates __stdcall exports as _Name@N unless
# src\proxy\d3d9.def names them; a wrong table means a missing-import failure
# for whoever imports by name, and a wrong ordinal breaks anyone importing by ordinal.
#   .\tools\exports-check.ps1 build\src\RelWithDebInfo\d3d9.dll
# Exit 0 = exact match with tests\golden\d3d9-exports.txt; 1 otherwise.
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
if ($missing.Count -eq 0 -and $extra.Count -eq 0 -and $decorated.Count -eq 0) {
    "exports OK: $($have.Count) entries, ordinals and names match the system d3d9 table ($Dll)"
    exit 0
}
if ($missing) { "MISSING: $($missing -join ', ')" }
if ($extra) { "EXTRA: $($extra -join ', ')" }
if ($decorated) { "DECORATED (the .def did not apply): $($decorated -join ', ')" }
exit 1
