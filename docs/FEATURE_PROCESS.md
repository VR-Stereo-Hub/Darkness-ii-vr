# How a feature is added here

Dishonored has this process spread across its CLAUDE.md, its plans and its verification
catalog. This repo keeps it in one place. It applies to every ticket that changes what the
player sees or feels, and most of it applies to a Research ticket too (which ends at step F
with a verdict instead of a lever).

The shape, in one line: **ticket, read, find the engine function, choose where to intervene,
design the ladder, build the lever, confirm by observation, close.**

## A. The ticket and the reading

1. Read `docs/STATUS.md` "Where things are RIGHT NOW", the current milestone in
   `docs/ROADMAP.md`, and `git log --oneline -10`.
2. Find or create the Linear ticket (`docs/LINEAR_AND_GITHUB.md`): Summary, Technical detail
   naming the files and the levers that already exist, Non-goals, falsifiable Pass criteria.
   Move it to In Progress. Branch `claude/vr-<n>-<slug>` off `staging`, typed by hand.
3. Touching engine internals? Read `docs/darkness2/ENGINE_NOTES.md` first, including "Dead
   ends". Touching the tentacles, the guns, or anything that plays, stops or poses an
   animation or hides or moves a mesh? Read the topic doc under `docs/darkness2/` first.
4. **Grep the graveyard before proposing an approach.** `docs/TRAPS.md` sections 11 and 12,
   ENGINE_NOTES "Dead ends", the ARCHITECTURE decision log. A plan that starts with "the
   sibling did X" starts with the measurement that killed or proved X there.
5. **Check "Resources you already have"** in `CLAUDE.md`. The expensive sessions on the
   siblings were the ones that re-derived something already on disk.

## B. Find the engine function

The game is unusually talkative: every property, method, type and command carries its name
as a string, and the Lua VM (once R2 lands) reaches engine objects by name. Use that before
anything else.

1. **Start from a string.** The property name, the tooltip, the command name, the CTAB
   constant. `disasm-rva.py search` and `xref` find who references it. The reference leads to
   the reflection table, the table to the accessor or the type.
2. **Derive through a known-good route first.** Before trusting a route to answer a new
   question, make it reproduce an answer already in ENGINE_NOTES. Dishonored re-walked the
   crossbow's numbers before trusting the pistol's.
3. **Two independent confirmations**: the same callees in the same order from two starting
   points, or a static xref and a live probe that agree.
4. **Resolve by NAME, never hardcode an offset**, once at a sane moment (never at init, never
   on a cadence), behind a gate that refuses when the tables are not ready. Batch lookups.
5. **Run a caller census before hooking.** Zero static callers on a function the engine must
   call every frame means the hook target is dead. Read the first `ret` and count the
   arguments.
6. **Identify a render pass by making it MOVE**, and a flag by watching it CHANGE. A flag is
   not a flag until a run can show it changing.
7. **Before hooking a local, list every write to it up to the join** (`lea`-then-push counts).
8. **Record it**: the address in `patterns.h` with its byte-verify signature, the derivation
   in ENGINE_NOTES, in the SAME commit as the code that uses it.

## C. Choose where to intervene

- **Change the request, not the result.** When the engine recomputes a value every tick,
  change its input; do not race the recompute. Measure write survival first.
- **Redirect a consumer at its INPUT, never at its output**, and keep the engine's own
  magnitudes so collision, markers and effects agree.
- **Prefer the engine's own switch** where one exists (a property, a Lua call, a console
  command) over a hook. This game has more of those than any sibling had.
- **Engine-side writes** (the bone the engine reads, the aim vector the engine consumes)
  over render-side patches, so attachments and effects follow.
- **Writing a component requires engine ownership**: the engine's own owner field, a live
  object, retained identity. Never a name test.
- **Pick the lane** and stay on it: engine writes on the game thread, OpenXR calls only on the
  present thread, Lua only inside the engine's own call, ImGui only from the overlay's draw
  callback. Publish across lanes as self-expiring snapshots.
- **Never write the field the engine is steering by.** The mod or the game steers; never both.

## D. Design the ladder

Write it down before code, as a checklist in the ticket or, for anything bigger than a
session, as `docs/darkness2/PLAN-<topic>.md`:

1. **One behavioural change per build.** Each rung names what it proves and what kills it.
   Rung 1 writes nothing the game reads (read-only observation).
2. **The claims the first run must prove or kill**, each with a prediction and a
   counterprediction, and the arithmetic done for every branch before the run. Prefer a
   falsifiable prediction over another capture.
3. **Phase 1 is read-only** (observe the flag, log the anim starts, draw the skeleton in the
   overlay); **phase 2 intervenes.**
4. A Research ticket delivers one measurement and a verdict per prerequisite, in order, each
   committed before the next.
5. Plans that need an outside read go to a doc first and come back through a handoff doc.

## E. Build the lever

- A lever is four things or it is not shipped: an ini key, a seam word, an F10 control and a
  line in `status.json`. **Default OFF with a live A/B.** Exceptions are argued in the PR and
  approved by the user.
- A method or hook that refuses **logs why, with the values**, and fails soft: the previous
  method keeps running, the game runs flat.
- **Logging** per `CLAUDE.md`: name the owner before the result, log changes not state, gate
  per-frame lines, give every counter a way to print the unwelcome answer, log the derived
  number.
- **Config**: one home per key; a new name rather than a re-defaulted old one; no version
  bump; per-key migrations with a marker written in both branches; regenerate the golden ini.
- **A host test** compiles the production code with synthetic inputs and **requires the old
  code to fail** (the negative control). A test suite that has never failed has never tested.

## F. Confirm by observation

1. **Simulator first.** An `.xrs` sequence with assertions, captures with numeric thresholds,
   the resolved-config line, the beat line. Foreground the window. Reach gameplay, not the
   attract screen.
2. **Then install**: Release build, `install.ps1`, the full ini diffed against the previous
   one, CRLF preserved, the previous log archived.
3. **Hand the user ONE question per launch**, with the expected outcomes and what each one
   means. Never launch the game yourself. Never ask for a command typed mid-run; build the
   A/B into F10.
4. **Check the log banner matches the installed build** before reading anything from a run.
5. **Acceptance is a measured downstream effect.** The decal, not the logged origin. The
   shoulder position at two head yaws, not the write. The honoured-check (the engine's own
   state changing within a window), not the request.
6. **Perceptual verdicts come from the headset**, written in STATUS with the build id, in the
   tester's terms, never their words.
7. **The log must be able to explain a failure without another run.**

## G. Close

- Same commit: ENGINE_NOTES findings, TRAPS entries for anything new that bit, the decision
  in ARCHITECTURE's log if a non-obvious choice was made, the topic doc updated.
- STATUS "Where things are RIGHT NOW" and "Next steps" rewritten; ROADMAP boxes ticked.
- The PR against `staging`: evidence with numbers, every lever with its default and A/B,
  blast radius and fail-soft, **what is deliberately not here** with a ticket for each
  unfixed fault, testing.
- Measurements and verdicts on the TICKET, not only the PR.
- **Wait for the user's explicit merge instruction.**
- Promote tested values to defaults only when the user says so; the shipped ini is a byte
  copy of the tested one.
