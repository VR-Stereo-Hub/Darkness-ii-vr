# Linear and GitHub

How a change gets from "someone noticed something" to "that shipped": the ticket, the branch,
the pull request, the review, the merge, and the release. It binds humans and agents to the
same flow. The Dishonored VR mod adopted this flow on 2026-09-05 after three people had been
finding real faults and recording them in three private places; this repo starts with it on
day one so that never happens here.

The board lives at [linear.app/vr-stereo-hub](https://linear.app/vr-stereo-hub), team **VR**,
project **Darkness II VR Mod**. `CLAUDE.md` carries the short version of these rules; this
file is the long one.

## What goes on the board, and what does not

The board carries **work**: a thing to do, with an owner, a milestone and a definition of done.
It is not the project's memory.

The memory stays in `docs/`, and the split is not negotiable because the two rot at different
speeds. An engine address, a struct layout, a falsified hypothesis, the reason a lever exists:
those go to `docs/darkness2/ENGINE_NOTES.md`, `docs/TRAPS.md` or `docs/ARCHITECTURE.md` and
stay true for years. A ticket is true until it is closed. Putting a derivation in a ticket
description buries it the moment the ticket is done, and the next person re-derives it.

So: a ticket **links** to the notes and quotes the numbers it needs to be actionable. The notes
never cite a ticket as their source of record.

## The map

| Thing | Here |
|---|---|
| Team | **VR** - one team for every mod in the org |
| Project | **Darkness II VR Mod** - one project per repository |
| Branches | **`staging`** is the integration branch: every pull request lands here. **`main`** is the release branch: its tip is always the latest tag on the Releases page, and the only PR that touches it is the release PR (`staging` -> `main`) |
| Milestone | **A stage the user has decided to ship.** Four exist, named by the user on 2026-09-25: `M1 - In-process and on screen`, `M2 - Stereo world`, `M3 - Quad-wield`, `M4 - Noir polish 1.0`. Kept in sync with the repo's GitHub Releases page once releases exist |
| Cycles | **Not used.** One developer plus agent sessions; milestones carry the schedule |
| Estimates | **Not used.** If a ticket is too big to judge at a glance, split it |

**Only the user creates a milestone.** An agent never invents one, never splits the board into
a ladder of versions nobody asked for, and never files a ticket against a milestone that does
not exist yet. If work does not fit the current milestone, say so and ask; the answer is
usually that it fits after all, and sometimes it is a new milestone the user then creates.

This is the same rule as the release ritual below, for the same reason: how the work is carved
into shippable pieces is a product decision, and an agent guessing at it produces a board that
looks organised and describes a plan nobody agreed to.

`docs/ROADMAP.md` keeps the engineering ladder (the research gate R0-R7, then stages S0 to
S9), and each milestone's description names which stages it closes, so the two views agree
without either being a copy of the other: **the milestone is the destination, the roadmap is
the route.**

### Why two branches

Dishonored ran on one branch until 2026-09-24. A tester zip and then its 1.0.0 release came off
the same tip as work that was still being judged, and the 1.0.1 hotfix was tagged from a stack
of draft PRs the release branch did not contain. With several agent sessions opening PRs, the
question "what can a player install right now" has to be answerable from a branch name alone.

So, from the first commit here: **work integrates on `staging`; releases live on `main`.** A
feature branch comes off `staging` and its PR targets `staging`. When the user names a version,
one release PR carries `staging` into `main`, the tag goes on the `main` tip, and the two
branches are equal again until the next PR lands on `staging`. A hotfix is an ordinary PR to
`staging` followed by an immediate release PR; nothing ever lands on `main` by another route.
`git log main` is the release history, and every commit on it is in some tag.

The repository's default branch stays `main` so the landing page and the issue-template links
show released code. That means **a PR opened without `--base staging` targets the wrong
branch.** Check the base before asking for review.

## Statuses

Linear's categories are fixed (Backlog, Unstarted, Started, Completed, Canceled, Duplicate) and
every status belongs to exactly one. The team's set, shared with every mod in the org:

| Status | Category | What it means here |
|---|---|---|
| **Backlog** | backlog | Accepted and real, not scheduled. **This is the inbox**: anyone files straight here |
| **Todo** | unstarted | Scheduled for the current milestone. The next thing someone picks up |
| **In Progress** | started | A branch exists. Linear sets this automatically when the PR opens |
| **In Review** | started | The PR is ready to read, with its simulator results and its headset run already in the body |
| **Done** | completed | Merged to `staging` and its pass criteria measured. Not yet in anyone's hands |
| **Released** | completed | Carried into `main` by a release PR and shipped as a tagged build on the GitHub Releases page |
| **Canceled** | canceled | Not doing it. The comment says why |
| **Duplicate** | duplicate | System-managed by Linear |

Two states this project deliberately does **not** have, because both were considered and
rejected on Dishonored:

- **No Triage.** Backlog is the inbox. A queue someone has to drain is not worth a third state.
- **No "needs headset test".** A PR must not merge without a headset run where one is owed, so
  the state would never be occupied. The headset run happens **before** review ends and its
  result goes in the PR body. Where acceptance is purely perceptual, the ticket carries
  `needs-headset` and the work of judging it *is* the ticket.

`Done` and `Released` are separate because they answer different questions. `Done` means the
code is on `staging`. `Released` means a person can install it: the release PR has carried
`staging` into `main` and the tag is on that tip. Between them sit the docs reconciliation,
the packaging and the tag, and that gap is where a project starts believing it has shipped
things it has not.

## Priority

Linear gives five levels and no more, on purpose. What earns each one here:

| Priority | Earns it |
|---|---|
| **Urgent** | A tester cannot play, or cannot trust what they are seeing. In M1: a research verdict every later stage depends on |
| **High** | The current milestone cannot close without it |
| **Medium** | A real fault with a workaround, or a feature the milestone wants but can ship without |
| **Low** | Polish, carried items, cleanups: anything whose absence nobody will notice |
| **None** | Not triaged yet. Should not survive a week |

Priority is about **the current milestone**, not about the project forever. An M4 item is
`Low` today and may be `High` when M4 is the milestone in flight. Re-prioritise when a
milestone opens; do not agonise over it before then.

## Labels

- **`Type` (a group, so exactly one per ticket)**: `Bug`, `Feature`, `Improvement`, `Docs`,
  `Chore`, `Research`, `Regression`.
  `Research` is the one worth explaining: its deliverable is a **measurement and a written
  verdict**, not code. It closes with the answer, and the answer opens whatever ticket it
  implies. A `Research` ticket that closes with "and we fixed it" was mislabelled. M1 is
  mostly Research tickets on purpose: the research gate.
- **`area:` labels (as many as apply)**: `render`, `camera`, `input`, `hands`, `hud`,
  `runtime`, `perf`, `tooling`, `engine`, `config`, and the five this game added:
  `tentacles` (the demon arms), `anim` (animation control), `models` (model control and the
  cache), `lua` (the engine's Lua VM and SWIG API), `weapons` (guns in the hands, dual wield).
- **Flags**: `needs-headset` (acceptance is perceptual; the simulator cannot close it),
  `needs-simulator` (has a sequence or an eye-check leg that must pass first), `blocked`,
  `good-first-issue`. `gingas-parity` belongs to Dishonored and is not used here.

## The ticket template

A ticket has two readers and they want different things. A human wants to know what is wrong
and whether it matters. An agent needs enough to act without this conversation: the files, the
levers that already exist, what not to touch, and how anyone will know it worked.

```markdown
## Summary
<Two to four plain sentences. What a player or a maintainer would notice, and what changes.
 No jargon, no engine internals. If someone read only this, they should know whether they
 care.>

## Why now
<The milestone it serves and the evidence that it matters: a measurement, a log line, a
 headset verdict, a count. "It feels wrong" is a starting point, not a reason.>

## Technical detail
<The lane (present thread, game thread, Lua), the seam, the files, the existing levers to
 reuse, and what is already known. Link ENGINE_NOTES and STATUS rather than restating them.
 Name what has already been falsified so nobody re-walks it.>

## Blast radius
<What else this touches, what could regress, and what the fail-soft is. If a wrong fix here
 puts the player through a floor, say so.>

## Non-goals
<Explicitly out of scope, so the work does not widen. This is the most valuable section for
 an agent and the one most often left out.>

## Pass criteria
<Falsifiable, measured, in the order they should be run. Name the tool and the number.
 Simulator first, headset last.>
- [ ] simulator: <command> -> <expected>
- [ ] headset: <what a human must judge, and the A/B that would disprove it>

## Links
<ENGINE_NOTES sections, STATUS run numbers, the PR, related tickets.>
```

Small tickets may drop `Why now`, `Blast radius` and `Non-goals`. **No ticket ships without
`Summary` and `Pass criteria`.** A ticket with no pass criteria cannot be finished, only
abandoned.

**This file is the template's home.** It is deliberately not installed as a Linear issue
template: a template makes every ticket open with a wall of empty headings, and a form nobody
fills in is worse than a convention people follow. Copy the block above when you need it, and
drop the sections that do not earn their place.

### What makes pass criteria good here

The sibling mods' own rules, applied to acceptance:

- **A verified write is not an honoured one.** "The field now reads 110" is not acceptance.
  "The projection constant uploaded to the shader changed by the predicted amount" is.
- **An instrument that cannot fail its own hypothesis is not evidence.** If the check can only
  come out one way, it is not a check.
- **A counter is not evidence until you know its population.** Say what would make it move.
- **Simulator first, headset last.** Anything the simulated runtime can answer must be answered
  there. A headset run costs a person their evening.
- **One rig is one data point.** Where a default is being chosen, say so and ask for a second.

## The flow

### 1. Find the ticket, or write it

Search Linear before creating anything. Half of what gets "found" is already filed.

If it does not exist, create it from the template with **project, milestone, priority and at
least a `Type` label filled in**. A ticket missing those is invisible on every view that
matters.

If the work is not in the current milestone and is not urgent, it still gets filed. Filing is
cheap; remembering is not.

### 2. Branch

```
claude/vr-<n>-<short-slug>
```

(`codex/vr-<n>-<short-slug>` for Codex sessions.)

**Do NOT copy the branch name from the ticket.** Linear's "copy git branch name"
(`Ctrl + Shift + .`) prefixes it with the Linear username, which is derived from the account's
email address and is usually a real person's name. This repository is PUBLIC and a branch name
is permanent: it survives in every closed pull request, and GitHub has no way to delete a pull
request once it exists. Type the branch by hand instead: the prefix is always `claude` or
`codex`, never a username, a handle, an email local-part or any part of a person's name.

Branch off `staging` (`git checkout -b claude/vr-<n>-<slug> origin/staging`), never off
`main`: `main` is the last release, and a branch off it is missing everything that has landed
since.

The branch name alone is enough for Linear to link the PR, but the PR body says it too, because
branches get renamed and merged bodies do not.

### 3. Work

Nothing about the engineering rules changes. `CLAUDE.md` still governs: one behavioural change
per build, findings to ENGINE_NOTES in the same commit as the code, every render lever default
OFF with a live A/B toggle, no em dashes, no game-derived content, byte-verified hooks, no
patching of the exe on disk.

Commit messages stay plain conventional commits, imperative, subject 72 characters or less, no
trailers and no AI attribution. **Do not put `Fixes VR-<n>` in a commit message**: on GitHub a
magic word in a commit moves the issue when the commit reaches the default branch, which
double-fires against the PR and clutters the ticket. The PR body is the single link.

### 4. Open the pull request

```
gh pr create --base staging ...
```

**The first line of the PR body is the link:**

```
Fixes VR-42
```

- `Fixes VR-<n>` when the PR's base is `staging`. Merging it closes the ticket.
- `Ref VR-<n>` when the PR's base is a working branch (a stacked PR). It links without closing,
  so only the PR that actually reaches `staging` marks the ticket Done.
- Several tickets on one PR: `Fixes VR-42, VR-43`.
- To attach a PR to a ticket with no status effect at all, use `Ref`.
- The release PR (`staging` -> `main`) carries **no** `Fixes` or `Ref` line. Its tickets are
  already Done; a magic word there would fire a second transition against every one of them. It
  lists the tickets by id in plain text instead, which is what the release notes are built from.

The PR title is a conventional-commit subject, the same shape as a commit: `feat:`, `fix:`,
`docs:`, `build:`, `tools:`, `chore:`, `refactor:`.

Then fill in `.github/PULL_REQUEST_TEMPLATE.md`. The contract it encodes:

- **What a player would notice**, before any mechanism.
- **The evidence**: numbers, before and after, log lines, run identifiers. A PR that changes
  behaviour and shows no measurement is not ready to read.
- **Every new lever, its default and its live A/B.** Default OFF unless there is a stated
  reason, and an exception is argued in the body rather than assumed.
- **Blast radius and fail-soft.**
- **What is deliberately not here**, with the reason. Faults found and not fixed get their own
  ticket, and the PR names it. That is how a PR hands over what it learned instead of losing
  it.
- **Testing**: which simulator sequences ran, which headset runs happened on which rig, and
  build, lint, exports and the golden ini.

### 5. Review

Opening the PR moves the ticket to **In Progress** automatically. Requesting review moves it to
**In Review**. (Both need the automation rows below and the `linear` GitHub App installed.)

The reviewer's job is not to re-derive the change. It is to ask:

- Does the evidence support the claim? A parked commit's message is not evidence.
- Was the build that produced the verdict actually installed? A log banner and a file hash
  settle it. Dishonored's first "it's fixed" was measured on a binary that had been compiled
  and never installed.
- Does every new lever have an A/B, and does every refused guard log why, with the values?
- Are the docs in the same PR as the code they describe?

Review comments go on the PR. **Measurements, verdicts and decisions go on the ticket**, because
the ticket outlives the branch and someone will look for them in six months.

### 6. Merge

Merge to `staging`, **with the user's explicit permission, every time**. A passing build, a
finished feature or a kind word about the work is not permission; only the user saying to merge
it is. Linear moves the ticket to **Done**. Delete the branch. `main` is not touched here: it
moves only in the release ritual below.

Then the session-end ritual from `CLAUDE.md`: rewrite "Current state" and "Next steps" in
`docs/STATUS.md`, append a dated session log entry, tick `docs/ROADMAP.md` boxes, add a dated
entry to `docs/ARCHITECTURE.md`'s decision log if a non-obvious choice was made, push.

If a group of tickets closed together, post a **batch project update**. Not one per ticket.

## Project updates

Two kinds, both on the project in Linear. Health is one of three values: **On track**,
**At risk**, **Off track**. Post honestly; a permanently green project is one whose updates
nobody reads.

**Keep them short.** An update is a scan, not a report. One line per item, the number that
makes it credible, and nothing else. If it takes more than about thirty seconds to read, the
detail belongs on a ticket or in `docs/` and the update should link there instead.

### The batch update

Posted when a group of work lands, or roughly weekly while work is active.

```markdown
**<dates>, <n> sessions, PRs #<a> to #<b>.**

**Shipped**
- <what changed, and the number that proves it>
- <one line each, six at most>

**Open**
<count> tickets. Next up: <three or four things, named>.

**Risks**
- <what the work rests on that might not hold>

**Blocked**
- <only if something is, with who or what it waits on>
```

Rules for the bullets: lead with what changed for a player, not the mechanism. Carry the
number, never the derivation. A falsified hypothesis is a result and earns a line. Reference
tickets by id rather than re-explaining them.

### The release update

Posted when a version ships. Separate from the batch update, because its audience includes
people who do not read the board.

```markdown
**Release <version>** - tag `<tag>`, <date>, <n> tickets.

**New**
- <in plain language, no internal names>

**Fixed**
- <same>

**Known issues**
- <what is still wrong and the workaround; link KNOWN_ISSUES.md>

**Upgrading**
- <settings removed or renamed, anything that breaks a tuned ini>
```

## The release ritual

**Only a human declares a release. An agent never does, and never proposes a version number as
though the decision has been made.** An agent may report that a milestone's tickets are all Done
and ask whether to cut it. That is the whole of an agent's role here.

When the user names the version:

1. **Check the milestone.** Every ticket in it is `Done`. Anything that is not either moves to
   the next milestone or the release waits.
2. **Reconcile the documents** on `staging`. `docs/STATUS.md` "Current state" describes HEAD.
   `docs/RELEASE_NOTES.md` has exactly one section for this version and it stops saying
   `(unreleased)`. `docs/KNOWN_ISSUES.md` and `docs/ROADMAP.md` match the code.
3. **Open the release PR.** Base `main`, head `staging`, title `release: v<version>`. The body
   lists the tickets by id in plain text and carries no `Fixes`/`Ref` line. **The user merges
   it** (a merge commit, so `main`'s history shows one entry per release).
4. **Build the artifact** from the `main` tip with `tools\package.ps1` (once it exists). It
   refuses on a `-dirty` tree, because a log from a dirty build cannot be traced to a commit.
5. **Tag and publish.** A git tag `vX.Y.Z` on the `main` tip, then a GitHub release with the
   zip and the launcher exe attached. The release notes come from the milestone's tickets; the
   same content goes into `docs/RELEASE_NOTES.md`.
6. **Post the release update** on the Linear project.
7. **Move every ticket in the milestone from `Done` to `Released`.**
8. **Close the milestone.** The next one is the user's to create, if they want one.

The invariant all of this protects: **`main`'s tip is always the latest release tag.**

Step 7 is the one that gets skipped and it is the one that makes `Released` worth having. A
ticket sitting in `Done` after its version shipped is a lie about what a player can install.

## Working with agents

Agents (Claude Code sessions, Codex sessions, Linear's own agent) follow the same flow with four
additions:

- **An agent never declares a release**, per above.
- **An agent never creates a milestone.** It may report that the current one is full, or that
  something does not fit, and ask.
- **An agent creates the ticket if it is missing**, from the template, rather than doing
  untracked work. A change with no ticket is a change nobody else can see coming.
- **Delegation is not assignment.** In Linear, delegating an issue to an agent keeps the human
  as assignee and owner. The human stays responsible for the verdict.

Ticket descriptions are also agent prompts, so the sections that constrain matter most:
`Technical detail` naming the files and the levers that already exist, `Non-goals` saying what
not to touch, and `Pass criteria` giving something falsifiable to aim at. A ticket that says
"make the tentacles work" produces a session of exploration; one that names the pose-override
layer, says the clavicle bones are children of the camera bone, and asks for the shoulder
position logged at two head yaws produces a change.

### The workspace agent guidance

Linear can hand every agent a block of markdown before it starts (Settings > Agents >
Additional guidance, workspace-wide, with per-team guidance taking priority). This is the
text to paste there. It is org-wide, so it carries only what is true of every mod repo and
points at each repo's own `CLAUDE.md` for the rest. (This version fixes a contradiction in the
Dishonored copy, which said never to use Linear's copy-branch format and then said to copy it.)

```markdown
# VR Stereo Hub - agent guidance

Every repo in this org is a single-player VR mod for a game we own, running locally on our
own machines: stereo rendering, head tracking, motion controls, renderer and input work.
Write about them that way, in code, comments, commits, PRs and issues.

## Referencing issues in commits and pull requests

- Branch: `claude/vr-<number>-<slug>` (or `codex/...`). Type it by hand. Never use Linear's
  copy-branch-name format: it carries the account holder's name into a public repository
  permanently.
- PRs target the repo's INTEGRATION branch (`staging` in every repo that has one; the repo's
  `CLAUDE.md` names it), never its release branch. The release branch moves only by the
  release PR the user merges.
- The FIRST line of the PR body is the link: `Fixes VR-123` when the PR targets the
  integration branch, `Ref VR-123` when it targets a working branch, so that only the PR
  which actually reaches the integration branch closes the issue. The release PR carries
  neither.
- Never put a magic word in a commit message. The PR body is the single link; a magic word
  in a commit fires independently and produces duplicate transitions.
- PR title: a conventional-commit subject (feat: / fix: / docs: / build: / tools: / chore: /
  refactor:), imperative, 72 characters or less.

## Before writing any code

Read the repo's `CLAUDE.md` and its docs index. They carry hard rules that were paid for, and
they override anything assumed from experience with other codebases. Never copy a constant,
an address or an offset from another game.

## How work is judged here

- Acceptance is a MEASURED effect, not landed code. A verified write is not an honoured one.
- Validate in the simulator before asking a human for a headset run. A headset run costs a
  person their evening.
- Every new render lever ships default OFF with a live A/B toggle.
- Every refused guard logs why, with the values that produced the refusal.
- An instrument that cannot fail its own hypothesis is not evidence, and a counter is not
  evidence until you know its population.
- A falsified hypothesis is a result. Write it down so nobody re-walks it.

## Hard rules

- Never commit game-derived content: no decompiled script, no extracted assets, no frame
  dumps, captures or crash dumps.
- No em dashes anywhere, in code, strings, docs, scripts or commits. Use "-".
- Never quote a chat verbatim in anything published. Report the observation instead;
  numbers, log lines and config keys stay quotable.
- An agent NEVER declares a release and NEVER creates a milestone. It may report that a
  milestone's issues are all Done and ask.
- File an issue for any fault found and deliberately not fixed, and name it in the PR.
```

## Never quote a chat verbatim

It applies to everything published: commit messages, PR bodies, Linear tickets and comments,
code comments, `docs/`.

A perceptual report is evidence and belongs in the record. Its exact wording never is. Report
the observation and every fact survives, without putting someone's casual sentence in front of
strangers permanently. Numbers, log lines, ini keys, the game's own shipped strings and a
person's own written PR body stay quotable.

## What only the Linear UI can do

The MCP cannot reach any of these. If something below looks wrong, it is a settings change, not
a bug in a script.

| Setting | Where |
|---|---|
| Create or reorder workflow statuses | Settings > Team > Issue statuses and automations |
| PR automation rows and branch-specific rules | same page |
| Branch name format | Settings > Integrations > GitHub > Branch format |
| "On git branch copy, move issue to started status" | Settings > Account > Code and reviews |
| Agent guidance, workspace and per-team | Settings > Agents > Additional guidance |

Issue templates are also UI-only, but this project does not use one on purpose: the ticket
template above is the version of record and lives here.

### Linear installs two separate GitHub Apps, and you need both

This cost Dishonored a session's worth of confusion, and on 2026-09-25 the check below showed
the org still has only one of them. Linear has:

- **`linear-code`**: code access. Powers the Reviews surface, diffs, file contents, title and
  state sync, and coding sessions.
- **`linear`**: the issue integration. Magic-word linking, linkback comments on the PR, and
  every PR automation row above.

**They install separately and neither implies the other.** With only `linear-code` installed,
everything looks healthy from inside Linear (PRs appear in Reviews, titles and diffs sync
within seconds of a push) while `Fixes VR-<n>` does nothing at all, on a freshly opened PR as
well as an edited one, and no linkback comment ever appears.

The check that settles it, from a machine with `gh` authenticated to the org:

```bash
gh api orgs/VR-Stereo-Hub/installations --jq '.installations[] | "\(.app_slug)  selection=\(.repository_selection)"'
```

Both `linear` and `linear-code` must be listed. If only `linear-code` is, connect GitHub from
Linear's own GitHub integration settings and accept the app install on the org. Enabling
linking does **not** retroactively scan existing PRs: after fixing the install, re-save one PR
description (adding and removing a character is enough) to fire a fresh webhook, then confirm
the link appears on the issue. `list_diffs` on the MCP shows `linkedIssues` per PR; empty
before, non-empty after, is the proof.

The automation rows this project wants (team VR, so they apply to every mod repo):

| Row | Value |
|---|---|
| On pull request opened | In Progress |
| On review requested or activity | In Review |
| On PR or commit merge | Done, **restricted to base `staging`** |
| Any other base branch (including `main`) | no action |

The restriction is what makes stacked PRs behave: a PR merged into a working branch must not
close its ticket, because the change has not reached `staging` yet. It also keeps the release
PR inert: merging `staging` into `main` must not move anything, because those tickets are
already Done and go to Released by hand in the release ritual.

## Quick reference

```
1. Search Linear. Create from the template if it is not there.
   Project + milestone + priority + Type label, always.
2. Branch: claude/vr-<n>-<slug>, off staging. Type it; do NOT copy it from the ticket.
3. Work. Simulator first. Headset last. ENGINE_NOTES in the same commit.
4. PR base: staging (gh pr create --base staging). Body line 1: "Fixes VR-<n>" into
   staging, "Ref VR-<n>" into a working branch. Title: a conventional-commit subject.
5. Fill the PR template. Evidence, levers and defaults, blast radius,
   what is deliberately not here, testing.
6. Review, then merge to staging WITH THE USER'S YES. Linear marks it Done.
7. STATUS, ROADMAP boxes, decision log, push. Batch project update if several closed.
8. Release only when the user says so: release PR staging -> main (the user merges),
   tag the main tip, publish. Then Done -> Released, milestone closed.

Never invent a milestone or a release. Both are the user's call.
main is the release. Nothing lands on it except the release PR.
```

## Related documents

| File | Purpose |
|---|---|
| `CLAUDE.md` | The short version of these rules, plus every engineering rule |
| `docs/STATUS.md` | Session handoff: current state, next steps, blockers, session log |
| `docs/ROADMAP.md` | The research gate and the ladder S0 to S9 |
| `docs/VERIFICATION.md` | Intent, to tool, to command, to how to read the result |
| `docs/RELEASE_NOTES.md` | Per-version notes; the release ritual writes here |
| `docs/KNOWN_ISSUES.md` | User-facing known issues; ships in the zip |
