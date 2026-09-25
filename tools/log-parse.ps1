# log-parse.ps1 - summarise a darkness2_vr.log: the banner, the route lines
# (R0), the create call and device parameters, the fingerprint, hooks installed
# or refused, the canary lines (R1), every WARN/ERROR line, and the tail.
#   .\tools\log-parse.ps1                     # the installed game's log
#   .\tools\log-parse.ps1 -Path some.log      # an archived or reported log
#   .\tools\log-parse.ps1 -Canaries           # only the canary summary
# NOTE: keep this file pure ASCII (PowerShell 5.1 misreads BOM-less UTF-8).
param([string]$Path = "", [int]$Tail = 20, [switch]$Canaries)
. (Join-Path $PSScriptRoot "lib\game-path.ps1")
if (-not $Path) { $Path = Get-D2LogPath }
if (-not (Test-Path $Path)) { throw "no log at $Path" }
$lines = Get-Content $Path
"file: $Path ($($lines.Count) lines)"
function Section($title, $pattern, $max = 12) {
    $hits = @($lines | Where-Object { $_ -match $pattern })
    if ($hits.Count -eq 0) { return }
    ""
    "--- $title ($($hits.Count))"
    $hits | Select-Object -First $max
    if ($hits.Count -gt $max) { "    ... $($hits.Count - $max) more" }
}
if (-not $Canaries) {
    Section "banner"       "proxy loaded|proxy unloading|DISABLED" 4
    Section "route (R0)"   "route: (our module|host exe|DllDirectory|loaded from|DllMain on)|census\(" 12
    Section "create"       "Direct3DCreate9|d3d9 backend|interface hooks|device hooks|first Present" 12
    Section "device"       "CreateDevice(Ex)?: |Reset" 10
    Section "fingerprint"  "fingerprint:|EnableDirect3D9Ex byte" 4
    Section "callers"      "called from exe" 8
    Section "config"       "config: " 6
    Section "hooks"        "INSTALLED|REWRITTEN|REFUS|removed|restored" 16
}
Section "canaries (R1)" "canary" 400
if (-not $Canaries) {
    Section "warnings"     "\] \[W\]|\] \[E\]|EXCEPTION|FAILED" 40
    Section "exit"         "quit|teardown|unloading|minidump|crash test" 10
    ""
    "--- tail ($Tail)"
    $lines | Select-Object -Last $Tail
}
