# xrsim-soak.ps1 - the S0.5 instrument: both simulator eyes receive the game's
# frame at the game's own rate for N minutes (docs/ROADMAP.md S0.5, VR-241).
#
# What it measures, every -SampleSeconds, from the two files the run already
# writes (nothing here touches the game):
#   the MOD's status.json   frame.presents, xr.submits, stereo.framesOut, frame.hz
#   the SIM's state.json    endFrames, layeredFrames, sessionState
# and every -ShotEveryMinutes a per-eye capture (xrsim-shot.ps1), asserting both
# eyes non-black. The verdict at the end:
#   PASS when  submits/presents >= -MinRatio over the whole window (every present
#              handed the runtime a texture), the sim's layeredFrames grew by the
#              same count, every capture had both eyes >= -MinNonBlack percent
#              non-black, the session stayed FOCUSED, and the mod log written
#              during the window has no SUBMISSION IDLE, EXCEPTION, POISONED or
#              canary REVERTED line.
# The game must already be running on the simulator (xrsim-launch.ps1 -ViaSteam)
# and be in GAMEPLAY (boot.ps1): a menu or a load screen is a legitimate black.
#   .\tools\xrsim-soak.ps1 -Minutes 5
# Exit 0 PASS, 1 FAIL. NOTE: keep this file pure ASCII (PowerShell 5.1).
[CmdletBinding()]
param(
    [int]$Minutes = 5,
    [int]$SampleSeconds = 10,
    [int]$ShotEveryMinutes = 1,
    [double]$MinRatio = 0.98,
    [double]$MinNonBlack = 10,
    [string]$Dir = "",
    [string]$GamePath = "",
    [string]$OutDir = ""
)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot "lib\game-path.ps1")
if (-not $Dir) { $Dir = Join-Path (Get-D2DataDir) "xrsim" }
$stateScript = Join-Path $PSScriptRoot "xrsim-state.ps1"
$shotScript  = Join-Path $PSScriptRoot "xrsim-shot.ps1"
$statusPath  = Get-D2StatusPath
$modLog      = Get-D2LogPath $GamePath
if (-not (Test-Path $statusPath)) { throw "no status.json at $statusPath - is the game running with the mod?" }
if (-not $OutDir) { $OutDir = Join-Path (Get-D2DataDir) "soak" }
if (-not (Test-Path $OutDir)) { New-Item -ItemType Directory -Path $OutDir | Out-Null }

function Read-Status {
    for ($k = 0; $k -lt 5; $k++) {
        try { return Get-Content $statusPath -Raw -ErrorAction Stop | ConvertFrom-Json } catch { Start-Sleep -Milliseconds 150 }
    }
    throw "could not read $statusPath"
}
function Read-LogSince([long]$from) {
    if (-not (Test-Path $modLog)) { return "" }
    $fs = [IO.File]::Open($modLog, 'Open', 'Read', 'ReadWrite')
    try {
        if ($from -gt $fs.Length) { $from = 0 }
        $fs.Seek($from, 'Begin') | Out-Null
        $sr = New-Object IO.StreamReader($fs, [Text.Encoding]::GetEncoding(28591))
        return $sr.ReadToEnd()
    } finally { $fs.Close() }
}

$logMark = if (Test-Path $modLog) { (Get-Item $modLog).Length } else { 0 }
$s0 = Read-Status
$x0 = & $stateScript -Dir $Dir -Quiet
$t0 = Get-Date
$deadline = $t0.AddMinutes($Minutes)
$rows = @()
$shotRows = @()
$fails = @()
$nextShot = $t0.AddMinutes($ShotEveryMinutes)
Write-Host ("soak: {0} min, sampling every {1} s, a per-eye capture every {2} min; build {3}, method {4}, capture {5}" -f
    $Minutes, $SampleSeconds, $ShotEveryMinutes, $s0.build, $s0.stereo.method, $s0.capture.mode)

while ((Get-Date) -lt $deadline) {
    Start-Sleep -Seconds $SampleSeconds
    $s = Read-Status
    $x = & $stateScript -Dir $Dir -Quiet
    $row = [pscustomobject]@{
        t          = [int]((Get-Date) - $t0).TotalSeconds
        presents   = [long]$s.frame.presents - [long]$s0.frame.presents
        submits    = [long]$s.xr.submits - [long]$s0.xr.submits
        framesOut  = [long]$s.stereo.framesOut - [long]$s0.stereo.framesOut
        hz         = [math]::Round([double]$s.frame.hz, 1)
        simLayered = [long]$x.layeredFrames - [long]$x0.layeredFrames
        session    = $x.sessionState
        capMode    = $s.capture.mode
        capUs      = $s.capture.costTotalUs
        poisoned   = $s.xr.poisoned
    }
    $rows += $row
    Write-Host ("  {0,4}s presents +{1,-6} submits +{2,-6} sim layered +{3,-6} {4,5} Hz {5,-12} cap {6} {7} us" -f
        $row.t, $row.presents, $row.submits, $row.simLayered, $row.hz, $row.session, $row.capMode, $row.capUs)
    if ($row.session -ne "FOCUSED") { $fails += "session left FOCUSED at ${($row.t)}s: $($row.session)" }
    if ($row.poisoned -eq $true) { $fails += "the VR work was POISONED at $($row.t)s (an exception on the present path)" }
    if ((Get-Date) -ge $nextShot) {
        $nextShot = $nextShot.AddMinutes($ShotEveryMinutes)
        $tag = "soak_{0:D2}m" -f [int][math]::Round($row.t / 60)
        try {
            $shot = & $shotScript -Dir $Dir -Out (Join-Path $OutDir $tag) -Quiet
            $sr = [pscustomobject]@{ t = $row.t; tag = $tag; nonBlackL = $shot.NonBlackPctL; nonBlackR = $shot.NonBlackPctR; quads = $shot.QuadLayers; layers = $shot.Layers }
            $shotRows += $sr
            Write-Host ("        capture {0}: L {1}% R {2}% non-black, {3} quad layer(s)" -f $tag, $sr.nonBlackL, $sr.nonBlackR, $sr.quads)
            if ([double]$sr.nonBlackL -lt $MinNonBlack) { $fails += "capture ${tag}: LEFT eye $($sr.nonBlackL)% non-black (< $MinNonBlack)" }
            if ([double]$sr.nonBlackR -lt $MinNonBlack) { $fails += "capture ${tag}: RIGHT eye $($sr.nonBlackR)% non-black (< $MinNonBlack)" }
            if ([int]$sr.quads -lt 1) { $fails += "capture ${tag}: no quad layer" }
        } catch { $fails += "capture ${tag} failed: $_" }
    }
}

$last = $rows[-1]
$ratio = if ($last.presents -gt 0) { [math]::Round($last.submits / $last.presents, 3) } else { 0 }
$simRatio = if ($last.submits -gt 0) { [math]::Round($last.simLayered / $last.submits, 3) } else { 0 }
if ($ratio -lt $MinRatio) { $fails += "submits/presents = $ratio over the window (< $MinRatio): presents that handed the runtime nothing" }
if ($simRatio -lt $MinRatio -or $simRatio -gt (2 - $MinRatio)) { $fails += "the sim's layeredFrames/submits = $simRatio: the simulator did not see one layered frame per submit" }
$text = Read-LogSince $logMark
foreach ($rx in @('SUBMISSION IDLE', 'EXCEPTION', 'POISONED', 'REVERTED to the original', 'CHANGED to something else')) {
    $m = [regex]::Matches($text, "(?m)^.*$rx.*$")
    if ($m.Count -gt 0) { $fails += "$($m.Count) log line(s) matched '$rx' during the window; first: $($m[0].Value.Trim())" }
}

Write-Host ""
Write-Host ("soak: {0} min window: presents {1}, submits {2} (ratio {3}), stereo framesOut {4}, sim layered {5} (ratio {6}), {7} captures" -f
    $Minutes, $last.presents, $last.submits, $ratio, $last.framesOut, $last.simLayered, $simRatio, $shotRows.Count)
$hzs = $rows | ForEach-Object { $_.hz }
Write-Host ("soak: present rate min {0} mean {1} max {2} Hz; capture cost last {3} us/present ({4})" -f
    ($hzs | Measure-Object -Minimum).Minimum, [math]::Round(($hzs | Measure-Object -Average).Average, 1), ($hzs | Measure-Object -Maximum).Maximum, $last.capUs, $last.capMode)
if ($fails.Count -eq 0) {
    Write-Host "soak: PASS - both eyes received the game's frame at the game's rate for $Minutes minutes" -ForegroundColor Green
    exit 0
}
Write-Host "soak: FAIL" -ForegroundColor Red
$fails | ForEach-Object { Write-Host "  - $_" -ForegroundColor Red }
exit 1
