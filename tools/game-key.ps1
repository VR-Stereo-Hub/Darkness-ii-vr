# game-key.ps1 - press keys in the game through the mod's own injection lane
# (the seam words key/type/mouse/focus, executed with SendInput INSIDE the game
# process on a worker thread; see src/core/input/inject.h). Each press is one
# seam line, so a sequence lands in order at the mod's 1 Hz poll cadence: pass
# several keys to one call and they go out as one batch.
#   .\tools\game-key.ps1 enter                  # tap
#   .\tools\game-key.ps1 w -Hold 800            # hold W for 800 ms
#   .\tools\game-key.ps1 esc enter -Gap 400     # esc, 400 ms, enter
#   .\tools\game-key.ps1 -Mouse 200 0           # look right
#   .\tools\game-key.ps1 -Click                 # left click
# NOTE: keep this file pure ASCII (PowerShell 5.1 misreads BOM-less UTF-8).
[CmdletBinding(PositionalBinding=$false)]
param(
    [int]$Hold = 60,
    [int]$Gap = 250,
    [int[]]$Mouse = @(),
    [switch]$Click,
    [switch]$RightClick,
    [switch]$Focus,
    [Parameter(ValueFromRemainingArguments=$true)][string[]]$Keys
)
$ErrorActionPreference = 'Stop'
$lines = @()
if ($Focus) { $lines += "focus" }
foreach ($k in $Keys) {
    $lines += "key $k tap $Hold"
    if ($Gap -gt 0 -and $k -ne $Keys[-1]) { $lines += "key pause up $Gap" }   # a harmless key-up spaced by the gap
}
if ($Mouse.Count -eq 2) { $lines += "mouse move $($Mouse[0]) $($Mouse[1])" }
if ($Click) { $lines += "mouse click left" }
if ($RightClick) { $lines += "mouse click right" }
if ($lines.Count -eq 0) { throw "give key names, -Mouse dx dy, -Click or -Focus" }
& (Join-Path $PSScriptRoot "game-cmd.ps1") @lines
