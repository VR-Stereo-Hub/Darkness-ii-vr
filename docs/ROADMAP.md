# Roadmap - the research gate and the ladder

The engineering route for the Darkness II VR mod. The four milestones on the Linear board are
the destinations; this file is the route. Each stage names its goal, **the engine question it
must answer before it commits**, the deliverable, and a falsifiable "done when". Boxes are
ticked in the session that measures them, with the run or build that did it.

Where this game differs from Dishonored, whose order this follows, it is said in the stage:
the Lua lane, CEG, gameswf instead of Scaleform, one first-person rig carrying the hands, the
body, both demon arms and four secondary tentacles under the camera bone, and two guns instead
of one weapon plus one power.

Conventions: "the mod module" is the DLL in the game process; "wrap" is an in-memory hook
that calls the original; "find the engine function" means locate by string, CTAB or SWIG-name
reference and byte-verify at load; "confirm by observation" means a logged measurement or a
simulator capture. Every render lever ships default OFF with a live A/B. Nothing numeric is
copied from a sibling.

---

## The research gate (R0-R7): answered before any stage past S0 commits

Each is a Research ticket that delivers a measurement and a written verdict in
`docs/darkness2/` (ENGINE_NOTES for engine facts, a `R<n>-<topic>.md` where the answer is
long). No stage that depends on a verdict starts until the verdict is written. They are
ordered by what they unblock; R0 and R1 gate everything, R2 is the biggest lever.

| ID | Question | Method | The verdict is written when |
|---|---|---|---|
| **R0** | Which loading route puts the mod module in-process before `Direct3DCreate9Ex` is called? | Try, in order: (a) `d3d9.dll` next to `DarknessII.exe` forwarding every export to `%SystemRoot%\System32\d3d9.dll` (a bare-name `LoadLibrary` resolves from the app directory first and d3d9 is not a KnownDLL, so this is the expected winner); (b) an `xinput1_3.dll` or `dinput8.dll` proxy (both delay-loaded) that wraps `LoadLibrary` + `GetProcAddress` and intercepts the D3D9 create; (c) a suspended-launch injector. Measure the exe's actual DLL search directory with loader snaps first: the game's own DLLs live under `Tools\`, so the app dir may not be what it looks like | The log shows `Direct3DCreate9Ex(SDK=32)` entered from our module in 10 of 10 normal Steam launches, with the Steam overlay still drawing. The verdict names the route and the fallback |
| **R1** | Do CEG's integrity checks tolerate in-memory code hooks, and where? | A canary 5-byte hook at three sites (a hot per-frame render function, a cold init function, the main tick) plus one call-site rewrite (the re-entry shape), all behind seam toggles. Play 30 minutes across a level load, a checkpoint reload and a menu round trip. Record whether the game exits, whether bytes revert, whether behaviour changes, and whether checks run once at startup or periodically | A 30-minute run with all four hooks live, no exit, no revert. The verdict lists any code ranges the mod must not touch and whether hooks must wait for the startup check |
| **R2** | Can the mod reach and execute Lua on the game thread, and what does SWIG expose? | Find `luaL_loadbuffer`, `lua_pcall`, `lua_gettop` by their Lua 5.1.3 strings; find the game's `lua_State*` by wrapping `lua_pcall` and recording the state; find the per-tick point on the game thread. Execute `camCtrl:SetBaseFovOverride(110)` (the shipped `SetFov.lua` shows the call). Walk the SWIG registration (`swig_types`, the per-class method tables) and dump every class and method name with its wrapper address | The FOV change is visible in a capture. `docs/darkness2/swig-api.md` lists every engine class and method reachable from Lua with byte-verified addresses (names only; no game content) |
| **R3** | Does `-console` open a console, and can the mod dispatch `Cmd*` commands? | Launch with `-console`; find the command dispatcher by string references to `CmdSetBaseFOV` and `CmdToggleFreeCamera`; fire `CmdToggleFreeCamera` from `command.txt`. Also: what `-log` writes and where | The free camera toggles from `command.txt`. The verdict names the dispatcher and whether it needs the console window |
| **R4** | Where is the projection built, and is the native 3D Vision path engine-side or driver-side? | Wrap `SetVertexShaderConstantF`; match CTAB registers for `SS_Projection`, `WorldViewProjection`, `SS_InverseViewProjection`, `SS_PrevViewProjection`; diff uploaded matrices against the camera object's FOV, near and aspect. Write FOV via Lua and confirm the constants change. Write an off-centre skew into the candidate field and observe whether the picture skews with the deferred lighting still aligned. Toggle `EnableNVidia3DVision` with and without an NVIDIA driver and record whether the engine draws twice or hands stereo to the driver | The verdict names the engine function that builds the projection, the field that feeds it, whether an asymmetric off-centre projection can be injected engine-side (lighting stays aligned), and rules the 3D Vision path in or out as a per-eye source |
| **R5** | Can the cache be read offline, and can the compiled FBX blobs be parsed? | Write the read-only extractor (`tools/cache/extract.py`): TOC magic `0x1867C64E` v16, 96-byte entries, LZF blocks. Extract every `.lua` (source embedded), `FPDJackie_skel.fbx`, the demon arm and DualPistol anims. Find the skeleton table in the skel blob by searching for the known bone-name strings | 47,425 paths listed; every `.lua` dumped as readable source into the gitignored `tools/lua/`; the skel blob's bone list parsed and matching `GAME_ASSETS.md`. The verdict states the blob layout as far as understood and what is still opaque |
| **R6** | What owns FP rig placement: the `GAME_C1_CAMERA` bone, `mFirstPersonViewOffset`, or a per-frame entity transform write? | Write `mFirstPersonViewOffset` by name-resolved property and watch the hands. Toggle `EnableFirstPersonEyeOffset`. Write the camera bone's world matrix after animation and watch. Log the FP entity's world transform write each frame and find the caller | The verdict names the one write that places the FP entity, and reports whether `GAME_x1_TENTACLE_CLAV` and the four secondary tentacle roots are children of `GAME_C1_CAMERA` or of `GAME_C1_ROOT` |
| **R7** | Where do animations start, evaluate and output? | Wrap the play-by-name request (R2's method list names it), log every start with its name, target entity and the `IV_*` inputs; find the AnimTree evaluate and the bone pose buffer before the palette build. Suppress `DemonArm_Idle_Left_03` by name and watch | The log shows named starts for idles, slashes, grabs, pistol fires; one suppressed idle is visibly absent. The verdict names the pose buffer and its coordinate space |

- [x] R0 verdict written (VR-231, 2026-09-25: the app-dir `d3d9.dll`; ENGINE_NOTES s9)
- [x] R1 verdict written (VR-232, 2026-09-25: four canaries, 33 min, no revert; ENGINE_NOTES s10)
- [ ] R2 verdict written and `swig-api.md` committed (2026-09-25: the map is committed, 177 classes / 1143 methods / 208 attributes, and the VM census is in ENGINE_NOTES s3; the in-game execution proof is still open, VR-233)
- [ ] R3 verdict written
- [ ] R4 verdict written
- [x] R5 extractor committed; verdict written (VR-236, 2026-09-25; GAME_ASSETS s8)
- [ ] R6 verdict written
- [ ] R7 verdict written

---

## S0 - Framework and foundation (M1)

- **Goal**: the mod module loads, logs, survives, and can be driven headset-free.
- **Engine question first**: R0 (the loading route) and R1 (hook survival) are verdicts.
- **Deliverable**: the proxy R0 picked, logging from `DllMain`, a crash handler with
  minidumps, `command.txt` polled at 1 Hz, `status.json`, CMake Win32 build with the 32-bit
  guard, `tools/lint.ps1` (exists), the simulated 32-bit OpenXR runtime presenting as a Quest
  3, the PowerShell harness (`build`, `install`, `launch-game`, `tail-log`, `xrsim-*`,
  `game-cmd`, `status-dump`).
- **Done when**:
  - [x] `launch-game.ps1` starts the game through Steam and the log shows the module entry
        and the D3D9 create (2026-09-25, every launch of the session)
  - [x] `status.json` updates every second (2026-09-25: 716 writes in a 12-minute stretch, `statusWrites` in the file)
  - [x] A forced crash writes a minidump with a readable stack and a crash file with the run
        identity (2026-09-25: `crash test`, a 28 MB dump, `read-dump.py` decodes the exception)
  - [x] `xrsim-selftest.ps1` passes (2026-09-25: PASS, 60 frames, FOCUSED) and the game reaches gameplay on the simulator (2026-09-25, VR-241: `xrsim-launch.ps1 -ViaSteam`, `boot.ps1`, the alley in both eyes)

## S0.5 - The OpenXR layer, the mono screen, the camera eyetest (M1)

- **Goal**: the game frame arrives in both eyes of the simulator; we know which camera write
  the renderer honours.
- **Engine questions first**: Ex or non-Ex device (log both create paths and the
  `CreateDevice(Ex)` parameters: backbuffer format, multisample, presentation interval);
  `Present` or `PresentEx` and from which thread; the honoured FOV write (Lua
  `SetBaseFovOverride` vs `mHorizontalFOVHack` vs the camera object field vs the CTAB constant
  itself).
- **Deliverable**: the runtime layer adopted from Dishonored (a D3D11 device on the runtime's
  adapter LUID; the D3D9 backbuffer copied to a shared D3D11 texture), rung 1 "mono screen"
  (a head-locked quad, both eyes), the `camera eyetest` instrument that writes each candidate
  in turn and reports which one changes the uploaded projection.
- **Done when**:
  - [x] Both simulator eyes receive frames at the game's frame rate for 5 minutes (2026-09-25, VR-241: `xrsim-soak.ps1`, 24,425 presents = 24,425 submits, 24,404 layered by the sim, FOCUSED; ENGINE_NOTES s11)
  - [ ] `camera eyetest` prints exactly one HONOURED write with the measured FOV delta
  - [x] The device path verdict (Ex vs non-Ex, present call and thread) is in ENGINE_NOTES (2026-09-25, s2: `Direct3DCreate9Ex`, `CreateDeviceEx`, `PresentEx` from the main thread)

## S1 - Mono screen in a headset; head tracking; positional; render size (M1 / M2)

- **Goal**: seated play of the flat game on a stable head-locked quad, then the camera
  follows the head.
- **Engine questions first**: which rotation input is honoured on the tick (the view-rotation
  setter the stick feeds, so yaw composes as a delta; pitch absolute); where the engine
  applies a resolution (its own video-settings apply function, because the config is
  obfuscated and the mod does not write it); whether `CameraNearPlane` can be lowered for
  hands close to the face; the world unit (a tooltip says metres; confirm).
- **Deliverable**: the mono screen accepted in a headset; head rotation on the game thread
  (yaw delta, pitch absolute); positional tracking on the camera position (crouch, lean,
  roomscale) with a neck-pivot cancel; the render size requested through the engine's own
  settings path; **the FOV set from the headset's own projection, live**, with the submitted
  claim always equal to the rendered FOV (the Dishonored FOV-feedback traps: a readback that
  fed itself shrank gameplay after a load).
- **Darkness difference**: the Lua lane is available for one-shot config (FOV, near plane),
  but per-tick rotation is written at the engine tick from the mod, not through Lua, to keep
  latency at one tick.
- **Done when**:
  - [x] Headset run: the game on the mono screen in both eyes, comfortable for the first
        chapter (M1 closes here) (2026-09-25, VR-243: accepted in the headset on VDXR with the
        installed geometry, 2.4 m wide at 1.75 m, head-locked, build 01eed75; ENGINE_NOTES s11)
  - [ ] A +90 degree headset yaw gives a +90 degree view with the stick still adding on top;
        latency one tick, logged
  - [ ] A 30 cm lean moves the camera 30 cm in-world, measured against a doorway of known
        width; pivot error under 2 cm
  - [ ] The requested render size equals the size the swap chain reports
  - [ ] The rendered FOV read from the projection constant equals the runtime's within 0.5
        degrees, and returns to the same value after a load, a cinematic, a zoom and a death

## S2 - True stereo by SequentialReentry (M2)

- **Goal**: two correct eye images per tick from the engine's own scene draw.
- **Engine questions first**: which function is the scene-draw root (identify by making it
  MOVE: wrap candidates and offset the camera during one only); whether it has one call site;
  how `SS_PrevViewProjection` is fed (motion blur or reprojection will see the other eye's
  matrix unless the mod keeps per-eye previous matrices or turns blur off); how the cel
  outline and deferred lighting passes read `SS_InverseViewProjection` (per-eye consistent
  only if the projection is written engine-side, per R4); whether the 3D Vision path
  contributes anything (R4's verdict).
- **Deliverable**: the single call site patched to draw twice per tick with the other eye's
  camera; eye tags through a ring paired by the per-present camera step; the swapped-eye fix;
  the cadence beat (a render size that makes one tick one display period at 90 Hz); the
  judder fix (submit the pose the image was rendered with); world scale and IPD; **render
  quality from 50 to 200 percent of the judged size, changed live in F10 and set in the
  launcher**, with the FOV following.
- **Done when**:
  - [ ] A vertical edge at 2 m shows the measured disparity for the runtime's IPD within 5
        percent
  - [ ] The frame-time histogram shows one tick per display period with under 1 percent
        misses on the test scene at 90 Hz
  - [ ] A fixed target under head rotation shows no double image
  - [ ] Deferred lights and cel outlines register in both eyes with no offset
  - [ ] Headset: fusion confirmed, one full level called comfortable
  - [ ] A render-quality change from 100 to 70 percent takes effect without a restart, with
        the three size lines agreeing and the FOV readback unchanged

## S3 - Motion controls (M3)

- **Goal**: hands and guns on the controllers, the head decoupled from the body.
- **Engine questions first**: which yaw INPUT feeds body rotation (write there, not to the
  result, so head and body separate); the FP entity anchor (R6); the pose buffer write point
  (R7); the pre-spawn fire seam per hand; what `mDualWieldingAimViewOffset` does once the
  view no longer defines the aim.
- **Deliverable, in this order and no other**:
  1. The XInput bridge so the OpenXR controllers appear as the pad the game already
     understands (locomotion, menus, everything works on day one).
  2. Head/body decouple at the yaw input.
  3. Floating hands: `JackieHands` cut from `JackieBody` by bone influence at `CLAV1` / `ARM1`.
  4. Hands at the controllers, first through a bone palette correction, then through the
     engine-side pose write once A3 lands.
  5. Each gun on its own controller, matched to engine components through the coordinate
     bridge (`DUAL_WIELD.md` W1).
  6. One ray per hand (W2-W5).
  7. The tentacles re-anchored and targeted (`TENTACLES.md` T2-T7).
  8. Hands scaled independently.
- **Done when**:
  - [ ] A controller moved 20 cm moves its hand 20 cm in-world within one tick
  - [ ] A shot from each hand lands where that hand's ray hits (decal within 1 cm of the debug
        ray at 5 m), while the other hand fires
  - [ ] Turning the head 90 degrees leaves the body and the guns where they were
  - [ ] Both demon arms pass T2's shoulder measurement

## S4 - Animation and model control layers (M3, alongside S3)

- **Goal**: the layers everything above sits on, built as layers so no later feature hits a
  wall. `docs/darkness2/ANIM_AND_MODEL_CONTROL.md` has each layer's done-when.
- **Deliverable**: A0 read, A1 mask, A2 trigger, A3 pose override, A4 blend-back, A5 root
  retarget; M0 enumerate, M1 per-part visibility, M2 scale, M3 re-anchor. (A6, A7, M4, M5, M6
  are M4 work.)
- **Done when**:
  - [ ] A0: a named animation log for a full level
  - [ ] A1: a suppressed anim never starts and its effect is visibly absent
  - [ ] A2: `anim play DemonArm_Slash_LeftUp` visibly plays
  - [ ] A3: identity written to `ARM2` straightens the arm and a gun on `WEAPON1` follows
  - [ ] A4: control returns within one tick of the logged end event across all 14 finishers
  - [ ] A5: the idle plays identically from the shoulder anchor with the head turned
  - [ ] M0-M3: the FP rig enumerated; `JackieBody` hidden leaves floating hands; 0.9 hand
        scale measured on the palm; a part re-parented to the shoulder (same as T2)

## S5 - Stability (M2 / M3)

- **Goal**: no flicker, no wrong state, no one-frame jumps.
- **Engine questions first**: menu identity (which gameswf movie is up); gameplay state flags
  through name-resolved properties (in darkness, dual wielding, executing, in cinematic, dead,
  loading); Bink playback state.
- **Deliverable**: the startup and load flicker fixed; menu and loading identity; state flags
  in `status.json`; animation hand-back during full-body moves (executions, mantles, ledges);
  a one-frame-jump detector.
- **Done when**:
  - [ ] A 20-minute run with the jump detector shows zero camera jumps over 5 cm per tick
        outside teleports
  - [ ] Every menu and loading screen is classified in `status.json`
  - [ ] Flags flip within one tick of the observed event

## S6 - Cinematics and comfort (M2 for comfort; M3 for hand-back)

- **Goal**: the frequent first-person cutscenes are comfortable and the noir look survives.
- **Engine questions first**: which `_cin` anims drive `GAME_C1_CAMERA`, and whether the
  camera object copies from the bone or the bone from the camera; where shake, bob and sway
  are applied (`mWalkBobDepth`, `mCameraShakeStrength`, `mSwayAmplitude`,
  `mFPEntityIgnoresShakeTranslationAndRotation`).
- **Deliverable**: a mono quad for menus, loading and Bink, anchored where the player looks;
  cinematic head-look (position follows the animated camera, orientation is the HMD over a
  damped upright base); upright pitch and roll in cinematics; shake, bob and sway zeroed at
  the source through name-resolved properties, each with an A/B; the collision-pop glide off;
  a 120 Hz world-smoothness option.
- **Done when**:
  - [ ] In the opening restaurant sequence the horizon stays within 2 degrees of level for
        the whole scene while the head looks freely
  - [ ] Shake amplitude on the camera position during a nearby explosion is zero with the
        lever on and non-zero with it off
  - [ ] Every menu, loading screen and Bink video shows on the quad with no flicker at the
        transition

## S7 - The HUD on anchors (M4)

- **Goal**: the flat HUD moves off the head-locked plane into anchors.
- **Engine questions first**: the gameswf draw signature (vertex declaration, shader or
  fixed-function state); how to name a draw (wrap the movie load and tag the render calls
  with the movie name, from the 61 `.swf`).
- **Deliverable**: gameswf draws recognised and redirected to per-element anchors
  (head-locked window, world, wrist L/R); per-hand reticles on each hand's ray; a darkness /
  light indicator near the tentacle shoulders; weapon and ammo on the wrists; the heart
  count on the left wrist; subtitles head-locked low.
- **Done when**:
  - [ ] Every one of the 61 movies is classified to an anchor or explicitly left head-locked,
        in a table in `docs/darkness2/HUD.md`
  - [ ] Each hand's reticle sits on that hand's decal within 1 cm at 5 m

## S8 - Gestures and polish (M4)

- **Deliverable**: slash by controller swing (T5), grab by ray (T6), eat the heart by bringing
  the grabbed enemy to the face, executions as hand-back (T8), physical crouch, snap turn,
  the F10 ImGui panel driven from the controllers with Basic / Advanced / Debug tiers, hand
  adjust, the weapon mirror for one-sided models, the tentacle hero pass (T10), the loose
  file and Lua overrides (A6, A7, M4).
- **Done when**: each item's row in `docs/POLISH.md` is closed.

## S9 - Shipping (M4)

- **Deliverable**: the installer and launcher, `package.ps1` (refuses a dirty tree), the
  support log collector, GitHub Releases, 1.0.0.
- **Done when**:
  - [ ] A clean-machine install runs the game with the mod in one click
  - [ ] The collector bundles the log, minidumps, `status.json` and the verify results,
        bounded in size
  - [ ] The shipped ini is a byte copy of the headset-tested machine's ini

---

## Session records

New entries are prepended here, newest first, as ticket checklists (the Dishonored shape:
`## <title> (VR-<n>, <date>)` with checkboxes), so the top of this file is always the current
work and the ladder above is the reference.

## The loading route, the framework floor, the R1 canaries, the extractor (VR-231, VR-239, VR-232, VR-236, 2026-09-25)

- [x] R0 measured: the app-dir `d3d9.dll` is loaded by the exe's bare-name `LoadLibraryA`
      0.5 s after process creation, in 2 of 2 launches; `Direct3DCreate9Ex(SDK=32)` entered
      from our module; the Steam overlay loaded (ENGINE_NOTES s9)
- [x] The framework floor: proxy (23 exports), log (ten deep), crash handler (run identity,
      minidump), command seam at 1 Hz, `status.json`, D3D9 vtable hooks and the present tick,
      backbuffer capture, the in-process input lane, ini + golden, build fingerprint
- [x] The harness: build, install (full ini diff), launch through Steam, boot to gameplay,
      game-cmd, game-key, game-shot, status-dump, module-census, quit-game, soak, log-parse,
      exports-check, disasm-rva, pe-xref, read-dump, ini-golden
- [x] The simulator ported and self-tested (`d2vr-xrsim`, Quest 3, 60 frames, FOCUSED); the
      game on it waits for VR-241
- [x] R1 measured: four byte-verified canaries live for 33 minutes across a menu round trip,
      a checkpoint reload and a level restart; 0 reverts, 0 exceptions, clean quit
      (ENGINE_NOTES s10)
- [x] R5: `tools/cache/extract.py` lists 47,425 paths, dumps 430 Lua scripts as source,
      parses both skeleton tables; the camera and tentacle roots are children of the root bone
      (GAME_ASSETS s5, s8)
- [x] The harness rule changed: the session drives the game (CLAUDE.md, TESTING, VERIFICATION,
      the decision log)
- [ ] The PR against `staging` opened with `Fixes VR-231, VR-239, VR-232, VR-236`; the user
      merges

## Docs set and the Linear flow (VR-230, 2026-09-25)

- [x] The game surveyed read-only: PE identity, imports, engine strings, the cache format,
      the FP rig and demon arm assets (`docs/darkness2/`)
- [x] The sibling mods surveyed: docs, flow, feature order, traps (`docs/TRAPS.md`,
      `docs/LESSONS_FROM_SIBLING_MODS.md`)
- [x] The board shaped: five new `area:` labels, the project spec, four milestones
- [x] The docs set written; `tools/lint.ps1` passes
- [x] The backlog filed (51 tickets, VR-230 to VR-281 except VR-260) and the first project
      update posted
- [ ] The PR against `staging` opened with `Fixes VR-230`; the user's UI checklist handed back
