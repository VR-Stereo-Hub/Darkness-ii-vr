# dumpbin.ps1 - locate the VS-bundled dumpbin and read a DLL's export table.
# Dot-sourced by exports-check.ps1.
# NOTE: keep this file pure ASCII (PowerShell 5.1 misreads BOM-less UTF-8).

function Get-D2Dumpbin {
    $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    if (-not (Test-Path $vswhere)) { throw "vswhere.exe not found - install VS 2022 Build Tools." }
    $vs = & $vswhere -latest -products * -property installationPath | Select-Object -First 1
    $d = Get-ChildItem "$vs\VC\Tools\MSVC\*\bin\Hostx64\x86\dumpbin.exe" -ErrorAction SilentlyContinue | Select-Object -First 1
    if (-not $d) { $d = Get-ChildItem "$vs\VC\Tools\MSVC\*\bin\Hostx86\x86\dumpbin.exe" -ErrorAction SilentlyContinue | Select-Object -First 1 }
    if (-not $d) { throw "dumpbin.exe not found under $vs - install the MSVC C++ build tools." }
    return $d.FullName
}

# Returns one "<ordinal> <name|NONAME>" line per export, the shape of
# tests\golden\d3d9-exports.txt.
function Get-D2Exports {
    param([Parameter(Mandatory)][string]$Dll)
    if (-not (Test-Path $Dll)) { throw "not found: $Dll" }
    $out = & (Get-D2Dumpbin) /exports $Dll
    $rows = @()
    $grab = $false
    foreach ($l in $out) {
        if ($l -match 'ordinal\s+hint') { $grab = $true; continue }
        if ($grab -and $l -match '^\s*Summary') { break }
        # named:   "   37   14 00001234 Direct3DCreate9"
        if ($grab -and $l -match '^\s+(\d+)\s+[0-9A-F]+\s+[0-9A-F]{8}\s+(\S+)') { $rows += "$($Matches[1]) $($Matches[2])"; continue }
        # NONAME:  "   16      00001234 [NONAME]"
        if ($grab -and $l -match '^\s+(\d+)\s+[0-9A-F]{8}\s+\[NONAME\]') { $rows += "$($Matches[1]) NONAME"; continue }
    }
    return $rows
}

# Returns the DLL names a module imports statically (its import directory), one
# per line, upper-cased and sorted: the shape of tests\golden\d3d9-imports.txt.
# The proxy must add no static import that changes the game's DLL load order,
# so this list is asserted after every build (exports-check.ps1).
function Get-D2Imports {
    param([Parameter(Mandatory)][string]$Dll)
    if (-not (Test-Path $Dll)) { throw "not found: $Dll" }
    $out = & (Get-D2Dumpbin) /imports $Dll
    $rows = @()
    $inImports = $false
    foreach ($l in $out) {
        if ($l -match 'Section contains the following imports') { $inImports = $true; continue }
        if ($l -match 'Section contains the following delay load imports') { $inImports = $false; continue }
        if ($inImports -and $l -match '^\s+([A-Za-z0-9_.-]+\.dll)\s*$') { $rows += $Matches[1].ToUpper(); continue }
    }
    return @($rows | Sort-Object -Unique)
}
