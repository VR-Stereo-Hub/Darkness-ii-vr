# soak.ps1 - R1's protocol (docs/ROADMAP.md): a timed play session with the four
# canary hooks live, across a menu round trip, a checkpoint reload and a level
# restart, driven entirely through the mod's input lane. The log carries the
# evidence: each canary's hits/s and byte re-read every second, `mark` lines
# for every protocol step, and the exit kind at the end.
#
#   .\tools\soak.ps1 -Minutes 30                 # install (canaries ON), launch, boot, play, quit, parse
#   .\tools\soak.ps1 -Minutes 30 -Attach         # the game is already up with the canaries live
#   .\tools\soak.ps1 -Minutes 30 -Attach -NoBoot # already in gameplay
# Exit codes: 0 protocol complete and the game quit cleanly; 4 the game exited
# on its own (the CEG failure shape: read the log's last lines); 5 gameplay
# never answered the seam.
# NOTE: keep this file pure ASCII (PowerShell 5.1 misreads BOM-less UTF-8).
param(
    [int]$Minutes = 30,
    [switch]$Attach,
    [switch]$NoBoot,
    [int]$MenuMinute = 10,
    [int]$CheckpointMinute = 15,
    [int]$LevelMinute = 20,
    [string]$GamePath = ""
)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot "lib\game-path.ps1")
$cmd = Join-Path $PSScriptRoot "game-cmd.ps1"
$shot = Join-Path $PSScriptRoot "game-shot.ps1"
$log = Get-D2LogPath $GamePath

function Step([string]$text) { Write-Output ("[{0:HH:mm:ss}] {1}" -f (Get-Date), $text) }
function Cmd { param([string[]]$Lines) try { & $cmd -TimeoutSec 6 @Lines | Out-Null; return $true } catch { Step "seam: $($_.Exception.Message)"; return $false } }
function Shot([string]$tag) { try { $p = & $shot -Tag $tag -WaitSeconds 8; Step "shot $p" } catch { Step "shot failed: $($_.Exception.Message)" } }
function Alive { return [bool](Get-D2Process) }

if (-not $Attach) {
    & (Join-Path $PSScriptRoot "install.ps1") -Release -GamePath $GamePath -Set "Canary.Cold=1","Canary.Tick=1","Canary.CallSite=1","Canary.Hot=1"
    & (Join-Path $PSScriptRoot "launch-game.ps1") -WaitBanner -GamePath $GamePath
}
if (-not (Alive)) { throw "$script:D2Proc is not running" }
if (-not $NoBoot) { & (Join-Path $PSScriptRoot "boot.ps1") }

if (-not (Cmd @("mark soak: START $Minutes min protocol", "canary status"))) { Step "gameplay never answered the seam"; exit 5 }
$start = Get-Date
$end = $start.AddMinutes($Minutes)
$doneMenu = $false; $doneCheckpoint = $false; $doneLevel = $false
$lastShot = $start; $i = 0
$exitKind = "clean"
Shot "soak-start"
while ((Get-Date) -lt $end) {
    if (-not (Alive)) { $exitKind = "the game EXITED on its own"; break }
    $minute = [int]((Get-Date) - $start).TotalMinutes
    if (-not $doneMenu -and $minute -ge $MenuMinute) {
        $doneMenu = $true
        Step "menu round trip"
        Cmd @("mark soak: menu round trip (esc, 5 s, esc)", "key esc tap 150") | Out-Null
        Start-Sleep -Seconds 5; Shot "soak-pause"
        Cmd @("key esc tap 150", "mark soak: menu round trip done") | Out-Null
        Start-Sleep -Seconds 3
        continue
    }
    if (-not $doneCheckpoint -and $minute -ge $CheckpointMinute) {
        $doneCheckpoint = $true
        Step "checkpoint reload"
        Cmd @("mark soak: checkpoint reload (esc, down, down, enter, enter)", "key esc tap 150") | Out-Null
        Start-Sleep -Seconds 3
        Cmd @("key down tap 150", "key pause up 300", "key down tap 150") | Out-Null
        Start-Sleep -Seconds 2; Shot "soak-checkpoint-highlight"
        Cmd @("key enter tap 150") | Out-Null
        Start-Sleep -Seconds 3
        Cmd @("key enter tap 150", "mark soak: checkpoint reload confirmed, loading") | Out-Null
        Start-Sleep -Seconds 45; Shot "soak-after-checkpoint"
        Cmd @("mark soak: checkpoint reload done") | Out-Null
        continue
    }
    if (-not $doneLevel -and $minute -ge $LevelMinute) {
        $doneLevel = $true
        Step "level restart (a level load)"
        Cmd @("mark soak: level restart (esc, down, enter, enter)", "key esc tap 150") | Out-Null
        Start-Sleep -Seconds 3
        Cmd @("key down tap 150") | Out-Null
        Start-Sleep -Seconds 2; Shot "soak-level-highlight"
        Cmd @("key enter tap 150") | Out-Null
        Start-Sleep -Seconds 3
        Cmd @("key enter tap 150", "mark soak: level restart confirmed, loading") | Out-Null
        Start-Sleep -Seconds 75; Shot "soak-after-level"
        Cmd @("mark soak: level restart done") | Out-Null
        continue
    }
    # Keep the game live: a step forward or back and a look, alternating.
    $i++
    switch ($i % 4) {
        0 { Cmd @("key w tap 500") | Out-Null }
        1 { Cmd @("mouse move 200 0") | Out-Null }
        2 { Cmd @("key s tap 500") | Out-Null }
        3 { Cmd @("mouse move -200 0") | Out-Null }
    }
    if (((Get-Date) - $lastShot).TotalMinutes -ge 5) { $lastShot = Get-Date; Shot ("soak-min{0:00}" -f $minute) }
    Start-Sleep -Seconds 20
}
Step "protocol loop ended: $exitKind"
if (Alive) {
    Cmd @("mark soak: END after $([int]((Get-Date) - $start).TotalMinutes) min", "canary status") | Out-Null
    Start-Sleep -Seconds 2
    Shot "soak-end"
    & (Join-Path $PSScriptRoot "quit-game.ps1") -TimeoutSec 20
}
Start-Sleep -Seconds 2
Write-Output ""
Write-Output "=== soak summary ==="
$lines = Get-Content $log
$reverts = @($lines | Where-Object { $_ -match "REVERTED|CHANGED to something else" })
$refused = @($lines | Where-Object { $_ -match "canary.*REFUS" })
$exceptions = @($lines | Where-Object { $_ -match "EXCEPTION" })
$lastStatus = @($lines | Where-Object { $_ -match "\[canary\] canary (cold|tick|callsite|hot) +0x" } | Select-Object -Last 4)
Write-Output "duration: $([int]((Get-Date) - $start).TotalMinutes) min (asked $Minutes); events: menu=$doneMenu checkpoint=$doneCheckpoint level=$doneLevel"
Write-Output "exit: $exitKind; process alive now: $(Alive)"
Write-Output "byte reverts/changes: $($reverts.Count)"; $reverts | Select-Object -First 8
Write-Output "refused canaries: $($refused.Count)"; $refused | Select-Object -First 4
Write-Output "exceptions recorded: $($exceptions.Count)"; $exceptions | Select-Object -First 4
Write-Output "last canary lines:"; $lastStatus
Write-Output "log tail:"; $lines | Select-Object -Last 6
if ($exitKind -ne "clean") { exit 4 }
exit 0
