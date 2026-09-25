# Architecture

The planned shape of the mod, marked **planned** or **measured** section by section, and the
dated decision log at the bottom. Until the research gate (`docs/ROADMAP.md` R0-R7) is
answered, most of this is the Dishonored shape carried over with the seams this engine will
need. When a section is measured, the word changes and the measurement is cited.

## Overview (planned)

```
 DarknessII.exe (32-bit, D3D9, Lua 5.1, gameswf)
   |
   |  R0: the loading route (app-dir d3d9.dll proxy expected)
   v
 the mod module -------------------------------------------------------------.
   |                                                                          |
   |  DllMain: paths, clock, log, kill switch, two early ini ints             |
   |  Direct3DCreate9(Ex) wrap: register stereo methods, load config,         |
   |    bring up the runtime layer, install the frame hooks                   |
   |                                                                          |
   |  game thread            present thread                Lua lane           |
   |  ------------           --------------                --------           |
   |  head pose -> camera    capture -> eye texture ->     one-shot config,   |
   |    seam (rotation,      runtime layer -> xrEndFrame   SWIG calls by      |
   |    position, FOV)       pace thread: xrWaitFrame      name (R2)          |
   |  pad bridge (XInput     stereo method: mono | reentry                    |
   |    IAT slot)            overlay (ImGui F10)                              |
   |  anim/model control     HUD redirect (gameswf)                           |
   |    layers A0-A7, M0-M6                                                   |
   |  hands, weapons,                                                         |
   |    tentacles (engine-                                                    |
   |    side pose writes)                                                     |
   '--------------------------------------------------------------------------'
```

Three lanes. **The game thread** owns every engine write (camera, pose buffer, gameplay
state), because the engine reads those there. **The present thread** owns every runtime call
(`xrEndFrame`, swapchains, layers) and the capture; a pace thread runs `xrWaitFrame` by
request only, with a deadline. **The Lua lane** is code the mod executes inside the engine's
own `lua_State` at a point on the game thread the engine already calls into script; it never
runs on any other thread. Cross-lane state is published as self-expiring snapshots.

## The loading route (R0 verdict, 2026-09-25: the app-dir d3d9.dll)

The renderer is not in the import table: the exe calls `LoadLibraryA("D3D9.DLL")` by bare
name and then `Direct3DCreate9Ex(0x20)` (`Direct3DCreate9` only if the export is missing or
`Graphics.EnableDirect3D9Ex` is 0). **The mod module is a `d3d9.dll` next to `DarknessII.exe`**
that forwards all 23 exports of the system DLL (by ordinal for the six unnamed ones) and wraps
the two create calls. Measured (`docs/darkness2/ENGINE_NOTES.md` s9): the loader takes it from
the exe's directory 0.5 s after process creation, even though the exe had already called
`SetDllDirectoryA` to its PhysX folder, because the application directory is always searched
first. Fallbacks, unused: an `xinput1_3.dll` proxy hooking the exe's `LoadLibraryA` wrapper; a
suspended-launch injector.

`DllMain` is loader-lock safe: paths, clock, log, the kill switch (`disable_vr.txt`), the log
environment and the route self-report (our module path, the exe, `GetDllDirectory`, the PEB
loader list). Nothing that touches the ini, another DLL or engine state. Everything else
(config, the crash handler, the seam, `status.json`, the game layer, the D3D9 vtable hooks)
runs from the first `Direct3DCreate9Ex` call, and every engine code hook from the first
present.

**CEG.** The exe is Valve CEG linked. No byte is ever written to the exe on disk. Hooks are
in-memory and installed after the game is up. R1 measures whether and where they survive.

## The frame path (planned; the Dishonored shape)

Per present: capture the D3D9 backbuffer (with a D3D9Ex device, a fenced shared-surface copy;
otherwise a deferred readback), blit into an eye-tagged D3D11 texture on the runtime's
adapter, `xrEndFrame`. The stereo method decides what a present IS: on the mono screen, one
present is the whole frame on a head-locked quad in both eyes; on reentry, one tick yields two
presents, each tagged with its eye through a ring paired by the per-present camera step.

## The stereo ladder (planned)

Behind one seam (`core/gfx/stereo.h`), each method switchable live with `stereo <name>`:

- **mono**: the game's frame on a head-locked quad, the same image in both eyes. The
  fallback, always available.
- **reentry** (SequentialReentry): the scene-draw root's ONE call site patched to draw twice
  per tick, once per eye, into a projection layer. Dishonored's shipping default.
- **aer** (AlternateEye): one eye per tick. Registered as a stub for the A/B, as on
  Dishonored; never built unless a measurement asks for it.

R4 adds a question no sibling had: the engine has a native 3D Vision path. If it draws the
scene twice engine-side with its own convergence and separation, that is a fourth method
candidate with the projection already per-eye. If it hands stereo to the driver, it is
irrelevant.

## The runtime layer (planned; adopted verbatim)

`core/vr/openxr_runtime` and `openxr_input` from Dishonored, which took them from BioShock.
OpenXR only, `XR_KHR_D3D11_enable`, the static Khronos loader linked in, a D3D11 device
created on the adapter LUID the runtime names (`D3D_DRIVER_TYPE_UNKNOWN`). Runtime
selection: VDXR, any native 32-bit runtime, or the OpenXR-on-OpenVR shim for SteamVR. The
64-bit implicit API layer guard (OBS) comes with it. Two host seams only: the device
provider and the frame texture. Everything else stays byte-identical so fixes port.

## The camera seam (planned; S0.5 and S1 measure it)

`game/darkness2/camera.h`: `set_eye`, IPD, world scale (the unit confirmed in S1), the FOV
lever, a per-thread second-pass fork for reentry. The `camera eyetest` instrument writes each
candidate field (the camera object's FOV, `SetBaseFovOverride` through Lua,
`mHorizontalFOVHack`, the CTAB constant directly) and reports which the renderer honoured.
Head rotation is written at the engine tick as a yaw delta (so it composes with the stick) and
an absolute pitch. Position is written on the camera seam with a neck-pivot cancel.

## The Lua lane (planned; R2 decides)

The engine's own scripts reach engine objects by name through SWIG (`camCtrl:
SetBaseFovOverride(finalFov)`). Once the mod holds the game's `lua_State` and a safe point on
the game thread, it can do the same: one-shot configuration, reading properties by name,
triggering an animation by name, toggling a debug draw. The SWIG registration tables are also
the fastest map from a class or method NAME to an address. Rules: Lua only from the game
thread at the engine's own script entry; never per-tick for latency-critical writes
(rotation goes through the seam); every chunk the mod loads is the mod's own text, never
game content; A7 (Lua chunk override) substitutes source for a named chunk and logs it.

## The animation and model control layers (planned)

`docs/darkness2/ANIM_AND_MODEL_CONTROL.md`. The pose override (A3) is the seam the hands, the
guns and the tentacles all write through; it sits after the AnimTree evaluates and before the
palette is built, in the buffer's own space, per bone, weighted. Everything engine-side, so
attachments (`WEAPON1`, `mTentacleAttachBone`) follow.

## Hands, weapons, tentacles (planned)

`docs/darkness2/DUAL_WIELD.md` and `docs/darkness2/TENTACLES.md`. Head decoupled from the body
at the yaw input; hands cut from the rig by bone influence on the render thread (the interim
step) and placed by the pose override (the real step); one ray per hand from the aim pose plus
a per-weapon trim; the demon arms on body-locked shoulders, grab by the left controller's
ray, slash by the right controller's swing mapped to the 8 directional anims, finishers as
cinematic hand-back.

## The HUD (planned; S7)

gameswf, not Scaleform. Draws are recognised by their own signature, tagged with the movie
name at load, redirected into private sinks and placed on anchors (off, frame, head-locked
window, world, hand L/R). Menus, loading screens and Bink go to a mono quad placed where the
player looks. The F10 panel is ImGui drawn into the method's output texture and driven from
the controllers.

## Input (planned)

The exe's `XInputGetState` slot serves a synthesized pad composed from the runtime's input
snapshot; rumble returns as haptics. The delay-loaded import gives an IAT slot to re-point
after the game resolves it (the Steam overlay hooks the export thunk, so the slot is the
reliable place; measured on BioShock). Chords: both stick clicks tap for F10 and hold to
recenter. On top: gestures (slash, grab, heart), physical crouch, snap turn. The mod owns the
dual-wield trigger mapping regardless of the game's swap option.

## Config and files

`darkness2_vr.ini` next to the exe, one home per key, the resolved value logged at startup
(`config:` lines), the default literal in `core/config/config.cpp` extracted into
`tests/golden/darkness2_vr.ini` by `tools/ini-golden.py` and diffed by `install.ps1` before
every run. Data dir `%LOCALAPPDATA%\Darkness2VR\` (`D2VR_DATA_DIR`, then `[Paths] DataDir`):
`command.txt`, `ack.txt`, `status.json`, `dumps\`, `shots\`, `xrsim\`. Logs next to the exe,
ten sessions deep. The game's own config is never written.

## Code structure (planned; the Dishonored layout)

The directory contract, in full in `CLAUDE.md` "Repo map": `src/proxy/` (the DLL the game
loads, `DllMain` and the exports), `src/core/` (engine-agnostic: `util/`, `hooks/`,
`framework/`, `gfx/`, `vr/`, `input/`, `ui/`, `config/`), `src/game/darkness2/` (everything
that knows an address or an engine layout: `patterns.h`, `lua/`, camera, head tracking,
present_tick, anim, models, hands, weapons, tentacles, the gameswf HUD, game state, the seam's
game words), `src/legacy/`, `src/tools/` (`xrsim/`, `ovrshim/`, `installer/`, `xr_hello32/`),
`third_party/`, `tools/` (the PowerShell harness and `cache/`), `tests/golden/`, `docs/`. The
one rule that keeps it honest: nothing in `core/` includes anything from `game/`, and every
engine address lives in `patterns.h`.

## The unity build and how a module leaves it (planned)

Dishonored started as one 23k-line file and split it module by module; the rule was that a
module leaves the unity translation unit when it has its own header, its own state and a host
test. This repo starts split: `core/` is engine-agnostic and `game/darkness2/` knows
addresses, and nothing in `core/` includes anything from `game/`.

## Known costs (to be measured)

The capture and the second draw are the two known costs on a D3D9 host; Dishonored measured
16 ms of DMA per present at the headset size on the readback path before moving to the
shared-surface capture, and 220-470 microseconds for the second draw. The numbers here will
be different and are measured in S0.5 and S2, on this GPU, at this render size, and recorded
with the build tag.

---

## Decision log

Dated, newest first. A non-obvious choice, why it was made, and what would reverse it.

### 2026-09-25 - the session drives the game; the "never launch" rule is retired

The user's instruction in M1 session 1: testing is automatic, the session launches, plays and
quits the game and prepares the simulation scripts. What made it possible: an input lane
inside the game process (`core/input/inject`, `SendInput` with scancodes from a worker
thread) and a backbuffer capture (`shot`) the session reads as an image. Headset judgements
stay the user's. Reversed by the user asking for the old one-question-per-launch protocol.

### 2026-09-25 - the proxy forwards all 23 exports, with naked thunks

Not the nine names one game imports: the whole system table, same ordinals, `NONAME` for the
six unnamed entries, so any other module in the process that imports d3d9 by name or ordinal
resolves. Everything but the two creates is a naked tail-jump thunk, so unknown signatures
pass through untouched. `tools/exports-check.ps1` gates the table. Reversed by nothing.

### 2026-09-25 - the hot canary site is derived at runtime, not statically

The engine wraps the device in its own driver class, so the COM `Present` call cannot be told
apart from 1306 look-alike shapes in `.text`. The `Present` vtable hook logs its return
address into the exe; the enclosing function (0x920EE0) became the hot site, byte-verified like
every other. The pattern for later hooks: measure the return address first, then fix the
number in `patterns.h` with its signature. Reversed by nothing.

### 2026-09-25 - engine hooks install from the first present, canaries default OFF

Nothing resolves at init: the four R1 canaries (and every later engine hook) install from the
present thread on the first frame, after the fingerprint check, and are OFF unless the ini or
the seam turns them on. The soak turns them on through `install.ps1 -Set`, which prints the
diff. Reversed by an R1 verdict that says hooks must wait for a later moment.

### 2026-09-25 - staging and main from the first commit

Dishonored ran on one branch until its 1.0.1 hotfix was tagged from a stack of draft PRs the
release branch did not contain. This repo starts with `staging` (integration) and `main`
(release, moved only by the release PR the user merges). Reversed by nothing short of the
user deciding one branch is enough again.

### 2026-09-25 - the runtime layer, the simulator and the shim are adopted verbatim

From Dishonored, which took them from BioShock. Two host seams only. Every fix ports three
ways while the rest stays identical; the day a host-specific change is needed outside the
seams, it goes behind a compile-time host flag rather than a fork.

### 2026-09-25 - CEG: in-memory hooks only, measured before trusted

The exe is CEG-linked and not packed. The mod never writes the exe or patches code on disk.
R1 measures whether in-memory hooks survive a 30-minute session across loads, and where.
Until R1 is written, every code hook is an experiment behind a lever. Reversed by an R1
verdict that names ranges or timing constraints, which then become rules here.

### 2026-09-25 - the Lua lane is researched first, before any feature

No sibling had a script VM with the engine API bound by name. R2 (reach the VM, dump the SWIG
API) is Urgent because a positive verdict changes the cost of every later stage: FOV, near
plane, shake and bob, animation triggers and debug draws become one-line calls instead of
hooks. A negative verdict costs one research ticket.

### 2026-09-25 - the tentacles are re-anchored to the body, not left on the camera

The demon arms hang off `GAME_C1_CAMERA` (to be confirmed by R6). Leaving them there with a
decoupled head would swing them under the chin on every look down. The plan is a body-locked
shoulder anchor (yaw from the body, position from the HMD minus a neck offset, pitch not from
the head), written through the pose override, with the authored idles retargeted to it.
Reversed only by a T1 verdict that the rig cannot be re-parented without model work, in which
case the model layers (M3, M6) do it.

### 2026-09-25 - one ray per hand, and the mod owns the trigger mapping

Two guns means two rays and nothing that averages them. The mod maps left trigger to left
gun regardless of `SwapFireButtonsWhenDualWielding`, and zeroes `mDualWieldingAimViewOffset`
because the view never moves for aiming in VR. Reversed by nothing; a player who wants the
swap gets it as a mod option.

### 2026-09-25 - animation and model control are built as layers before the features

The user's brief: complete control, even if it takes longer, so no later feature hits a wall.
A0-A5 and M0-M3 are M3 tickets that precede the tentacle and dual-wield features that use
them. Reversed by nothing; A6, A7, M4-M6 are M4 work and can slip.

### 2026-09-25 - the game's own config is never written

It is obfuscated on disk, and a setting with two owners has none (the first trap in
`docs/TRAPS.md`). Settings are requested through the engine's own apply path or the Lua lane.
Reversed only if a setting the mod needs has no live route, in which case the format is
decoded and the write is a documented, logged, single-owner operation.
