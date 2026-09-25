# Codex session guide

Read `CLAUDE.md` in full before doing anything. It is the contributor guide for every agent,
not only Claude, and its hard rules are the record of things that already went wrong on the
sibling mods. This file adds only what Codex sessions have needed on top of it.

## Session requirements

- **Never launch the game.** Build, install, and hand the user one question per launch with
  the expected outcomes and what each one means. Never ask the tester to run commands.
- **Check the installed log banner against the build before interpreting a run.** The log is
  overwritten every launch (rotation keeps ten). Archive `darkness2_vr.log` and the `.prev`
  files before every relaunch.
- **Diff the full installed ini on every install** and preserve CRLF where the game's files
  use it. Report a zero-setting diff as such.
- **A class-name check is not liveness.** Revalidate retained identity after menus and loads;
  an unchanged pointer does not establish that it is the same live object.
- **Ticket before number.** Create or verify the Linear ticket before putting its number in a
  branch, a file or a commit.
- **Branch `codex/vr-<n>-<slug>`** off `staging`, typed by hand. PR base `staging`. Body line
  one `Fixes VR-<n>`. Then move the ticket yourself through the Linear MCP: In Review with the
  PR URL attached when the PR opens, Done after the user merges. Nothing moves automatically.
- **No trailers.** Commits, PRs and merges carry no generated-by or co-authored-by lines.
- **Never merge.** The merge into `staging` is the user's, every time.
- **Plans for review go to a doc before code** (`docs/darkness2/PLAN-<topic>.md`), with the
  claims the first run must prove or kill written out, each with its prediction and
  counterprediction.
