# Builds the mod (32-bit): d3d9.dll (the proxy + core + the Darkness II layer),
# the simulated OpenXR runtime and the xr_hello32 smoke client. CMake is the one
# bundled with VS 2022 Build Tools, located via vswhere (nothing on PATH).
#   .\tools\build.ps1              # Debug
#   .\tools\build.ps1 -Release     # RelWithDebInfo (the build to play and measure)
#   .\tools\build.ps1 -Release -Install
# The 32-bit guard lives in CMakeLists.txt: a 64-bit configure is refused there.
# NOTE: keep this file pure ASCII (PowerShell 5.1 misreads BOM-less UTF-8).
param(
    [switch]$Release,
    [switch]$Install,
    [switch]$Legacy,      # also compile src/legacy (retired experiments)
    [string]$GamePath = ""
)

$ErrorActionPreference = "Stop"
$repo = Split-Path -Parent $PSScriptRoot
. (Join-Path $PSScriptRoot "lib\msvc.ps1")
$cmake = Get-D2CMake

Push-Location $repo
try {
    # The legacy switch is decided by THIS call and by nothing else: read what
    # the cache holds and reconfigure whenever it is not what was asked for.
    $legacyFlag = if ($Legacy) { "ON" } else { "OFF" }
    if (-not (Test-Path "build\CMakeCache.txt")) {
        & $cmake --preset win32 "-DD2VR_WITH_LEGACY=$legacyFlag"
        if ($LASTEXITCODE -ne 0) { throw "CMake configure failed." }
    } else {
        $cached = (Select-String -Path "build\CMakeCache.txt" -Pattern '^D2VR_WITH_LEGACY:BOOL=(\w+)' | Select-Object -First 1)
        $cachedFlag = if ($cached) { $cached.Matches[0].Groups[1].Value.ToUpper() } else { "" }
        if ($cachedFlag -ne $legacyFlag) {
            Write-Host "build: the CMake cache holds D2VR_WITH_LEGACY=$cachedFlag and this build asks for $legacyFlag - reconfiguring" -ForegroundColor Yellow
            & $cmake -S . -B build "-DD2VR_WITH_LEGACY=$legacyFlag"
            if ($LASTEXITCODE -ne 0) { throw "CMake reconfigure failed." }
        }
    }
    $preset = if ($Release) { "release" } else { "debug" }
    & $cmake --build --preset $preset
    if ($LASTEXITCODE -ne 0) { throw "Build failed." }
    $cfg = if ($Release) { "RelWithDebInfo" } else { "Debug" }
    $dll = Join-Path $repo "build\src\$cfg\d3d9.dll"
    if (Test-Path $dll) {
        Write-Host ("build: {0}  {1}  sha256 {2}" -f $dll, (Get-Item $dll).LastWriteTime, (Get-FileHash $dll -Algorithm SHA256).Hash.Substring(0, 16))
    }
}
finally {
    Pop-Location
}

if ($Install) {
    & (Join-Path $PSScriptRoot "install.ps1") -Release:$Release -GamePath $GamePath
}
