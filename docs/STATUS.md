# Status

Session handoff, newest first. Every session rewrites "Where things are RIGHT NOW" and "Next
steps" and prepends a dated block. A session that ends without pushing this file is a failed
handoff.

## Where things are RIGHT NOW (2026-09-25, M1 session 3 end)

- **The Lua lane runs chunks inside the game's VM** (VR-233, the in-game half): three
  byte-verified prologue wraps (`ScriptSystem::Resume`, `lua_resume`, `lua_pcall`) installed
  from the first present, default OFF (`[Lua] Enabled`), re-read every LogEverySeconds. The
  safe call point is the entry of `ScriptSystem::Resume` on the game thread, which IS the
  present thread; the main state's `l_G` matched on 110,664 resumes with 0 mismatches; the
  `lua_pcall` control is flat in play (the engine uses it at transitions only, from one call
  site); `lua fov 110` ran at the next Resume with 0 deferrals and read back through
  `GetBaseFovOverride`; `lua swigcheck` 10 of 10. The R2 verdict is ENGINE_NOTES s3.
- **The rendered FOV is a number** (VR-242): the projection watch reads `WorldViewProjection`
  back at every upload (device slots 91/92/94, the CTAB parsed by name, P00/P11 recovered
  from the column norms of a rigid W*V*P) and votes per present. The alley is V = 45.00 deg,
  H = 72.73, 16:9. **`SetBaseFovOverride` through the lane is HONOURED as a vertical angle**
  (60 -> 60.00, 80 -> 80.00, 0 -> 45.00, exact), lerped by the engine over about two seconds.
  The negative control (`camera eyetest nowrite`) prints DISCARDED on both asks. The
  instrument's own HONOURED line is still open: its fixed settle judged the lerp; the settle
  is adaptive in build 2F98D983, which is built but NOT installed (the game was running).
  `camera ctab <deg>` is the direct lever (92,720 constants rewritten, the shot wider).
  ENGINE_NOTES s12.
- **The F10 panel shows in both eyes** (Ref VR-275, the simple form): one ImGui window in the
  eye texture (DX11 backend only, D3DCompile forwarded, no new import), F10 or `overlay on`,
  hidden by default; the stereo method, the capture mode with its cost, the screen geometry,
  the Lua lane, the canaries and the OpenXR line. `panel.xrs` passes (non-black and luma rise
  equally in both eyes and fall back on close); the eye capture shows every section.
- **The game is running on VDXR** (launch 8, build 893D63DD installed, no headset connected
  at the time, retrying quietly) in the alley with the lane and the watch ON, for the headset
  A/B below. If it is closed: `.\tools\install.ps1 -Release -Set 'Canary.Tick=1','Canary.LogEverySeconds=5','Lua.Enabled=1'`
  (this also installs 2F98D983), `.\tools\launch-game.ps1 -WaitBanner`, `.\tools\boot.ps1`.
- **Branch and PR**: `claude/vr-233-lua-lane-ingame`, PR #4 against `staging` with `Fixes
  VR-233`, `Ref VR-275`, `Ref VR-242`. Not merged; the merge is the user's. VR-233 is In Review
  with the PR attached; VR-242 In Progress (its instrument line is open); VR-275 widened by a
  comment. R0 count: 8 of 10 launches with the banner (2 + 2 + 4 this session).

## Next steps (in order)

1. **The headset A/B (the one question)** on the running VDXR game: A = `capture mode
   deferred` (as shipped), B = `capture mode shared` (the panel's capture radio or the seam).
   Is B free of tearing, lag or a stale frame when the head turns? If yes, flip `[Capture]
   Mode=shared` in the ini literal and the golden in the next PR.
2. **VR-242, close the instrument**: install 2F98D983, `camera eyetest all` in the alley
   standing still; expect HONOURED as `vert` on both asks with the settle length logged (the
   engine's FOV smoothing time), then tick the S0.5 box and move the ticket to In Review.
3. **The Reset path**: an in-game resolution change through the options menu with the input
   lane (a 9Ex fullscreen device does not Reset on alt-tab); expect `Reset #2` ->
   `stereo: on_reset` -> a new swapchain pair. Not done this session (no launch left for it).
4. **S1 head tracking on the mono screen** (VR-244): the FOV lever is the lane's
   `SetBaseFovOverride` (vertical degrees) with the watch as its sensor; rotation goes on the
   engine tick, not through Lua (ARCHITECTURE "The camera seam").
5. R3 (VR-234) `-console` and `Cmd*` dispatch: the lane can now call by name, so the console
   question may be answered from Lua first.
6. R6 (VR-237), the runtime half; the tiered panel (VR-275) when the controller input exists.

## Found and not fixed

- **The eyetest instrument judged a lerp** (a fixed 30-present settle; the engine smooths a
  base-FOV change over about two seconds). Fixed in the tree (adaptive settle, the timeline
  log), unmeasured: the HONOURED line is next launch's. Ticket VR-242 stays open for it.
- **The projection watch's change line printed only the first step of a lerp** (it compared
  against the previous present, not the last logged value). Fixed in 2F98D983, unmeasured.
- **The log banner's "built" time is the proxy translation unit's compile time**, two builds
  stale when only new files change; the sha256 the install prints is the identity (TRAPS s12).
- **`boot.ps1` still calls the alley "not reached"** (luma 18-23 against its 80 threshold);
  the picture is gameplay every time. The gameplay detector that is not a luma threshold is
  still the S5 item.
- **The panel's mouse is the desktop cursor and the game keeps reading it** while the panel is
  open (the view can spin while pointing). The controller pointer is VR-275's work; the seam
  words drive every control the panel shows.
- **The `ctab` lever is judged by the picture only**: the watch reads the upload before the
  substitution by design. A second sensor (a readback of the substituted constant, or the
  capture bbox) would make it a number. Not filed (the issue limit); noted on VR-242.
- **`stereo status` after a refused choice**, the `mono` texture after `quit`, the bbox
  sampler's one failed `GetRenderTargetData`, the PEB warning, the missing GitHub App: as
  before.

## Blockers

None. The headset A/B needs the user in the headset; everything else runs on the simulator.

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

### 2026-09-25 - M1 session 3: the Lua lane in-game, the first F10 panel, the projection watch and the FOV verdict (VR-233, VR-275, VR-242)

- Started from `origin/staging` 3b5f878 (PR #3 merged); ticked S1 box 1 with the VR-243
  verdict (the mono screen accepted in the headset, build 01eed75).
- Built the Lua lane as designed: the three wraps, the mailbox, the chunk runner on the main
  state, the FOV chunk from the shipped script's object path (read locally with the R5
  extractor, never committed), `lua status|on|off|run|fov|swigcheck`, the status.json object.
  Launch 5 (simulator): live from the first present, the game thread is the present thread,
  l_G matched on every resume, the control flat in play (48 engine pcalls at transitions from
  one site, 0xBBA876), `lua fov 110` ran with 0 deferrals and the shot is visibly wider, 10 of
  10 SWIG modules verified. The R2 verdict is written (ENGINE_NOTES s3, the ROADMAP box).
- Built the simple F10 panel with ImGui's DX11 backend alone and a `D3DCompile` forwarder
  (the import list stays the exe's; `exports-check.ps1` asserts it), `IM_ASSERT` to the log,
  `overlay on|off|toggle|status`, F10 on the present thread, the game section (the lane and
  the canaries). Launch 6: the panel drawn into the 2560x1440 eye texture, visible in the sim's
  left-eye capture with every section; the first assertion (non-black past 30) was wrong for
  a dark window; `@capchange` added to `xrsim-run.ps1`; launch 7: `panel.xrs` passes.
- Built the projection watch (slots 91/92/94, the CTAB parse) and the eyetest. Launch 6: 0 of
  81 shaders name `SS_Projection`; a checkpoint reload creates no shaders. Launch 7 (the lazy
  `GetFunction` read, `camera names on`): the shaders take `WorldViewProjection` at c0 and
  never a pure projection; the FOV is recovered from the column norms of a rigid W*V*P (the
  Dishonored method). Launch 8 (VDXR, flat): V = 45.00 in the alley; `SetBaseFovOverride`
  HONOURED as a vertical angle (60.00, 80.00, back to 45.00, exact); the engine lerps over
  about two seconds, which the instrument's fixed settle misjudged (MOVED-UNPREDICTED, then
  INVALID); the negative control DISCARDED on both asks; `camera ctab 90` rewrote 92,720
  constants with a wider shot. The settle is adaptive and the timeline log fixed in 2F98D983
  (built, not installed: the game runs for the headset A/B).
- Docs: ENGINE_NOTES s3 (the R2 verdict), s8 (the SWIG module table, the read-back wrapper,
  the live pcall caller), s12 (the constant inventory, the decomposition, the FOV table);
  ARCHITECTURE decisions (the pcall control by return address and the returned result; the
  DX11-only panel and the forwarder); TRAPS s12 (the banner time); VERIFICATION rows for
  `lua`, `camera`, `overlay`.
- Not done, on purpose or otherwise: the instrument's own HONOURED line (next launch), Reset
  #2 (no launch left), the capture default flip (after the headset A/B), no merge.

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
