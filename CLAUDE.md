# darkness2-vr - Claude session guide

A VR mod for The Darkness II (2012, Digital Extremes; Evolution Engine, 32-bit, Direct3D 9,
Lua 5.1 scripting, gameswf UI; Steam app 67370). Single-player, running locally on a copy of
the game the player owns. The mod module will live in the game process, turn the game's D3D9
frame into eye-tagged D3D11 textures an OpenXR runtime submits (rung 1 "mono screen" first,
then true stereo by re-entering the scene draw once per eye), drive the game camera from the
head pose, serve the VR controllers as the pad the game already understands, and then put each
gun on its own controller and both demon arms on the player's shoulders. One backend: OpenXR,
through VDXR (Quest via Virtual Desktop), any native 32-bit runtime, or the bundled SteamVR
shim. The runtime layer, the simulator and the process are adopted from the Dishonored VR mod
(same org), which was built the same way and shipped 1.0.

**The framework floor exists** (M1 session 1, 2026-09-25): the `d3d9.dll` proxy, logging,
the crash handler, the command seam, `status.json`, the input lane, the backbuffer capture,
the R1 canaries, the simulated OpenXR runtime and the PowerShell harness. Nothing VR yet: the
runtime layer, the capture into D3D11 and the mono screen are VR-241 (S0.5). Anything below
marked "planned" describes the intended layout, not something that exists.

Two branches: **`staging`** (integration; every PR lands here) and **`main`** (release; its tip
is always the latest tag, moved only by the release PR `staging` -> `main` that the user
merges).

## Hard rules

- **NEVER MERGE TO `staging` OR `main` WITHOUT EXPLICIT PERMISSION.** Committing, pushing a
  feature branch and opening a PR against `staging` are all fine on your own judgement; none of
  them touch either branch. **The merge into `staging` is the per-PR gate**, and it is the one
  irreversible step. Ask, show exactly what is about to land, and wait for a yes. A passing
  build is not permission. A finished feature is not permission. **"This branch is good to
  go", "looks great", "nice work" and "close the ticket" are NOT permission**; they are
  approval of the work, not of the merge. If in doubt, do not merge and ask. `main` is never a
  PR base for ordinary work: it moves only in the release ritual (`docs/LINEAR_AND_GITHUB.md`).
- **NEVER commit game-derived content**: no decompiled Lua, no extracted assets, no shader
  dumps, no frame dumps, captures or crash dumps. `tools/lua/`, `tools/cache-out/` and
  `*.png/*.bmp/*.dmp/*.lua/*.swf` are gitignored for a reason. The game's own identifier
  strings (class names, bone names, asset paths) are fine to quote in `docs/`; the assets they
  name are not. Findings go to `docs/darkness2/ENGINE_NOTES.md`, never game files.
- **Never write the game's exe, and never patch its code on disk.** `DarknessII.exe` is linked
  with Valve's CEG. Every hook is an in-memory hook installed after the game is up, and R1's
  verdict (`docs/ROADMAP.md`) says where hooks may sit. Until R1 is written, treat every code
  hook as an experiment behind a lever.
- **Every engine address, IAT slot and struct offset lives in `src/game/darkness2/patterns.h`**
  (planned) and is documented in ENGINE_NOTES with how it was derived. The exe has no ASLR
  (base 0x400000), so absolute addresses are fine, but every code hook byte-verifies its
  target and refuses on a mismatch. **Never copy a number from another game.** The sibling
  mods are Unreal Engine games; this is not. Nothing numeric transfers, and neither do shapes.
- **Never quote a chat verbatim in anything published.** Not in commit messages, PR titles or
  bodies, issues, Linear tickets or comments, code comments, or any file under `docs/`.
  **Report the OBSERVATION, not the sentence.** A perceptual report is evidence and belongs in
  the record; its exact wording never is. Numbers, log lines, ini keys and the game's own
  strings stay quotable.
- **No em dashes anywhere** (code, strings, ini text, docs, scripts, commits): use `-`.
  PowerShell 5.1 parse errors and log/UI mojibake. `tools\lint.ps1` enforces it.
- **Commit messages**: plain conventional commits (`feat:`/`fix:`/`docs:`/`build:`/`tools:`/
  `chore:`/`refactor:`), imperative, subject 72 chars or less, no trailers, no AI attribution
  of any kind. Never put `Fixes VR-<n>` in a commit message: the PR body is the single Linear
  link, and a magic word in a commit double-fires against it.
- **NEVER put a person's name in anything published to GitHub.** Not in a branch name, a
  commit author line, a PR title or body, an issue, a comment, a code comment or any file.
  This includes email local-parts, usernames derived from them, and Linear's own
  copy-branch-name format, which is prefixed with the account holder's username and is
  therefore banned as a source. Branches are `claude/vr-<n>-<slug>` or `codex/vr-<n>-<slug>`,
  typed by hand. This repository is public and GitHub provides no way to delete a pull request.
- **Every change starts from a Linear ticket** in the "Darkness II VR Mod" project (team `VR`,
  workspace `vr-stereo-hub`). Search first; create from the template if it is not there, with
  project, milestone, priority and a `Type` label filled in. Branch `<owner>/vr-<n>-<slug>`
  off `staging`; the PR's base is `staging` (`gh pr create --base staging`; the default branch
  is `main`, so an unqualified PR targets the wrong branch); the PR body's FIRST line is
  `Fixes VR-<n>` (or `Ref VR-<n>` when the base is a working branch). **An agent never
  declares a release and never creates a milestone**: it may report that a milestone is full
  or that its tickets are all Done, and ask. The whole flow is `docs/LINEAR_AND_GITHUB.md`.
- **32-bit only.** The CMake guard (planned) stops a 64-bit configure; don't fight it. The
  OpenXR runtime, the shim and the simulator are all 32-bit.
- **The runtime layer stays as close to the Dishonored copy as the host allows.**
  `core/vr/openxr_runtime.cpp` is a proven layer with two D3D9-host seams (the device
  provider, the frame texture). Fixes port between the three projects only while the rest
  stays verbatim.
- **Every new render lever ships default OFF with a live A/B toggle.** A stereo method is a
  lever: it registers by name, `stereo <name>` switches live, a method that refuses leaves the
  previous one running (fail soft). A seam word with no F10 control is not shipped.
- **Fail soft.** A failed hook, a failed scan, a refused verify logs why with the values and
  lets the game run flat. Nothing may resolve at init: the engine's tables are not populated
  at `DllMain` time.
- **Retired experiments go to `src/legacy/`** (compiled only with a CMake option), never
  deleted silently.
- **No code from UEVR** (all rights reserved; concepts only). REFramework (MIT) may be adapted
  with an attribution comment.
- Terminology: "the mod module" = the DLL in the game process (planned: a `d3d9.dll` proxy,
  pending R0); "runtime layer" = `core/vr/openxr_runtime` (the OpenXR instance/session/
  pacing/poses/layers); "shim" = the OpenXR-on-OpenVR DLL for SteamVR rigs; "method" = a
  stereo strategy behind `core/gfx/stereo.h` (mono | reentry, aer kept as the A/B stub);
  "rung" = its place on the ladder; "the stereo seam" = that interface; "the camera seam" =
  `game/darkness2/camera.h` (rotation, FOV, eye offset per eye); "eye tag" = the -1/+1/0 a
  method attaches to a present; "seam" alone = `command.txt`; "lane" = the thread a write
  belongs to (present thread, game thread, Lua); "the Lua lane" = code executed inside the
  engine's own `lua_State` on the game thread; "verdict" = the written answer a Research
  ticket closes with.
- **The engineering rules carried over from the BioShock trilogy and Dishonored mods** (each
  one a bug that shipped): a verified write is not an honoured one (acceptance is a measured
  downstream effect); never copy a number between games; an instrument that cannot fail its
  own hypothesis is not evidence; sample strided and vote, a frame is not homogeneous; a
  measurement carries the identity of what it measured; a counter is not evidence until you
  know its population; measure first; prefer a falsifiable prediction over another capture;
  derive lens laws at more than one aspect; identify a render pass by making it MOVE; every
  new render lever ships default OFF with a live A/B toggle; one ray (anything that claims to
  point where shots go derives from the identical ray, and with two guns that means one ray
  PER HAND); engine-side writes let attachments follow for free, matrix patches do not; fail
  soft; stopping and handing back are different operations; never take a reference to an
  engine D3D object inside a detour; backbuffer detectors sample before our own writers; ImGui
  only from the overlay's draw callback; a probe hook's argument count must equal `ret imm /
  4`; the present thread owns every runtime call (the pace thread runs `xrWaitFrame` by
  request only). The full list with the story behind each is `docs/TRAPS.md`.
- **The original Dishonored author's process rules**, each one paid for: one behavioural
  change per build; build on a snapshot confirmed good; when something that worked breaks, diff
  against the working build FIRST; measure before theorising (read the artifacts already on
  disk); never ship a guessed constant as a measured one; the packaged ini is a byte copy of
  the tested machine's ini; motion controls must never stop working; do not attribute logs to
  machines by drive letter, check the build tag.

## Resources you already have - CHECK THESE BEFORE DERIVING ANYTHING

The sibling mods paid for a set of instruments, and sessions there kept re-deriving things one
of them answered in a single command. **Before hand-walking a disassembly, guessing an offset,
or asking for a headset run, ask whether one of these already knows.**

**The rule that governs all of them: the TOOLS are ours and are committed; their OUTPUT is
game-derived and never is.** Summarize findings into ENGINE_NOTES; keep dumps, addresses in
bulk, extracted assets and decompiled text out of the tree.

### Reverse engineering, offline (no game running)

| Tool | Answers |
|---|---|
| `tools/disasm-rva.py <exe> dis\|bytes\|float\|search\|xref\|calls\|disp` (port from Dishonored, planned) | Disassemble a range, find a float constant, find who references an address, find who calls an RVA, find every instruction using a structure displacement |
| `tools/pe-xref.ps1 -Exe <exe> -TargetRva <rva>` (port, planned) | Caller census. **Zero callers on a function the engine must call every frame is the cheapest way to learn a hook target is dead BEFORE installing it** |
| `tools/cache/extract.py` (R5, planned) | The read-only `.toc`/`.cache` reader: list every asset path, extract one by path, dump every `.lua` as readable source (the source text is embedded in the bytecode), parse a `_skel.fbx` blob's bone table |
| The exe's own strings | This engine is unusually talkative: type paths (`/EE/Types/...`, `/D2/Types/...`), SWIG method names, property names with their editor tooltips (`Base FOV, leave at 0.0 to use default gFOV`), command names (`CmdSetBaseFOV`), setting keys. A string reference is the first step to every function in this game. The inventory so far is in ENGINE_NOTES |

**There is no `ue3-natives.py` here.** That tool walks Unreal Engine 3's native registration
table, and this is not UE3. The equivalents in this engine are the SWIG registration tables
(R2) and the string-referenced type system.

### Reverse engineering, at runtime (the engine knows things the exe does not)

| Thing | Answers |
|---|---|
| The Lua lane (R2, planned) | **Everything SWIG exposes**: once the mod can execute Lua inside the game's `lua_State`, engine objects can be found and called by NAME (`camCtrl:SetBaseFovOverride`), which is how the shipped scripts do it. This is the biggest lever this game offers and the reason R2 is Urgent |
| Property watch (planned, port of Dishonored's `propwatch`) | **WHICH field just changed**, when the name is what is missing |
| The animation start log (R7, planned) | Every animation start by name and target entity |

### Game content (local only, gitignored, never committed)

| Resource | What it is good for |
|---|---|
| `tools/lua/` (extracted by R5) | The game's own Lua scripts, readable. Menu options and what they set (`OptionsDisplayCustomize.lua`), the FOV script (`SetFov.lua`), the player and camera scripts. Declarations, calls and tweak values |
| `docs/darkness2/GAME_ASSETS.md` | The map of the caches: what is where, the FP rig inventory, the demon arm assets, the animation sets. Read it before asking whether the game has something |
| `docs/darkness2/GAME_CONFIG_MAP.md` | What is known about the game's settings, which are obfuscated on disk. **Do not write `EE.cfg`**; request through the engine's own path |
| The sibling repos | `C:\Users\user\Documents\Random projects\Dishonored-VR` (branch `staging`) and `...\bioshock-1-vr-mod` (branch `origin/staging`) on the dev PC, for parity on a subsystem before re-deriving it. Copy the METHOD; never the numbers |

### Live instrumentation (planned; the Dishonored shape)

`tools\game-cmd.ps1 "<cmd>"` drives the command seam; `tools\status-dump.ps1` reads
`status.json`; `tools\tail-log.ps1 -Grep` follows the log; `D2VR_LOG=trace` and
`D2VR_LOG_CATS=tentacles:debug` open a lane. The simulator (`tools\xrsim-*.ps1`) answers
anything that is not perceptual without costing a person their evening. `docs/VERIFICATION.md`
is the catalog of intent -> tool -> command -> how to read it.

## Logging

**Log generously. The log is the only instrument a remote tester can send back.** Every
session writes `darkness2_vr.log` next to the exe (previous runs rotated `.prev.log` through
`.prev9.log`, ten sessions); `log::init` runs from `DllMain` before anything else can fail, and
**must never be gated** on VR bring-up, a config read, or a headset being present. A run
always produces a log, even one that dies in the first second.

Extensive does not mean noisy. The rules that buy volume without cost:

- **Use the levels.** `Error`/`Warn` are for things a player or a maintainer must act on;
  `Info` is the readable narrative of a run and is what a bug report contains; `Debug`/`Trace`
  belong to a lane under investigation and stay off until asked for.
- **Never pay for a line you do not print.** Put the work INSIDE the log call so the
  per-category threshold gates it.
- **Nothing unbounded in a per-frame or per-draw path.** `LOG_EVERY_MS`, `LOG_ONCE`,
  `LOG_FIRST_N`.
- **Log state CHANGES, not state.**
- **An instrument that cannot fail its own hypothesis is not evidence.** A counter line must
  also say what would make the counter move, and it must be able to print the unwelcome
  answer. If a zero is expected by design, the line must say so on the line.
- **Name the owner before the result.** Where several subsystems can drive one thing (the
  camera, the hands, the tentacle anchor, the frame), log WHICH one owns it, then what it did.
- **Every refused guard says why, with the values.**
- **Log the derived number, not just the inputs.** Where geometry decides what the player sees
  (subtended angles, aspect, world scale, the shoulder anchor position), log the computed
  result so a complaint is arithmetic instead of opinion.

## Session protocol

- **A setting that "does not work" is a `docs/TRAPS.md` question before it is a code question.**
  Find every place the value can live, read what the run RESOLVED it to (not what you wrote),
  and confirm it reached the consumer. New traps and failed plans go in that file in the same
  commit as the work.
- **Before deriving an address, an offset or a behaviour, read "Resources you already have"
  above.** Then read `docs/darkness2/ENGINE_NOTES.md` "Dead ends" and `docs/TRAPS.md`
  "Failed plans". Grep the graveyard before proposing an approach.
- **Touching a feature? Read its doc first.** Tentacles: `docs/darkness2/TENTACLES.md`. Guns:
  `docs/darkness2/DUAL_WIELD.md`. Anything that plays, stops or poses an animation, or hides or
  moves a mesh: `docs/darkness2/ANIM_AND_MODEL_CONTROL.md`. Adding any feature:
  `docs/FEATURE_PROCESS.md`.
- **START**: read `docs/STATUS.md`, the current milestone in `docs/ROADMAP.md`, then
  `git log --oneline -10`. If the previous session's PR has been merged since, move its ticket
  to Done through the MCP if nobody has. **Find the Linear ticket** for the work (search before
  creating; create from the template if absent, with project, milestone, priority and a `Type`
  label), move it to In Progress and branch `<owner>/vr-<n>-<slug>` off `staging`. Touching engine
  internals? Read ENGINE_NOTES first; new findings go there in the same commit as the code.
- **Validate in the SIMULATOR before asking for a headset.** `tools\xrsim-selftest.ps1` says
  whether the simulator itself is healthy; the game runs on it once the runtime layer exists
  (VR-241). Perceptual questions (comfort, judder, world scale, warp, whether the tentacles feel
  like yours) still need the headset and the F10 overlay.
- **The session launches, drives and quits the game itself** (the user's decision,
  2026-09-25; it replaced the earlier "never launch" rule once the harness could drive the
  flat game). `tools\launch-game.ps1` goes through Steam and archives the previous log;
  `tools\boot.ps1` reaches gameplay on the newest save; `tools\game-key.ps1` /
  `game-cmd.ps1 "key ..."` inject input INSIDE the game process; `tools\game-shot.ps1` writes
  the backbuffer as a BMP the session reads as an image; `tools\quit-game.ps1` closes it. Every
  launch: install, diff the ini, archive the log, launch, check the banner names the installed
  build before reading anything. Perceptual questions (comfort, judder, world scale) still need
  the user in the headset, with a live A/B lever, and are asked as ONE question per headset run.
- Non-obvious design choices get a dated entry in the decision log at the bottom of
  `docs/ARCHITECTURE.md`.
- **END**: rewrite "Current state" and "Next steps" in `docs/STATUS.md`, append a dated session
  log entry, tick `docs/ROADMAP.md` boxes, commit, push. A session that ends without pushing
  STATUS.md is a failed handoff. Open the PR against `staging` with `Fixes VR-<n>` as the
  body's first line and fill in `.github/PULL_REQUEST_TEMPLATE.md`. **Then move the ticket
  yourself through the Linear MCP**: In Review with the PR URL attached when the PR opens; Done
  after the user has merged (`Released` comes with the release PR into `main` and the tag).
  Nothing in Linear moves automatically here; the GitHub App automations are not used. Put
  measurements and verdicts on the TICKET, not only the PR; the ticket outlives the branch.
  File a ticket for every fault found and deliberately not fixed, and name it in the PR's
  "what is deliberately not here". If a group of tickets closed, post one batch project
  update on Linear, not one per ticket.

## Build / install / test

```powershell
.\tools\build.ps1 [-Release]              # the mod module + the simulator + the smoke client
.\tools\install.ps1 -Release [-Set "Canary.Cold=1"]   # copies next to DarknessII.exe; DIFFS the full ini
.\tools\lint.ps1                          # the em-dash gate and friends
.\tools\exports-check.ps1 build\src\RelWithDebInfo\d3d9.dll   # the 23-entry export table
.\tools\launch-game.ps1 -WaitBanner       # through Steam; archives the previous log; waits for the banner
.\tools\boot.ps1                          # title -> menu -> Continue -> gameplay, with shots
.\tools\tail-log.ps1 [-Once] [-Grep "route:|canary"]   # follow <game>\darkness2_vr.log
.\tools\game-cmd.ps1 "status" "shot menu"  # the command seam (waits for the ack)
.\tools\game-key.ps1 esc -Hold 150        # keys/mouse through the mod's input lane
.\tools\game-shot.ps1 -Tag x              # the backbuffer as <data>\shots\x_NNN.bmp
.\tools\status-dump.ps1                   # status.json, pretty-printed
.\tools\module-census.ps1                 # the running game's modules (32-bit), the route from outside
.\tools\quit-game.ps1                     # WM_CLOSE through the seam, Stop-Process fallback
.\tools\soak.ps1 -Minutes 30              # R1's protocol with the four canaries live
.\tools\log-parse.ps1 [-Canaries]         # summarise a log
.\tools\xrsim-selftest.ps1 -Release       # is the SIMULATOR healthy? (xr_hello32 on d2vr-xrsim)
.\tools\xrsim-launch.ps1                  # launch the game on the simulator (needs VR-241)
.\tools\xrsim-cmd.ps1 "head rot 30 0 0"   # drive the simulated head/hands/controls
.\tools\xrsim-shot.ps1 -Out shot          # per-eye compositor capture + JSON to assert on
python tools\disasm-rva.py <exe> dis 520EE0   # offline RE (output never committed)
.\tools\pe-xref.ps1 -Exe <exe> -TargetRva 8F2C30  # caller census
python tools\read-dump.py <dmp>            # summarise a minidump
```

- Game: Steam appid 67370, `<library>\steamapps\common\Darkness II\DarknessII.exe` (on the
  dev PC: `D:\SteamLibrary\steamapps\common\Darkness II`). Scripts resolve it from
  `D2VR_GAME_DIR` or Steam's `libraryfolders.vdf` and throw otherwise. **Launch through Steam**;
  the exe delay-loads `Tools\steam_api.dll` and expects the Steam client.
- Game config: `%APPDATA%\DarknessII\` (`EE.cfg`, `Editor.cfg`, `<steamid>\settings`,
  `<steamid>\CONTINUE.SAV`), all obfuscated. Read-only for the mod.
- Files next to the exe: `darkness2_vr.ini`, `darkness2_vr.log` (+ `.prev.log` .. `.prev9.log`),
  `darkness2_vr_crash.txt`, `disable_vr.txt` (kill switch: log and route report only). Harness
  files in `%LOCALAPPDATA%\Darkness2VR\`: `command.txt`, `ack.txt`, `status.json`, `dumps\`,
  `shots\`, `xrsim\`. Override with `D2VR_DATA_DIR`. Env: `D2VR_LOG`, `D2VR_LOG_CATS`,
  `D2VR_SKIP`, `D2VR_GAME_DIR`.
- Clean clone: `git clone --recursive` (submodules `third_party/OpenXR-SDK`, `third_party/imgui`).

## Repo map (planned)

- `src/proxy/` - `DllMain` and the exports of whichever DLL R0 picks; chains to the system DLL
- `src/core/` - engine-agnostic VR core: `util/` (log, crash, clock, mem, ini, paths),
  `hooks/` (vtable, IAT, detour), `framework/` (frame_hooks = the D3D9 hooks and the frame
  path's order; command seam; status.json), `gfx/` (stereo = the seam and registry, capture,
  blit, mono_screen, reentry, d3d11_device), `vr/` (openxr_runtime = the runtime layer,
  openxr_input = the action layer), `input/` (virtual gamepad, hotkeys), `ui/` (overlay),
  `config/`
- `src/game/darkness2/` - everything that knows an address or an engine layout: `patterns.h`,
  `lua/` (the Lua lane, the SWIG map), camera (the per-eye seam + eyetest), head tracking,
  present_tick, anim (the control layers), models, hands, weapons, tentacles, gameswf HUD,
  game state, the seam's game words
- `src/legacy/` - retired experiments (off by default)
- `src/tools/` - `xrsim/` (the simulated OpenXR runtime), `ovrshim/` (the SteamVR shim),
  `installer/`, `xr_hello32/` (smoke client)
- `third_party/` - imgui, OpenXR-SDK (submodules), vendored OpenVR
- `tools/` - the PowerShell harness, `cache/` (the extractor), `lib/` shared helpers
- `tests/golden/` - the default ini, the export table
- `docs/` - the project's brain; index below

## Docs index

| File | Purpose |
|---|---|
| `docs/STATUS.md` | **Session handoff**: current state, next steps, blockers, session log |
| `docs/ROADMAP.md` | The research gate R0-R7 and the ladder S0-S9, mapped to the four milestones, with "done when" boxes |
| `docs/ARCHITECTURE.md` | The planned frame path, the stereo ladder, the runtime layer, the camera seam, the Lua lane, the control layers, lanes, config, and the dated **decision log** |
| `docs/TRAPS.md` | **Traps and the graveyard**: every class of trap the sibling mods paid for, grouped, with sources; failed plans not to retry without new evidence; this repo's own entries |
| `docs/LESSONS_FROM_SIBLING_MODS.md` | The order each sibling delivered its features, what made Dishonored polished, why BioShock stayed rougher, the ten rules that transfer |
| `docs/FEATURE_PROCESS.md` | **How a feature is added here**, from ticket to verdict: find the engine function, choose where to intervene, design the ladder, build the lever, confirm by observation, close |
| `docs/POLISH.md` | What finished means for this mod and the measurement or headset judgement that closes each item |
| `docs/VERIFICATION.md` | The three tiers, the decision table shape, the simulator plan, the harness gotchas that transfer, what still needs a human |
| `docs/RESEARCH.md` | Engine facts, prior art, runtime facts, legal posture, all with sources |
| `docs/LINEAR_AND_GITHUB.md` | **The dev flow**: ticket -> branch -> PR -> review -> merge -> release. Statuses, priority, labels, the ticket and PR templates, project updates, the release ritual, what only the Linear UI can do |
| `docs/KNOWN_ISSUES.md` | User-facing known issues (will ship in the zip) |
| `docs/TROUBLESHOOTING.md` | User-facing troubleshooting (will ship in the zip) |
| `docs/RELEASE_NOTES.md` | Per-version notes |
| `docs/darkness2/ENGINE_NOTES.md` | **The reverse-engineering knowledge base**: PE identity, imports, the engine's namespaces, every string inventory, config locations; addresses with derivation once they exist; dead ends |
| `docs/darkness2/GAME_ASSETS.md` | **The caches and what is in them**: the `.toc`/`.cache` format, the FP rig, the demon arm assets, the animation sets, the extractor plan |
| `docs/darkness2/TENTACLES.md` | **The demon arms in VR**: the verdict on the assets, the sub-ladder T0-T10, targeting, finishers as hand-back |
| `docs/darkness2/DUAL_WIELD.md` | **Two guns on two controllers**: W0-W8, one ray per hand, the fire seam, button ownership, reload under hand control |
| `docs/darkness2/ANIM_AND_MODEL_CONTROL.md` | **The control layers** A0-A7 and M0-M6, in the order they unblock features |
| `docs/darkness2/GAME_CONFIG_MAP.md` | The game's own settings: what is known, what is obfuscated, the rule not to write them |
| `docs/darkness2/TESTING.md` | Install/launch loop, flat checks, headset checklist shape, crash triage, the rig |
