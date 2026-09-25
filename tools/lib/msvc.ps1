# lib/msvc.ps1 - where the MSVC toolset and the VS-bundled CMake are on THIS
# machine. Dot-source it and call Get-D2MsvcRoot / Get-D2CMake.
# vswhere first (the VS 2022 Build Tools live under Program Files (x86)), the
# full-VS glob as the fallback, and a clear error naming both.
# NOTE: keep this file pure ASCII (PowerShell 5.1 misreads BOM-less UTF-8).

function Get-D2VsWhere {
    $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    if (-not (Test-Path $vswhere)) { throw "vswhere.exe not found - install VS 2022 Build Tools (C++ workload + CMake tools)." }
    return $vswhere
}

function Get-D2MsvcRoot {
    $vswhere = Get-D2VsWhere
    $root = $null
    $vs = & $vswhere -latest -products * -property installationPath | Select-Object -First 1
    if ($vs) {
        $root = (Get-ChildItem "$vs\VC\Tools\MSVC\*" -Directory -ErrorAction SilentlyContinue |
                 Sort-Object Name -Descending | Select-Object -First 1).FullName
    }
    if (-not $root) {
        $root = (Get-ChildItem "C:\Program Files\Microsoft Visual Studio\*\*\VC\Tools\MSVC\*" -Directory -ErrorAction SilentlyContinue |
                 Sort-Object Name -Descending | Select-Object -First 1).FullName
    }
    if (-not $root) { throw "MSVC toolset not found under vswhere's install or C:\Program Files\Microsoft Visual Studio. Install the VS 2022 Build Tools C++ workload." }
    return $root
}

function Get-D2CMake {
    $vswhere = Get-D2VsWhere
    $cmake = & $vswhere -latest -products * -find "Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" | Select-Object -First 1
    if (-not $cmake) { throw "VS-bundled CMake not found - install the 'C++ CMake tools for Windows' component." }
    return $cmake
}
