# Status

Session handoff, newest first. Every session rewrites "Where things are RIGHT NOW" and "Next
steps" and prepends a dated block. A session that ends without pushing this file is a failed
handoff.

## Where things are RIGHT NOW (2026-09-25)

- **No code.** The repository holds the documentation set, the dev flow, the `.github`
  templates, `tools/lint.ps1`, and nothing else. The game has never run with a mod module.
- **The board is shaped.** Linear project "Darkness II VR Mod" (P-VR-5): five new `area:`
  labels (`tentacles`, `anim`, `models`, `lua`, `weapons`), the project spec, four milestones
  (M1 In-process and on screen, M2 Stereo world, M3 Quad-wield, M4 Noir polish 1.0), the
  backlog filed from `docs/ROADMAP.md`. VR-230 is this session's own ticket.
- **Branches**: `staging` (integration) and `main` (release). This session's PR targets
  `staging`.
- **The game is surveyed** (read-only): `docs/darkness2/ENGINE_NOTES.md` (PE identity,
  imports, engine strings, config), `GAME_ASSETS.md` (the cache format, the FP rig, the
  demon arm assets and animations), `TENTACLES.md` (the VR verdict and the sub-ladder).
- **The tentacle verdict**: the assets are good enough for VR; the risks are anchoring (the
  arms hang off the camera bone), keyframed motion (the mod supplies the dynamics), and
  possible flat-only geometry. All three are M3 tickets.
- **Two things only the user can do** are on the UI checklist below: install the `linear`
  GitHub App on the org (only `linear-code` is installed, so `Fixes VR-n` does nothing yet)
  and set the PR automation rows.

## Next steps (in order)

1. **R0**: decide the loading route for a runtime-loaded d3d9. Measure the exe's DLL search
   directory with loader snaps; try the app-dir `d3d9.dll` proxy first.
2. **R1**: prove in-memory hooks survive CEG for a 30-minute session with four canary hooks.
3. **R2**: reach the Lua VM and dump the SWIG API. This is the biggest lever in the project;
   do it before any feature.
4. In parallel with R1/R2: **R5** (the cache extractor; it needs no running game) and the
   framework chore (proxy, logging, crash handler, seam, status.json, CMake, the simulator
   port), which is blocked on R0/R1 only for the loading route.
5. Then R3, R4, R6, R7, and S0.5 (the runtime layer, the mono screen, `camera eyetest`).

## Found and not fixed

- The Linear org has only the `linear-code` GitHub App installed; magic-word linking is dead
  for every repo in the org (Dishonored's PRs all show `linkedIssues: []`). UI checklist item;
  not a code fault.
- The Linear workspace agent guidance (org-wide) contradicts itself on branch naming
  (Dishonored's copy). A corrected block is in `docs/LINEAR_AND_GITHUB.md`; pasting it is the
  user's.

## Blockers

None. The research gate can start.

## The user's UI checklist (Linear settings the MCP cannot reach)

1. Install the `linear` GitHub App on the `VR-Stereo-Hub` org (Linear Settings > Integrations
   > GitHub > connect), accept on GitHub, then confirm with
   `gh api orgs/VR-Stereo-Hub/installations --jq '.installations[].app_slug'` that both
   `linear` and `linear-code` appear. Re-save one PR body afterwards to fire a webhook and
   check `linkedIssues` is no longer empty.
2. Team VR > Issue statuses and automations: PR opened -> In Progress; review requested ->
   In Review; PR merged -> Done **restricted to base `staging`**; any other base (including
   `main`) -> no action. (The Dishonored copy of this rule needs the same change once its
   vr-218 branch lands.)
3. Settings > Agents > Additional guidance: paste the corrected block from
   `docs/LINEAR_AND_GITHUB.md` "The workspace agent guidance".
4. Optional: branch protection on `main` (PRs only, no direct push), so nothing but a release
   PR can move it.

---

## Session log

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
