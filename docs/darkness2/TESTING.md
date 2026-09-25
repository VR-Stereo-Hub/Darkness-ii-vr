# Testing - The Darkness II

Everything here is planned until the framework ticket lands; the shape is Dishonored's, which
proved out over 1.0. `docs/STATUS.md` records what has actually run.

## The rig

- Dev PC: the game at `D:\SteamLibrary\steamapps\common\Darkness II` (resolved from
  `libraryfolders.vdf`; never hardcode the drive in a script).
- Headset: Quest 3 over Virtual Desktop (VDXR), 90 Hz first, 120 Hz once the cadence is
  judged. SteamVR headsets go through the 32-bit shim.
- **One game at a time.** Only one process can own the headset; the harness asserts no other
  mod's game is running.

## Install / launch loop

1. `tools\build.ps1` then `tools\install.ps1` (the mod module, the shim, `openvr_api.dll`).
   Install diffs the full installed ini against the previous one and reports a zero-setting
   diff as such.
2. **Launch through Steam** (`tools\launch-game.ps1`). The exe delay-loads
   `Tools\steam_api.dll` and expects the client. `tools\tail-log.ps1` in a second window.
   The first lines: the build tag banner, the loading route that fired (R0), the runtime
   layer's instance line (which OpenXR runtime answered), `device hooks installed`,
   `INSTALLED at 0x...` for each engine hook with its byte-verify result, then
   `xr: session created`, `xr: pipeline READY`, `stereo: method 'mono' registered - default`.
3. Continue the newest save. Whether New Game is safe in the harness is unknown until the
   opening restaurant sequence has been run under the mod.
4. Copy `darkness2_vr.log` out before every relaunch (rotation keeps ten, but the archive is
   the evidence).

Files: the log and the ini next to the exe; `command.txt` / `status.json` in
`%LOCALAPPDATA%\Darkness2VR` (or `D2VR_DATA_DIR`).

## Flat checks (no headset)

- `tools\game-cmd.ps1 "status"` then `tools\status-dump.ps1`: `state` GAMEPLAY, `hooks.*`
  true with their verify results, `stereo.method` mono, `stereo.framesOut` advancing.
- `tools\game-cmd.ps1 "stereo status"` / `"camera status"`: the two seams' log lines.
- `tools\game-cmd.ps1 "dump frame"`: the capture and the method's output texture agree.
- `tools\game-cmd.ps1 "camera eyetest"` in gameplay, standing still: one HONOURED verdict
  among the candidate FOV writes, then `DONE`; recorded in ENGINE_NOTES.
- `tools\game-cmd.ps1 "lua <expr>"` (after R2): the Lua lane executes and prints.

## Simulator checks (OpenXR lane, no headset)

`docs/VERIFICATION.md`. `xrsim-launch.ps1`; expect `xr: runtime "d2-xrsim"`,
`xr: pipeline READY`. Then reach gameplay, foreground the window, `mono.xrs` (the rung-1
gate: a quad layer, both eyes non-black, equal bboxes), `headlook.xrs` (the quad is
head-locked, so the captured screen must NOT move). `stereo.xrs`, `world-6dof.xrs`,
`hands.xrs` and `eye-check.ps1` wait for a stereo method (S2).

## Headset checklist shape (M1 / M2)

- The game on a head-locked screen in BOTH eyes; recenter works; head look turns the game
  camera 1:1; lean and crouch; roomscale auto-recenter.
- The pad works (sticks, triggers, faces, bumpers for the demon arms); both stick clicks: a
  tap opens the F10 panel, a hold recenters.
- Menus, loading screens and Bink videos show on the screen; no `EXCEPTION` in the log; the
  session survives alt-tab.
- Save/load: head tracking survives.
- A 60-second non-regression sweep first, then the session's headline feature, then the
  specific judgements, each with a named live A/B lever. End with "expected noise, not bugs"
  and "toggle `<lever> off` first; if the symptom survives it predates this session".

## One question per launch

The agent never launches the game. It builds, installs, diffs the ini, archives the log and
hands the user ONE question with the expected outcomes and what each means. A test that needs
a command typed mid-run is a test that does not get run: build always-on probes or F10 A/Bs.

## Crash triage

`darkness2_vr_crash.txt` (fingerprint: module+offset, thread, registers, callers, the runtime
context) and the minidump in `%LOCALAPPDATA%\Darkness2VR\dumps`. `tools\read-dump.py <dmp>`
summarises a minidump without symbols. `D2VR_SKIP=hands,tentacles,overlay` bisects a crash to
a subsystem without a rebuild; the log must say what a skip disabled. A crash that leaves no
record is itself a bug (Dishonored VR-177: the crash recorder's budget was spent by the mod's
own probes).

**CEG note.** If the game exits silently with no crash record after a hook is installed, that
is R1's question before it is anything else: record which hooks were live, at which
addresses, and whether the exit came at startup, at a level load or at a fixed interval.
