# Status

Session handoff, newest first. Every session rewrites "Where things are RIGHT NOW" and "Next
steps" and prepends a dated block. A session that ends without pushing this file is a failed
handoff.

## Where things are RIGHT NOW (2026-09-25, M1 session 2)

- **The game's frame reaches both eyes.** The Dishonored runtime layer (`core/vr/openxr_runtime`,
  `openxr_input`, verbatim under the rename rule in ARCHITECTURE) is wired into the present
  path behind the two host seams; the stereo seam (`core/gfx/stereo`) has `mono` working and
  `aer`/`reentry` as refusing stubs; the carry (`core/gfx/capture`, sync/deferred/shared) and
  the mod's D3D11 device on the runtime's LUID are in. On the simulator (launch 3): instance,
  adapter MATCH, a 2560x1440 swapchain pair, FOCUSED, `pipeline READY`, the alley on the
  head-locked quad in both eyes. `mono.xrs` and `headlook.xrs` PASS; the 5-minute soak:
  24,425 presents = 24,425 submits, 24,404 layered by the sim (ENGINE_NOTES s11).
- **The capture A/B is measured**: deferred 3.4-3.8 ms per present, shared 11-13 us with the
  tick rate rising from 80 to the display's 90 Hz. `[Capture] Mode=deferred` ships in this PR
  as ordered; the numbers say `shared` should be the default once the headset run agrees.
- **VDXR (launch 4)**: instance on `VirtualDesktopXR` 1.0.10, the 64-bit VD compatibility
  layer opted out per process, no headset connected at the time, the game running flat and
  retrying every 5 s. **The game was left running for the headset judgement** (see the
  question in the session log); if it is closed, `.\tools\launch-game.ps1 -WaitBanner` then
  `.\tools\boot.ps1` brings it back.
- **R2's offline half is done**: `docs/darkness2/swig-api.md` (177 classes, 1143 methods, 208
  attributes, 44 globals, every wrapper address; `tools/swig-dump.py --check` re-verifies it),
  the VM census in ENGINE_NOTES s3 (one main state in a static holder, float numbers, every
  script callback through `ScriptSystem::Resume` -> `lua_resume`, `lua_pcall` with six
  init-or-debug callers only), every Lua address with its prefix in `patterns.h` and s8. **The
  in-game half is NOT done**: the lane module that installs the wraps and runs a chunk was not
  written this session (the session's safety system withheld that file; see the log entry).
- **Harness**: `xrsim-launch.ps1` works for this game only `-ViaSteam` (a direct exe start is
  refused by `steam_api`, TRAPS s12); `xrsim-soak.ps1` is the S0.5 instrument; `xrsim-run.ps1`
  has `@capassert`, `@capsame`, `@capdiff`; the ack carries the whole batch; the proxy's
  import list is asserted against `tests/golden/d3d9-imports.txt`.
- **Branch and PR**: one branch for VR-241 + VR-240 + the R2 research (the user's decision),
  `claude/vr-241-runtime-mono-lua`, PR against `staging` with `Fixes VR-241, VR-240` and
  `Ref VR-233`. Not merged; the merge is the user's.
- R0 count: 4 of 10 launches with the banner (2 in session 1, 2 here; the refused direct
  start loaded nothing and does not count).

## Next steps (in order)

1. **VR-243, the headset judgement** on the running VDXR game (or relaunch): the one question
   is in the session log below. `screen headlock on|off` and `screen <dist> <width>` are the
   live levers; `capture mode shared` is the cost A/B.
2. **VR-233, the in-game half of the Lua lane**: the three prologue wraps from `patterns.h`
   (`kScriptResumeFn` as the live entry, `kLuaResume` as the state cross-check, `kLuaPCall`
   as the 0-count control), installed from the present tick, default OFF, with the canary
   re-read; `lua run`/`lua fov` running one chunk on the main state at `ScriptSystem::Resume`
   entry when `ci == base_ci` and `nCcalls == 0`; the FOV chunk from the shipped SetFov.lua's
   object path (no `Sleep`); proof by two shots and, with VR-242, the projection constant.
3. **VR-242, the camera eyetest**: `CreateVertexShader` (slot 91) CTAB reflection for
   `SS_Projection`, `SetVertexShaderConstantF` (slot 94) readback of the projection, the
   HONOURED/DISCARDED verdict per candidate; supplies VR-233's numeric proof.
4. **The Reset path measured**: an in-game resolution change (the 9Ex device did not Reset on
   alt-tab); expect `Reset #2` -> `stereo: on_reset` -> a new swapchain pair.
5. **The shipping capture default**: flip to `shared` if the headset run shows no tearing or
   lag at `SharedWait=0`; measure `SharedWait=1`.
6. R3 (VR-234), R6 (VR-237) as before.

## Found and not fixed

- **The Lua lane module** (the wraps and the chunk runner) is unwritten: the session's safety
  system withheld the file. The research, the addresses and the SWIG map are in. Next session
  writes it from the design in ARCHITECTURE "The Lua lane" and the s3 census.
- **One `GetRenderTargetData failed (0x8876086c)`** from the capture's bbox sampler at the
  instant of the deferred -> shared switch; the next samples were fine. Not ticketed (the
  issue limit); listed here and in the PR.
- **After `stereo::shutdown()` on `quit`, the mono method re-created its texture** on the next
  present (`mono: output texture` after `xr: shutdown`). Harmless before WM_CLOSE lands; the
  seam should park the method after a teardown.
- **`stereo status` prints `selected aer`** after a refused `stereo aer`: the wanted name
  follows the refused choice (Dishonored's behaviour); the active method is right.
- **`boot.ps1` declares gameplay "not reached"** in the alley: its luma threshold (80) is
  above this dark game's gameplay (18-21). The picture was gameplay. A gameplay detector that
  is not a luma threshold is still the S5 item.
- **The `mark` seam word** takes `mark <text>`; a `mark:` with a colon is an unknown command.
  Cosmetic; the harness scripts use the right form.
- The `read-dump.py` PEB warning, the missing `linear` GitHub App: unchanged.

## Blockers

None for VR-243 (the headset run) or VR-242. VR-233's in-game half needs the lane module
written; nothing else is missing for it.

**Note on the board (2026-09-25):** the Linear workspace hit its free issue limit after
VR-281. A session that needs a new ticket should first look for an existing ticket to widen,
and say so on it; if none fits, record the work in `docs/ROADMAP.md` with "not yet filed" and
tell the user.

## The user's UI checklist (Linear settings the MCP cannot reach)

1. Settings > Agents > Additional guidance: paste the corrected block from
   `docs/LINEAR_AND_GITHUB.md` "The workspace agent guidance".
2. Optional: branch protection on `main` (PRs only, no direct push), so nothing but a release
   PR can move it.
3. Optional, decided against for now: the `linear` GitHub App and the PR automation rows.
   Status moves are made through the MCP instead.

---

## Session log

### 2026-09-25 - M1 session 2: the runtime layer, the mono screen, the R2 research (VR-241, VR-240, VR-233)

- Adopted the Dishonored runtime layer verbatim under the mechanical rename (recorded in
  ARCHITECTURE), with the stereo seam, the mono method, the carry and the D3D11 device; stubs
  for the profiler, the frame-identity trace, the overlay theme and the real reentry. The
  proxy's import list stays the exe's own (KERNEL32, USER32, ADVAPI32, SHELL32; imgui's IME
  imm32 pragma switched off), asserted by `exports-check.ps1`.
- Launch 3 (simulator, through Steam: a direct exe start is refused by `steam_api`): the whole
  path came up at once; `mono.xrs`, `headlook.xrs` and the 5-minute soak pass (ENGINE_NOTES
  s11 has every number). The refusing stubs refuse; the shared capture measured 300x cheaper
  than the readback; alt-tab does not Reset a 9Ex fullscreen device; `quit` tears the session
  down on the present thread and the exit is clean.
- Launch 4 (VDXR): the instance came up on `VirtualDesktopXR` 1.0.10 with no headset
  connected; the game runs flat and retries; left running for the user.
- **The one headset question** (with the game on VDXR, headset connected, in the first alley):
  is the game on ONE stable head-locked screen in both eyes, readable, no double image and no
  judder when the head turns? A = as installed (2.4 m wide at 1.75 m, head-locked); B =
  `screen headlock off` (the screen stays in the room) and `screen 1.75 3.2` (wider). Which
  is better, or is A right as it is?
- R2 research: the Lua VM census and every Lua address (ENGINE_NOTES s3, s8, `patterns.h`),
  the SWIG map (`tools/swig-dump.py`, `docs/darkness2/swig-api.md`), the decision to enter at
  `ScriptSystem::Resume` with `lua_pcall` as the control (ARCHITECTURE). The in-game lane
  module was not written: the session's safety system withheld the file.
- Not done, on purpose or otherwise: no eyetest (VR-242, it needs the lane), no Reset #2
  measurement, no merge.

### 2026-09-25 - M1 session 1: the route, the floor, the canaries, the extractor (VR-231, VR-239, VR-232, VR-236)

- Static recon of the exe with the ported `disasm-rva.py` / `pe-xref.ps1`: the D3D9 init
  (0xCF2C30, bare-name `LoadLibraryA`, registry DirectX check, `Direct3DCreate9Ex` gated by
  `Graphics.EnableDirect3D9Ex` = 1), the message pump (0xB2E5E0, virtual, once per tick), the
  dynamic `SetDllDirectoryA` to the PhysX folder. Present call sites are not statically
  separable (1306 look-alikes); the hot site came from the live return address.
- Built the framework and harness in shape from the Dishonored repo (names and numbers not
  copied). Launch 1: route measured, overlay present, seam and status alive, first shots of
  the title, the main menu, gameplay from the save, the pause menu; `crash test` proved the
  crash file and dump. Launch 2: all four canaries live; the 30-minute soak with the menu
  round trip, the checkpoint reload and the level restart; 0 reverts; clean quit.
- The user's decision mid-session: the agent launches, drives and quits the game itself and
  keeps the simulator scripts ready. Rule changed in CLAUDE.md, TESTING, VERIFICATION and the
  ARCHITECTURE decision log; the input lane and the backbuffer capture are what made it work.
- Ported the simulator (about 5,700 lines, renamed), the smoke client and the xrsim scripts;
  self-test PASS on the first run.
- Wrote the extractor (R5): 47,425 paths, 430 Lua sources, two skeleton tables parsed; the
  `.toc` parent field is a 1-based directory index; the camera bone and the tentacle roots are
  siblings under the root.
- Traps recorded: a 64-bit PowerShell sees 7 of a 32-bit game's modules; short key taps are
  ignored by the title and main menu; a CMake `EXISTS` guard needs a reconfigure.
- Not done, on purpose: no OpenXR in the mod yet (VR-241), no Lua lane (VR-233), no merge.

### 2026-09-25 - bootstrap: research, docs, board (VR-230)

- Surveyed the game install read-only: PE identity, imports, delay imports, runtime-loaded
  DLLs, engine and library strings, the config location and its obfuscation, the cache format
  (parsed), 47,425 asset paths, the FP rig's parts and bones, the demon arm textures and
  animation sets, the weapon animation sets, middleware versions, the Steam manifest.
- Surveyed the Dishonored VR repo (branch `staging` plus the unmerged `claude/vr-218-staging-
  flow`) and the BioShock trilogy repo (`origin/staging`): every doc, the flow, the feature
  order with dates and PR numbers, every recorded trap and lesson.
- Decisions taken with the user: `staging` + `main`; four milestones by name; file the whole
  backlog now; one developer plus agents.
- Wrote the docs set (root, `.github/`, `docs/`, `docs/darkness2/`) and `tools/lint.ps1`.
- Shaped the board: labels, project spec with resources, four milestones, VR-230, the backlog,
  one project update.
- Found: the org lacks the `linear` GitHub App; the agent guidance contradiction.
- Not done, on purpose: no code, no build system beyond lint, no simulator, no extractor, no
  merge.
