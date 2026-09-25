# Status

Session handoff, newest first. Every session rewrites "Where things are RIGHT NOW" and "Next
steps" and prepends a dated block. A session that ends without pushing this file is a failed
handoff.

## Where things are RIGHT NOW (2026-09-25, M1 session 1)

- **The mod module exists and runs in the game.** `d3d9.dll` next to `DarknessII.exe` (R0
  route (a), measured in 2 of 2 Steam launches: the exe's bare-name `LoadLibraryA("D3D9.DLL")`
  takes it from the exe's directory 0.5 s after process creation, even with `SetDllDirectory`
  pointing at the PhysX folder). `Direct3DCreate9Ex(SDK=32)` enters from our module; the game
  is a **D3D9Ex host** (`CreateDeviceEx`, one `Reset` to 2560x1440 fullscreen, `PresentEx`
  from the main thread). The Steam overlay loads beside us. `docs/darkness2/ENGINE_NOTES.md`
  s2, s9.
- **The framework floor** (VR-239) is in: logging from DllMain with ten-deep rotation, the
  crash handler (run identity in `darkness2_vr_crash.txt`, one minidump per run; proven with
  `crash test`), the command seam at 1 Hz with `ack.txt`, `status.json` at 1 Hz, the D3D9
  vtable hooks and the present tick, the backbuffer capture (`shot`), the in-process input lane
  (`key`, `mouse`, `type`, `focus`, `quit`), `darkness2_vr.ini` with its golden and the install
  diff, the build fingerprint that refuses code hooks on a wrong exe, the kill switch.
- **R1 is measured** (VR-232): four byte-verified canaries (cold init, per-tick pump, a
  rewritten call site, the present wrapper) lived 33 minutes across a menu round trip, a
  checkpoint reload and a level restart with **0 byte reverts, 0 exceptions, a clean quit**.
  Hooks installed from the first present survive. ENGINE_NOTES s10.
- **R5 is done** (VR-236): `tools/cache/extract.py` lists the 47,425 paths, dumps the 430 Lua
  scripts as readable source (the Lua chunk's source-name field holds the text), parses the
  skin bone table and the hierarchy table of a `_skel.fbx`. In the shipped rig data the camera
  bone and both tentacle roots are children of `GAME_C1_ROOT`, not of the camera (half of R6).
  `docs/darkness2/GAME_ASSETS.md` s5, s8.
- **The session drives the game** (the user's decision this session): `launch-game.ps1`,
  `boot.ps1` (title -> menu -> Continue -> gameplay, screenshot-checked), `game-cmd.ps1`,
  `game-key.ps1`, `game-shot.ps1`, `quit-game.ps1`, `soak.ps1`. The "never launch" rule is
  retired in CLAUDE.md, TESTING and VERIFICATION. Headset judgements stay the user's.
- **The simulator is ported and healthy**: `d2vr_xrsim32.dll` (Quest 3 identity, 90 Hz,
  head/hand/button commands, per-eye capture), `xr_hello32` smoke client, the `xrsim-*.ps1`
  scripts and `smoke.xrs`, `mono.xrs`, `headlook.xrs`. `xrsim-selftest.ps1 -Release` PASSES.
  The game cannot run on it until the runtime layer exists (VR-241).
- **Offline RE tools**: `tools/disasm-rva.py`, `tools/pe-xref.ps1`, `tools/read-dump.py`,
  ported (method only). Every address in `src/game/darkness2/patterns.h` has its derivation
  in ENGINE_NOTES s8.
- **Branch and PR**: one branch for VR-231 + VR-239 + VR-232 + VR-236 (the user's decision),
  PR against `staging` with `Fixes VR-231, VR-239, VR-232, VR-236`. Not merged; the merge is
  the user's.

## Next steps (in order)

1. **VR-241 (S0.5): the runtime layer and the mono screen.** Adopt Dishonored's
   `core/vr/openxr_runtime` with its two D3D9-host seams (the device provider, the frame
   texture): a D3D11 device on the runtime's adapter LUID, the D3D9Ex backbuffer carried into
   a shared D3D11 texture, rung 1 "mono screen" (a head-locked quad, both eyes). First on the
   simulator (`xrsim-launch.ps1`, `mono.xrs`, `headlook.xrs`), then VDXR. The device facts it
   needs are measured (ENGINE_NOTES s2): 9Ex device, X8R8G8B8, `PresentEx` on the main thread.
2. **R2 (VR-233): the Lua lane.** Find `luaL_loadbuffer` / `lua_pcall` / `lua_gettop` by
   their 5.1.3 strings with `disasm-rva.py`, wrap `lua_pcall` to catch the `lua_State*`, find
   the game-thread tick, execute `camCtrl:SetBaseFovOverride(110)` (the shipped `SetFov.lua`,
   now readable under `tools/lua/`, shows the call), dump the SWIG tables. Every hook goes in
   from the present tick, byte-verified, default OFF, with the canary re-read shape.
3. **`camera eyetest` (VR-242, S0.5)**: which FOV write the renderer honours, with the projection
   constant read back through a `SetVertexShaderConstantF` hook (R4 starts here).
4. **R3 (VR-234)**: `-console` and `Cmd*` dispatch, once the Lua lane says how commands run.
5. **R6 (VR-237)**: the runtime half is left (who writes the FP entity transform); the data
   half is answered (s5 of GAME_ASSETS).
6. Housekeeping: `LogEverySeconds=5` for the canaries in ordinary builds (8,000 lines per
   half hour at 1 Hz); a `gameplay` detector for `boot.ps1` better than a luma threshold once
   the game state is readable (S5).

## Found and not fixed

- **The main menu drops its keyboard highlight when the mouse moves**, and an Enter with no
  highlight selects nothing. `boot.ps1` retries Continue with a mouse click; a run whose boot
  fails is visible in its shots. No ticket: harness behaviour, fixed in the script.
- **The backbuffer capture cannot show the Steam overlay** (it draws after our hook). The
  overlay's presence is measured by its module and its displacement of the exception filter.
  VR-231's "still draws" criterion is met by that measurement, not a picture (ENGINE_NOTES s7).
- **`read-dump.py`'s PEB parse warns** on this game's dumps (the `minidump` package cannot
  read the PEB's image path); the exception, modules and registers still decode. Cosmetic.
- **`xrsim-launch.ps1 -ViaSteam` and `xrsim-run.ps1`** are ported but cannot be exercised until
  the mod has an OpenXR layer (VR-241). The scripts' log patterns (`xr: runtime "..."`) are
  the Dishonored ones and will be set when that layer logs.
- The Linear org still lacks the `linear` GitHub App; status moves stay manual through the MCP
  (unchanged from VR-230).

## Blockers

None. VR-241 can start on the measured device facts.

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
