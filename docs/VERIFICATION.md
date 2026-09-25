# Verification

How a question about the mod gets answered, and by what. The catalog is the Dishonored shape
because that shape shipped 1.0; every row marked **planned** lands with the framework and
simulator tickets in M1, and this file gains its rows as the instruments do.

The rule the whole file exists for: **asking the user to put the headset on for something the
simulator could have answered is a wasted evening; claiming a simulator pass settles a
perceptual question is worse.**

## The three tiers

| Tier | What it answers | Instrument |
|---|---|---|
| **1. Numeric** | Does the write survive, does the counter move, is the pairing right, what is the cost | The log, `status.json`, the command seam, host tests that compile production code with synthetic inputs |
| **2. Flat visual** | Is the thing drawn, where, in which eye, does it move with the head | Simulator captures (per-eye PNG plus JSON stats), `img-diff`, the frame dump |
| **3. Headset** | Comfort, judder, warp, world scale, fusion, whether the hands and the tentacles feel like the player's | A human, the F10 overlay with a live A/B, a written verdict in STATUS with the build tag |

Tiers are run in order. A tier-3 question is asked only when tiers 1 and 2 have nothing left
to say, and with the A/B that would disprove the answer already in the panel.

## The decision table (planned; grows per ticket)

The shape: one row per intent, naming the tool, the command and how to read the result. The
first rows this repo will have:

| Intent | Tool | Command | Reading |
|---|---|---|---|
| Is the simulator healthy | `xrsim-selftest.ps1` | `.\tools\xrsim-selftest.ps1` | PASS; `xr_hello32` reaches a session |
| Did the module load and hook | the log | `.\tools\tail-log.ps1` | the banner, the loading route line, `INSTALLED at 0x... verify OK` per hook |
| Is the seam alive | `game-cmd.ps1` | `"status"` then `status-dump.ps1` | `state`, `hooks.*`, `stereo.method`, `stereo.framesOut` advancing |
| Both eyes non-black on the mono screen | `xrsim-run.ps1` | `-Path tools\xrsim\mono.xrs` | a quad layer, both eyes non-black, equal bboxes; a black eye attributed by the `COMPOSITOR fault` / `APP fault` line |
| The screen is head-locked | `xrsim-run.ps1` | `headlook.xrs` | the captured screen does NOT move under head yaw |
| Which FOV write is honoured | `game-cmd.ps1` | `"camera eyetest"` | exactly one HONOURED verdict, the rest DISCARDED, then DONE |
| Stereo geometry | `xrsim-run.ps1` | `stereo.xrs` | two projection views, `eyeSeparationM == IPD`, left vs right `img-diff` well above the noise floor |
| The Lua lane works (after R2) | `game-cmd.ps1` | `"lua print(1+1)"` | `2` in the log from the game thread |
| An animation started or was suppressed (after R7) | the log | `D2VR_LOG_CATS=anim:debug` | named start lines; a suppressed name absent |

## The simulated runtime (planned; port of Dishonored's `xrsim`)

A 32-bit OpenXR runtime DLL that presents as a Quest 3: head and hand poses, every controller
button, deterministic frame stepping, per-eye compositor captures with source stats,
selected per process through `XR_RUNTIME_JSON`. `.xrs` sequences script it, with `@log`,
`@nolog`, `@modassert` and `@mod a; b; c` (which routes several seam words in ONE write,
because the seam is one slot polled at 1 Hz). Numeric thresholds are recorded here as they
are measured; the Dishonored values (standing-still noise about 0.4 mean-abs-diff, a real FOV
change 4 to 7, `nonBlackPct > 50` in gameplay, frames per second near the refresh rate) are
the shape, not the numbers.

## The end-to-end agent workflow (planned)

```powershell
.\tools\xrsim-selftest.ps1                       # 0. the SIM is healthy
.\tools\build.ps1; .\tools\install.ps1           # 1. build + install
.\tools\xrsim-launch.ps1                         # 2. launch on the sim; throws unless runtime == d2-xrsim
.\tools\boot.ps1 -Attach                         # 3. reach gameplay (-Attach is mandatory in sim mode)
.\tools\game-cmd.ps1 "status"                    # 4. the seam works; read status.json
.\tools\xrsim-cmd.ps1 "reset" "head rot 0 0 0"   # 5. drive the rig and capture
$a = .\tools\xrsim-shot.ps1 -Out "$env:TEMP\d2vr\head_0"
.\tools\xrsim-cmd.ps1 "head rot 35 0 0"
$b = .\tools\xrsim-shot.ps1 -Out "$env:TEMP\d2vr\head_35"
$a.ProjViews -eq 2; $a.EyeSeparationM             # 6. assert with numbers
.\tools\img-diff.ps1 -A $a.Left -B $a.Right      #    stereo: well above noise
.\tools\img-diff.ps1 -A $a.Left -B $b.Left       #    head look moved the camera
```

## Host tests (planned)

`tools/<topic>-host.ps1` plus `<topic>-tests.cpp`: compile the production code with synthetic
inputs, through `vswhere`, and **require the old code to fail** (a negative control). Every
pure decision core (the swing detector, the pad composer, the tag ring, the migration) gets
one before it ships.

## Gotchas that transfer from the siblings

Read these before blaming the mod for a harness result.

1. **An elevated shell**: the Khronos loader ignores `XR_RUNTIME_JSON` there. The launcher
   refuses.
2. **A 64-bit simulator DLL** is silently skipped by a 32-bit process and the real runtime is
   used. The installer checks the PE machine.
3. **`XR_RUNTIME_JSON` is per process**: a direct launch needs `boot.ps1 -Attach` or Steam
   starts a second game. Where a direct launch dies (this game expects the Steam client),
   the manifest goes through the ini and the launch goes through Steam.
4. **A `command.txt` written with a BOM** corrupts the first token. Use `WriteAllText`.
5. **Two quick `command.txt` writes overwrite each other.** One slot, 1 Hz.
6. **Foreground the game window before any capture.** Unfocused, the world stops rendering
   and the capture shows HUD on black.
7. **Trust per-eye statistics only after `mono.xrs` passes** on the build.
8. **The game does not auto-continue into a level on the simulator.** The attract screen's
   camera dispatches like gameplay; six Dishonored runs measured the attract camera. Walk in
   with keys and look at a capture before believing a number.
9. **A mod menu flag can stay up after a load**; open and close the pause menu to clear it,
   and record the trap if it happens here.
10. **A dump on the present thread changes what it dumps** (a 600 ms PNG encode re-armed the
    second draw). Encode off-thread.
11. **The menu's draws outnumber its presents**; any instrument pairing draws to presents by
    order alone reads wrong there.
12. **The simulator force-grants focus**, so session-state bugs (unfocused pacing) are
    invisible to it.
13. **The simulator's display clock leaps after a game-thread hitch**, so a simulated-hand
    gesture with no speed margin is not a reliable gate.
14. **The agent's shell can virtualize writes under the user profile** (a file the shell saw
    was absent to a game launched through Steam). Point the data dir at a real location.
15. **A launch-time "produced no frames" check can fire before the game reaches a rendering
    state.** Check for the process and read the log before believing it.
16. **`-ViaSteam` restores the ini from a pre-launch backup**, so a key the running game wrote
    is gone at the next launch. Keep launch-time asks in a file the launcher does not touch.
17. **A mean-abs-diff lies in a dark scene**, and this game is dark on purpose. Pick a lit
    reference view for picture diffs, or measure a feature (an edge position), not a mean.
18. **A quad layer is invisible to a window screenshot.** Judge layers from the compositor
    capture.
19. **One game at a time.** Only one process can own the headset.
20. **Debug-CRT dialogs** (`sprintf_s` on a too-small buffer, RTC stack checks) freeze the
    game behind a modal with no dump. Use truncating variants; count `ret imm / 4` before a
    probe hook.

## What still needs a human

Comfort, judder, warp, world scale, the mono screen's size and distance, fusion, hand
placement feel, whether the demon arms feel like the player's, whether a finisher is
comfortable, anything about Virtual Desktop's own reprojection. Write the verdict in STATUS
with the build id from the log's first line, in the tester's terms and never their words.
