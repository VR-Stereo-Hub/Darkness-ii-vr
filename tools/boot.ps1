# boot.ps1 - drive the running game from the title screen into gameplay on the
# newest save, through the mod's input lane, with a screenshot at each step so
# the sequence can be checked afterwards (the shots are BMPs under the data dir).
#
# Measured 2026-09-25: the gameswf title screen and the MAIN menu do not see a
# 60-150 ms key tap; 400 ms is seen by both. The PAUSE menu accepts 150 ms taps.
# The main menu opens with CONTINUE preselected; a 400 ms Enter selects it and
# the save loads in about 40 s. Each step is checked by the mean luma of a
# backbuffer shot: the title and menu are dark (under 70), gameplay in the
# alley is lighter (over 90); a step that did not change the picture is retried
# once, and the script exits 1 if gameplay is not reached.
#   .\tools\boot.ps1                 # title -> menu -> Continue -> gameplay
#   .\tools\boot.ps1 -LoadSeconds 60
# NOTE: keep this file pure ASCII (PowerShell 5.1 misreads BOM-less UTF-8).
param([int]$TitleWaitSeconds = 20, [int]$LoadSeconds = 45, [string]$Tag = "boot")
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot "lib\game-path.ps1")
$cmd = Join-Path $PSScriptRoot "game-cmd.ps1"
$shot = Join-Path $PSScriptRoot "game-shot.ps1"
if (-not (Get-D2Process)) { throw "$script:D2Proc is not running - launch-game.ps1 first" }
Add-Type -AssemblyName System.Drawing
function Luma([string]$bmp) {
    $b = New-Object System.Drawing.Bitmap $bmp
    try { $s = 0; $n = 0; for ($y = 0; $y -lt $b.Height; $y += 48) { for ($x = 0; $x -lt $b.Width; $x += 48) { $c = $b.GetPixel($x, $y); $s += $c.R + $c.G + $c.B; $n++ } }; return [math]::Round($s / (3 * $n), 1) }
    finally { $b.Dispose() }
}
function Step([string]$text) { Write-Output ("[{0:HH:mm:ss}] {1}" -f (Get-Date), $text) }

# Wait until the mod is presenting (the seam answers) and the title has had time to appear.
$deadline = (Get-Date).AddSeconds(90)
while ((Get-Date) -lt $deadline) {
    try { & $cmd -TimeoutSec 4 "mark boot: waiting for the title" | Out-Null; break } catch { Start-Sleep -Seconds 2 }
}
Start-Sleep -Seconds $TitleWaitSeconds
$t = & $shot -Tag "$Tag-title"; Step "title shot $t (luma $(Luma $t))"

# title -> main menu (Space, 400 ms); the menu differs from the title in its layout, not
# its brightness, so this step is trusted by the next one succeeding.
& $cmd "focus" "mark boot: title -> menu" "key space tap 400" | Out-Null
Start-Sleep -Seconds 5
$m = & $shot -Tag "$Tag-menu"; Step "menu shot $m (luma $(Luma $m))"

# Continue (preselected) with a 400 ms Enter, then wait for the load. Retry once if the
# picture stayed dark (the menu did not take the key).
for ($attempt = 1; $attempt -le 2; $attempt++) {
    # Attempt 1: a 400 ms Enter on the preselected CONTINUE. Attempt 2: a mouse click, which
    # selects the item under or last highlighted (the menu drops its keyboard highlight once
    # the mouse moves; measured 2026-09-25).
    if ($attempt -eq 1) { & $cmd "mark boot: Continue (attempt $attempt, Enter)" "key enter tap 400" | Out-Null }
    else { & $cmd "mark boot: Continue (attempt $attempt, click)" "mouse move -80 0" "mouse click left 120" | Out-Null }
    Start-Sleep -Seconds $LoadSeconds
    $g = & $shot -Tag "$Tag-gameplay"
    $l = Luma $g
    Step "gameplay shot $g (luma $l)"
    if ($l -gt 80) { & $cmd "mark boot: gameplay reached" | Out-Null; Write-Output "boot done; gameplay shot: $g"; exit 0 }
    Step "picture still dark after Continue; retrying"
}
& $cmd "mark boot: gameplay NOT reached" | Out-Null
Write-Output "boot: gameplay not reached (last luma $l); look at the shots under $(Get-D2ShotsDir)"
exit 1
