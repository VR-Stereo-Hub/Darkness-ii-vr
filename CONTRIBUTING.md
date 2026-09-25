# Contributing

This is a VR mod for The Darkness II (2012). It runs locally, on a copy of the game you own.
There is no build to install yet; the project starts with its documentation and its board.

## If you are reporting a bug or asking for a feature

Open a GitHub issue using one of the templates. Read
[`docs/KNOWN_ISSUES.md`](docs/KNOWN_ISSUES.md) and
[`docs/TROUBLESHOOTING.md`](docs/TROUBLESHOOTING.md) first, and **attach
`darkness2_vr.log`** once there is a build that writes one. It sits next to
`DarknessII.exe`; the previous runs are kept as `darkness2_vr.prev.log` and so on, so copy
them out before relaunching.

Please do not attach game footage, frame dumps or captures. This repository carries no
game-derived content, deliberately.

## If you are on the team

The board is the source of truth: [linear.app/vr-stereo-hub](https://linear.app/vr-stereo-hub),
team **VR**, project **Darkness II VR Mod**. The whole flow, from finding a ticket to cutting a
release, is in **[`docs/LINEAR_AND_GITHUB.md`](docs/LINEAR_AND_GITHUB.md)**.

The short version:

1. Search Linear. Create the ticket from the template if it is not there, with project,
   milestone, priority and a `Type` label filled in.
2. Branch `<owner>/vr-<n>-<slug>` off `staging`. Type the name; do not copy Linear's branch
   name, it carries the account holder's name.
3. Validate in the simulator (`tools\xrsim-*`, once it exists) before asking anyone for a
   headset run.
4. Open the PR **against `staging`** (`gh pr create --base staging`) with `Fixes VR-<n>` as
   the first line of the body, and fill in the template.
5. Merge to `staging` once the maintainer says so. Linear marks the ticket Done.
6. Update `docs/STATUS.md`, tick `docs/ROADMAP.md`, push.

`main` is the release branch: its tip is always the latest tag on the Releases page, and
only the release PR (`staging` -> `main`) moves it.

## Before you write any code

Read [`CLAUDE.md`](CLAUDE.md). It is the contributor guide as much as the agent guide, and its
hard rules are not style preferences: they are the record of things that have already gone
wrong on the two sibling mods this one is modelled on.

The ones that catch people first:

- **Never commit game-derived content.** No decompiled Lua, no extracted assets, no frame
  dumps, captures or crash dumps.
- **Never patch the exe on disk.** It is CEG-linked. Hooks live in memory, installed after the
  game is up.
- **No em dashes anywhere.** Use `-`. `tools\lint.ps1` enforces it, because the character has
  caused PowerShell 5.1 parse errors and mojibake in logs.
- **Every engine address and struct offset lives in `src/game/darkness2/patterns.h`** and is
  documented in `docs/darkness2/ENGINE_NOTES.md` with how it was derived. Every hook
  byte-verifies its target and refuses on a mismatch. Never copy a number from another game.
- **Every new render lever ships default OFF with a live A/B toggle.**
- **Acceptance is a measured effect, not landed code.** A verified write is not an honoured one.
- **Never quote a chat verbatim** in a commit message, PR body, issue, comment or doc. Report
  the observation instead.

## Building (planned)

```powershell
.\tools\build.ps1 [-Release]     # the mod module, the shim, the simulator and the smoke client
.\tools\install.ps1 [-Release]   # copies next to DarknessII.exe
.\tools\lint.ps1                 # the em-dash gate and friends
```

32-bit only; the CMake guard stops a 64-bit configure on purpose. A clean clone will need
`git clone --recursive` once the submodules under `third_party/` exist.

## Testing without a headset (planned)

`tools\xrsim-launch.ps1` will run the game against a simulated 32-bit OpenXR runtime that
presents as a Quest 3: head and hand poses, every controller button, deterministic frame
stepping and per-eye compositor captures. Almost everything can be answered there.
[`docs/VERIFICATION.md`](docs/VERIFICATION.md) is the catalog: intent, tool, command, and how
to read the result.

Perceptual questions (comfort, judder, world scale, warp, whether the demon arms feel like
they are yours) still need a real headset and the F10 overlay. Those tickets carry the
`needs-headset` label.

## Licence and provenance

No code from UEVR (all rights reserved; concepts only). REFramework (MIT) may be adapted with an
attribution comment. The OpenXR runtime layer, the simulator and the SteamVR shim are adopted
from the Dishonored VR mod and stay as close to that copy as the host allows, so fixes port
between the projects. See `THIRD_PARTY_NOTICES.md`.
