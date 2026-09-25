# Traps and the graveyard

Two sibling mods in this org paid for everything below: the BioShock trilogy mod (three
engines, about 74 sessions) and the Dishonored mod (one engine, about 40 sessions to 1.0.1).
This file carries their traps into a repo that has not written a line of code yet, so the
first line written here does not repeat them. It is grouped by the CLASS of mistake, because
the class is what recurs; the specific engine detail is the example.

Every entry names its source so the full story can be read there:

- **DH TRAPS** = `docs/TRAPS.md` in the Dishonored repo (branch `staging`).
- **DH ARCH** = the decision log in Dishonored's `docs/ARCHITECTURE.md`.
- **DH GINGAS** = `docs/dishonored/HANDOFF-GINGASVR.md` (the original author's handoff).
- **DH <topic>** = a per-topic doc under `docs/dishonored/` there.
- **BS ARCH** = the decision log in the BioShock repo's `docs/ARCHITECTURE.md` (branch
  `origin/staging`), about 2,600 lines of dated lessons.
- **BS RULES** = "Rules carried over" at the top of `docs/bioshockinfinite/ENGINE_NOTES.md`.
- **BS <file>** = other docs in that repo.

**How to use this file.** A setting that "does not work" is a section 1 question before it is
a code question. A number that looks conclusive is a section 2 question. Before proposing an
approach, grep section 11 and `docs/darkness2/ENGINE_NOTES.md` "Dead ends". New traps and
failed plans go in section 12, in the same commit as the work that found them, in the entry
format at the bottom.

---

## 1. The stale-setting class: check this FIRST when a value "does nothing"

The shape is always the same: a setting has more than one place it can live, the place you
edited is not the place that decides, and nothing says so. Two whole Dishonored sessions went
to this class.

- **An ini key that exists beats every compiled default.** Two builds changed a compiled
  default; the installed ini still named the old value; the loader reads a compiled default
  only when the key is ABSENT. Both builds ran the old value, the results were read as "the
  new value failed", and the true cause of a judder was wrongly eliminated. (DH TRAPS 1, VR-65)
- **A setting with two persistent homes has no owner.** The render size lived in two files;
  editing one left the engine asking for a five-day-old size. Two attempts went to editing
  the key again. (DH TRAPS 1, VR-66)
- **A preset file that loads later overrides the shipped defaults**, so a "default change"
  never reaches a machine that has one. Add a schema version to the preset before the first
  default ever changes. "A default change cannot ship through a file that stores the old
  default." (BS ARCH 2026-08-23; BS TROUBLESHOOTING "flat screen in headset")
- **Changing a default every installed ini already holds** reaches nobody. Three routes: a
  new key name (orphans the value a player tuned), a config-version bump (rewrites the whole
  file and wipes tuning), or a **one-time per-key migration keyed on a marker written in
  BOTH branches**. Only the third is right. (DH TRAPS 1, VR-37 and VR-170; DH VR-159)
- **The command seam is ONE slot, polled at 1 Hz.** Two commands inside the same second are
  one command: the second write replaces the first. Four attribution rounds were lost to it.
  Send everything for one moment in one call. (DH TRAPS 1, VR-172)
- **A capture window that opens late measures the wrong half.** When an A/B result is an
  ABSENCE, check the rows contain the event at all before crediting the lever. (DH TRAPS 1)
- **Environment variables beat the ini, and apply before the ini is read at all** (log
  verbosity, the data dir, the runtime JSON). A harness reading the wrong one finds an empty
  directory. (DH TRAPS 1 "where the settings live"; DH VERIFICATION gotcha 14)
- **A runtime chosen in the ini silently beats the one chosen in SteamVR or Virtual Desktop.**
  (DH STATUS process notes 2026-09-19)
- **The game rewrites its own ini at exit**, and a hard kill leaves the mod's values baked in
  it. (BS TROUBLESHOOTING; BS KNOWN-ISSUES) Here the game's config is obfuscated and the mod
  never writes it, which removes this class by construction: see
  `docs/darkness2/GAME_CONFIG_MAP.md`.
- **Refuse and log an out-of-range value; never clamp silently.** An ini save once persisted
  a whole block with one wrong literal; the loader that clamps must log the clamped value.
  Echo the whole resolved config at startup. (DH TRAPS "a save that persists a whole
  block"; BS CONTROLS)
- **Before touching a key**: find every place the value can live (grep `src/`, `tools/`, the
  game's own config); read what the run RESOLVED it to, not what you wrote, and if a lever does
  not log its effective value and source, add the line before running the experiment;
  confirm the change reached the consumer; **do not wipe and reinstall to diagnose this**, it
  has never once been the answer and it destroys the evidence. (DH TRAPS 1)

## 2. Instruments that could not fail their own hypothesis

Every one of these produced a confident number that meant nothing, because the instrument was
not able to print the unwelcome answer.

- **A counter read across two threads** produced a 39% "disagreement" figure. Retracted.
  (DH TRAPS 2)
- **An angle differenced between two coordinate systems** (an engine world yaw against an XR
  tracking yaw, then doubled by a sign convention) produced a 51-degree error. Retracted.
  (DH TRAPS 2)
- **A comparison circular by construction**: a camera compared against the head values that
  camera was built from can only ever print zero. (DH TRAPS 2)
- **A control that never ran** reported `passed 0 FAILED 0` for three builds because it counted
  records opened rather than checks performed. (DH TRAPS 2)
- **A threshold picked before anything was measured.** The `EVEN CADENCE` test forgave
  `|off| > 0.06`, so a beat every twenty frames printed as a clean bill of health, and that
  was read as falsifying the cadence hypothesis, which was correct. (DH TRAPS 2)
- **An average taken across a deliberate switch** destroys the signal the switch exists to
  produce. (DH TRAPS 2, VR-65)
- **Two counters that read 0 by design** were read as "the hands are dead" by three separate
  readers. A zero that is expected must say so on its own line. (DH TRAPS 2; DH CLAUDE.md
  Logging)
- **A negative result needs a positive control.** A zero from an instrument that has never
  been seen to fire is not evidence. (BS ARCH s30; BS RULES)
- **A counter is not evidence until you know which population it counts.** The wrong counter
  on the same line was read; a hitch tally was taken over the wrong population; windows were
  joined by eye. "Where a wait is observed does not identify who caused it." (DH TRAPS 2,
  VR-67 and VR-68; BS ARCH s30)
- **A measured value must carry the identity of what it measured**, not merely its freshness.
  `src=live` said where a number came from, never that it was right; a yaw warp came from the
  wrong render pass. (BS ARCH s28; BS RULES)
- **A single sample assumes homogeneity.** A frame is not homogeneous. Sample strided, vote,
  refuse to publish without a majority, and publish the runner-up as a named second thing.
  (BS RULES)
- **An instrument that reports SUCCESS when it fails to measure is worse than no instrument.**
  `lenses == 1` was read as acceptance when it meant "could not measure". A log printed its
  variables after zeroing them. (BS ARCH s33, s68e)
- **A probe must be chosen for the thing measured, not inherited.** A settle probe watched a
  bone that does not move; nine captures at 250-266 ms is a flat line and a tell. (BS ARCH
  s68i)
- **A probe that depends on a quantity it does not print cannot be debugged from its own
  output.** Print the quantity your argument says does not matter. (BS STATUS s74)
- **A correlation you caused is not a measurement.** Check whether your own code closes the
  loop. An input learned from the mod's own output can only ratchet. (BS ARCH s64; DH TRAPS 2,
  VR-36)
- **A liveness guard that cannot detect freeing.** A class-name comparison catches reuse but
  not freeing; freed memory keeps its old contents until reused. Only a live-object table
  check is liveness, and that table is a snapshot that must be refreshed per level. (DH TRAPS
  2 VR-85 and VR-88; DH GINGAS trap 3)
- **"Read-only is not free."** A property watch cost frame time and produced jitter that was
  then investigated as a bug. (DH TRAPS 2, VR-85)
- **A "fresh sample" test no real sample could pass.** (DH TRAPS 2, VR-78)
- **A refusal line whose number can only ever read zero.** (DH TRAPS 2, VR-82)
- **A golden-file check that compared the golden against itself.** Prove a gate can fail
  before trusting that it passed. (DH TRAPS 2, VR-84)
- **A shared format buffer is a shared variable.** An ini save wrote one key's value into
  another key's slot. (DH TRAPS 2, VR-84)
- **A crash dump that holds none of the memory the crash was about.** Include indirectly
  referenced memory. (DH TRAPS 2, VR-96)
- **Aggregate counters read as a self-sustaining loop** when the onset was a single late tag.
  (DH TRAPS 2, VR-80)
- **A picture diff cannot judge a direction question without a positive control**, and a
  mean-abs-diff lies in a dark scene. (BS bioshockinfinite ENGINE_NOTES; BS VERIFICATION)
- **A window screenshot cannot see a quad layer**, and window captures are eye-phase-locked. A
  mono screenshot is not a sufficient check for a lens question. (BS TESTING; BS VERIFICATION
  gotcha 9)
- **A diagnostic master switch must announce what else it disables**, and gating a diagnostic
  changes every tool that reads the log (grep `tools/` too). `DVR_SKIP` once disabled nothing
  while claiming to. (BS ARCH 2026-08-23; BS STATUS s74; DH TRAPS VR-160/VR-163)

## 3. Wrong hook points and seams

- **Run a static caller census before hooking.** Zero callers on a function the engine must
  call every frame is the cheapest way to learn a hook target is dead BEFORE installing it.
  BioShock 2 had inlined the event dispatch BioShock 1 hooked; the thunk seam had zero static
  callers. (BS ARCH s24; DH CLAUDE.md "Resources")
- **Script-entry thunks are script-entry only.** Hooking them caught zero calls from the
  engine's own C++ path; hook the implementation. (BS ARCH 2026-07-25) The equivalent here:
  a SWIG wrapper is the script's way in, not necessarily the engine's own call path. Confirm
  which path fires by observation.
- **Identify a render pass by making it MOVE**, never by draw counts or size coincidence.
  Size-coincidence discriminators die silently (at a square target, UI atlases matched
  "post-FX"). A residual rule ("everything not X") has no upper bound; use positive bounds.
  (BS RULES; BS ARCH s30)
- **A probe hook's argument count must equal `ret imm / 4`.** A mismatch returns on a
  misaligned stack. In a Debug CRT that pops a Run-Time Check dialog that writes no dump;
  press Abort, never force-kill. In Release it corrupts the stack silently. Read the first
  `ret` before hooking anything. (BS RULES; DH CLAUDE.md)
- **Binary adjacency is not a calling relationship.** (BS FREEZE_HANDOFF)
- **Hoisting a block above the early returns inside a function does not help if the function
  itself is below one.** Check the whole call chain. (DH TRAPS)
- **A hook placed before a call that takes the local by address** misses the write. Count
  `lea`-then-push as a write; list every write to a local up to the join before hooking it.
  (DH TRAPS 2, VR-82)
- **Patch the CALL SITE, not the prologue**, when re-entering a draw root: deny by default,
  check the return address. (DH ARCH)
- **The re-entry root must include the present kick.** Doubling a draw that did not contain
  the present gave no second present. Rule: the smallest function whose call tree contains
  the camera sample, the scene build AND the present. (BS ARCH s40)
- **Write out-params only where the engine offers them**, so a class of bug is impossible by
  construction. (BS ARCH s39)
- **An engine consumer is redirected at its INPUT, never at its output.** Blink was aimed by
  writing its input aim vector with the engine's own magnitude kept; every output patch
  disagreed with collision or the marker. (DH ARCH VR-36)
- **Change the request, not the result.** When the engine recomputes a value every tick, do
  not race the recompute; change its input. Measure write survival first (4 writes of 186
  survived). (DH ARCH VR-30)
- **A name test is not a licence to write a component.** Names do not establish ownership; a
  pointer walk reaches world meshes (the intro boat, doors). Read the engine's own owner
  field. The wrong write put the player through a boat. (DH TRAPS 3, VR-73)
- **A ProcessEvent-style call from inside the script tick re-enters the script tick.** Add an
  `inside` guard. (DH TRAPS, VR-182)
- **A system call per object in a scan** (`RangeReadable`, `VirtualQuery`) ran 470 times a
  second. No per-frame memory scans. (DH TRAPS VR-182; BS ARCH 2026-08-23)
- **Nothing may resolve at init.** The engine's name and object tables are empty at `DllMain`
  time; resolve once, behind a sanity gate, never at init and never on a cadence; batch name
  lookups. (DH GAMEPLAY_STATE; DH TRAPS VR-165)

## 4. Stereo rendering

- **Re-entry pair coherence.** Pass 2 must replay the cached pass-1 camera plus the IPD
  offset, never re-sample the head. Eye attribution goes through a tag ring, not present
  parity. Wait with a drain poll, not the engine's racy event wait. (BS ARCH 2026-07-24)
- **The eyes swapped** because the tag ring's order broke across single-to-double
  transitions. The pairing must follow a per-present camera step, with the ring realigned on
  disagreement. (DH ROADMAP S2b "THE EYES SWAPPED")
- **A late eye tag is repaired, not invented.** (DH ARCH VR-80)
- **A zero-layer `xrEndFrame` is a black frame, not a held one.** (DH ARCH)
- **The FOV claim must match the rendered FOV**, or the headset shows fisheye and fusion
  breaks. Cinematics render their own FOV. Two lenses per frame (world and foreground)
  coincide only at 16:9: **derive lens laws at more than one aspect**. (BS ARCH s28; BS RULES)
- **Anything that changes the camera rotator must land before the eye-offset step and the
  second-pass stash**, or it ships as a per-eye double image. (BS ARCH 2026-08-23)
- **Head roll: offset the eyes along the full rotation's right axis.** Rolling the head moved
  the camera sideways in the wrong direction when the roll was left out. (BS ROADMAP M9; DH
  VR-91)
- **The judder was the pose, not the cadence.** Submitting a pose one generation too new
  judders; blaming the submit cadence was falsified in a headset. Each image goes out with
  the head orientation it was rendered from. (DH TRAPS 3; DH HEAD_MOTION_120HZ)
- **The ghosting was the cadence beat.** A render size that puts the tick at 1.05-1.11
  display periods doubles edges; 1.00-1.02 does not. Pick the size for the refresh rate and
  watch the pair rate, not the tick mean. (DH ROADMAP S2b; DH TRAPS "Watch the pair rate")
- **Occlusion culling is not per eye by default**: an object culled from one eye vanished from
  both. (DH VR-79)
- **A single scene draw is not a cinematic exit.** (DH TRAPS VR-70)
- **An old completed D3D9 query does not submit new work.** (DH TRAPS 2026-09-14)
- **Delivered-pixel tags do not identify the live backbuffer.** Deferred capture returns
  previous-present pixels with their own tag. Keep current-draw and delivered-texture
  identities separate. (DH TRAPS VR-76)
- **A windowed device at the eye's size is clamped to the desktop's rows.** The fullscreen
  path had no clamp; the console `setres` was inert; writing the size into the game's ini was
  inert. Ask through the route the engine honours, and measure that it did. (DH TRAPS 3)
- **Backbuffer detectors sample before the mod's own writers**, and `SetRenderTarget` resets
  the viewport. (BS RULES; DH HANDOFF-HUD-ELEMENTS)
- **Never take a reference to an engine D3D object from inside a detour.** Two "safety"
  AddRef guards in two consecutive sessions each made a worse failure. When adding a guard,
  ask what the guard itself can break. (BS RULES)
- **The `hkReset` law**: every `POOL_DEFAULT` D3D9 resource must be released before the device
  resets and recreated after, or the game dies on every alt-tab and resolution change. (DH
  GINGAS trap 1)
- **An explicit adapter needs `D3D_DRIVER_TYPE_UNKNOWN`**, and the D3D11 device must be
  created on the runtime's adapter LUID. (DH GINGAS trap 7 and section 7)
- **Every new render lever ships default OFF with a live A/B toggle**, so a headset report can
  be bisected in-session without a rebuild. A lever with one working direction is not an
  A/B. (BS RULES; DH TRAPS VR-158)
- **A draw-duplication plus vertex-shader stereo approach was rejected** (a long tail of
  per-shader fixes, nothing carries between games). A Vulkan translation layer was removed.
  (BS ARCH stereo ladder; DH CLAUDE.md)

## 5. UI and HUD in stereo

- **Size-coincidence discriminators die silently; prefer properties true by construction.**
  (BS ARCH s30)
- **A residual rule has no upper bound.** (BS ARCH s30)
- **Full-screen effects and vitals fills can be identical draws**; routing one steals the
  other's colour. (BS ROADMAP M9)
- **Cinematic letterbox bars are a UI draw, not a squeeze.** An "unsqueeze" undid nothing and
  was deleted. (BS ROADMAP M9)
- **One movie name was three defects**: a pause movie on the stack during rides and scripted
  scenes caused false UI quads. A HUD gate needs a scene condition. (BS ARCH s64 part 2)
- **ImGui since 1.87 is event-driven**: direct `io.MousePos` writes are lost; use the
  `AddMouse*Event` calls. The viewport must come from the backbuffer, not the window client
  rect (which is clamped to the monitor, and cut half the panel). Each eye is its own present,
  so a bailed pose read gives a one-eye cursor. Sliders jump to the click point; use relative
  navigation. Trickle queues cause runaway scroll. Speeds are per second, not per frame.
  `io.IniFilename = nullptr` persists nothing. ImGui only from the overlay's draw callback.
  (BS ARCH s63; DH F10_MOTION_CONTROLS)
- **Anchored screens: use yaw `atan2(-fx, -fz)`** (the other form mirrors), never latch an
  anchor from an invalid pose, and any test that does not turn the head cannot see the bug.
  (BS ARCH 2026-08-21)
- **Each new menu opens where the player looks**; a reader keeps the LOCAL quaternion from
  when it opened. Re-park per menu, not once per session. (DH VR-207; DH HUD_ANCHORS)
- **A recogniser that must see a thing in one place first fails when it starts elsewhere.** A
  remembered key is continuity, never identity. The isolated-icon guess split widgets.
  (DH TRAPS VR-185/186)
- **HUD grouping ownership must be retained**, and native controls can bypass panel settings.
  An exported movie frame is not the runtime HUD. (DH TRAPS 2026-09-16)
- **A reference that can be voted onto the thing it references** collapses (the palm vote
  onto the hand bone). (DH TRAPS VR-188)
- **Anything the user must judge by eye belongs in the F10 overlay, not in a seam command**,
  because alt-tab destabilises the XR session. Show the number next to the toggle. (BS ARCH
  s33-34)
- **A screen offset is not the hand's**; arm on projection MODE, not the eye tag; build HUD
  quads after the zero-layer hold; a write-only vertex buffer cannot be read. (DH
  HANDOFF-HUD-ELEMENTS traps)

## 6. Hands, weapons and animation

- **Actor pinning is a dead end.** The pivot is the eye with the mesh a metre out, both arms
  are one mesh on one actor, and the weapon renders from its attach matrix. **Drive at the
  bone level.** (BS ARCH 2026-07-26)
- **Engine-side writes let attachments and effects follow for free; render-side matrix
  patches do not.** This single principle decided BioShock's viewmodel architecture and
  Dishonored's hands. (BS RULES; DH ARCH)
- **Actors cull by origin.** A pivot correction via position offset made the rig invisible.
  (BS ARCH 2026-07-25)
- **One ray.** Anything visible that claims to point where shots go derives from the identical
  ray. Rotation offsets compose as quaternions in the controller's local frame; euler adds are
  banned (28.21 degrees of divergence measured). An existing visual API can still violate the
  contract; do the geometry before the headset run. (BS RULES; DH TRAPS VR-57)
- **Patch the foreground pipeline's INPUTS, stop countering its outputs.** Three sessions of
  counter-modelling passed flat tests while the headset percept did not move. (BS ARCH
  2026-07-27)
- **Compose on the authored pose, not over it.** Discarding the anchor frame caused a 90
  degree offset. (BS ARCH s40)
- **Never adopt the scale channel** (it compounds geometrically); never scale the attach bone;
  never write exact zero; the rig's draw scale scales bone translations, not geometry. (BS
  ARCH s41; BS ENGINE_NOTES s61, s63, s71)
- **A frame question cannot be settled by asking whether a hand looks stable.** Subtract in
  WORLD, then rotate into the head frame. Normalise the hand against the SAME head the view
  was rendered from. (DH VR-33 section 8; DH VR-68)
- **Five filters hid the target**: a palette gate, a primitive pre-filter, a per-present
  budget, a tolerance band, a contract key without the vertex shader. An ENUMERATION cannot
  exclude what it is looking for. Uniform reject counts mean the target is outside the
  population. (DH VR-33 section 8)
- **Palette size is not identity; a VB/IB pair is not one geometry; one disappearance is not
  evidence; palette bone 0 is not the grip.** (DH VR-33 section 8)
- **An index buffer and the vertex buffer it was built against are ONE artifact.** Cut
  geometry on the render thread; buffers read under WRITEONLY are validated, not trusted.
  The cut is computed in bind pose and seen animated. Hiding a pass is not hiding a mesh.
  (DH ARM_HAND_SPLIT traps)
- **A per-bone visibility poke that is really an animation control** froze the arms to the
  view. The original author had recorded it in a code comment that three passes missed.
  (DH TRAPS 3)
- **The idle animation is a weighted random draw.** Capture a canonical rest pose once per
  holdable if one is needed. (BS ARCH s68)
- **"Stop adopting" is not "go back."** Leaving the adopt mask froze the gun at the recoil
  apex; blend back. (BS ARCH s68)
- **A timer is a guess about a state.** Read the state the engine publishes; a state edge says
  when the engine changed its mind, not when the consequences finished. Freeze at the state
  edge, wait a fixed delay, snapshot, ease. (BS ARCH s68b, s68c, s68j)
- **The engine evaluates the bone array on about 5% of frames**, so counts of evaluations are
  races; use wall clock. (BS ARCH s68d)
- **Before building a measurement, check that the thing being measured actually occurs.** A
  plasmid rig never goes still. (BS ARCH s68j)
- **Stopping and handing back are different operations.** Handing back writes engine memory
  and needs a live-world interlock; releasing bones through a freed skeleton hung a save load
  and SEH did not catch it. Engine writes sit below the world-change check. (BS RULES)
- **Cancellable action does not imply native pose ownership; arm visibility and animation
  ownership are different questions; a reading attachment is a relative pose, not an opening
  pitch fit.** (DH TRAPS 2026-09-17)
- **The scripted scene owns your hands.** Hide the rig during forced moves, hold signals open
  in the producer, drop latched references on both edges. (BS ARCH s64)
- **Recycled controls were live upgrade objects**: require liveness AND retained identity; an
  unchanged pointer after a menu is not the same object. (DH TRAPS 2026-09-13; AGENTS.md)
- **A fired projectile was not checked against the equipped weapon** and got attached to the
  hand. A fact given a freshness window expires; frames are not time; a per-axis bound admits
  a corner; a name arrives empty on the draw that needs it; a guard encoded a decision rather
  than a contract. (DH TRAPS VR-57; DH VR-59)
- **Reload, sprint and melee share substitution machinery.** A/B them in the same headset
  session; flat spread metrics were falsified. (BS bioshockinfinite ENGINE_NOTES)
- **IK is opt-in**: a slightly wrong IK arm reads worse than floating hands. Mirroring for a
  left-handed mode flips winding. (BS ROADMAP post-v1)
- **Three frames (component, world, body)**: a wrong choice cost three regressions. Pin every
  degree of freedom, unwrap angles before scaling, ease only derived joints, do not measure
  against a moving reference, per-hand state stays per hand. The shoulder must use the same
  net yaw as the hand. (BS ENGINE_NOTES s72/73 standing rules; BS STATUS IK branch)
- **A detector fed once per present sees every pose twice.** Key on the hand sample's own
  generation. A speed one sample can decide is a false attack waiting for a bad sample; use a
  median of three. Dedupe by generation, not by pose. (DH TRAPS VR-37; DH PHYSICAL_SWING)
- **A gesture trail follows the animation, not the blade.** Hide it. (DH VR-171)
- **A weapon eye taken from the drawing pass executed zero times in 83,400 draws**: the stereo
  passes ran on the game thread and the palette draws on the render thread, so there was no
  stack to look up. Know which thread owns which draw. (DH TRAPS 3)

## 7. Camera and body

- **Zeroing an input does not set a value.** A pitch kill froze the engine's own pitch at
  -88.9 degrees, which is why melee missed. Servo through the game's own input instead.
  (BS ROADMAP M9)
- **Never write the field the engine is steering by.** The mod or the game may steer, never
  both. A feature that removes a gate also arms everything that gate was holding off. (BS
  ARCH s64)
- **A camera-only adjustment is never only a camera adjustment**: walk direction, aim ray and
  viewmodel owe the same term. (BS ARCH 2026-08-23)
- **A filter classifying "the game did this" must be told about the mod's own changes.** The
  freeze inverted head look because the body transfer was not in the filter. (BS ARCH
  2026-08-23)
- **Read player intent from the raw XR pad, not the composed pad.** (BS ARCH s64)
- **`GetTickCount64` cannot see a frame**; use QPC for per-frame deltas. (BS ARCH s64)
- **A comfort filter inside the head-drive block is skipped exactly when it is needed.** (BS
  ARCH s64)
- **The pre-compensation was in the wrong ORDER, not the wrong shape.** When a correction is
  provably exact and the symptom survives, measure what the other side received. (BS ARCH
  s64)
- **A lever with one purpose switched off a control with another** (a crawl tuck zeroed the
  camera's look-at). A writer that walks a list must say what is on the list; a measurement
  made under a mod lever describes the game WITH that lever. (DH TRAPS VR-122)
- **A menu that hands over to another menu is the same menu**; the head reference belongs to
  the blocked stretch. (DH TRAPS VR-166)
- **Animation-driven camera sway must stop when the animation does**; the collision-pop
  smoother kept running after a chain release. Prefer the game's own switch where one exists.
  (DH VR-165)
- **The camera-object matrix is not what the renderer draws with**; mouse-injected head
  motion swims. (DH ENGINE_NOTES dead ends)
- **The persistent FOV readback cannot use a contraction ratio**; a stale cinematic FOV left
  a square view. (DH ARCH VR-213; DH VR-227)
- **Camera modifiers overwriting each other in one frame** was a theory killed by a diagnostic
  that never fired; the FOV base race was real but not the cause. Each of the original
  author's dead ends died by measurement, not argument. (DH GINGAS section 9)

## 8. Performance, crashes and pacing

- **`xrWaitFrame` has no timeout.** Run it on a dedicated pace thread with a deadline. Never
  destroy a session while that thread is inside it. (BS ARCH s28)
- **An unfocused session must not own the game thread's frame loop.** Keep submitting but
  stop waiting; a runtime will not re-grant FOCUSED to an app that submits nothing; the
  keepalive idea wedged the game. VR enable and disable must be symmetric. Keep leaked open XR
  frames closed before waiting. (BS ARCH s26, s28, s33, s34)
- **A loading screen is not a deadlock.** Watchdog episodes the game recovers from are loads.
  (BS ARCH s23, s36)
- **Bounding an INFINITE wait turned a freeze into a crash.** The fix for a race is removing
  the wait, not narrowing it; the proof is a counter reading zero, not a soak duration. (BS
  FREEZE_HANDOFF)
- **Never init under loader lock.** Deferred init thread. Never log from static initialisers
  (a logger inside a static-init lambda stopped the DLL loading entirely, with no log at all).
  (BS TESTING; DH GINGAS trap 2)
- **Diagnostics on the present path need a hard rate limit.** One took the game to 40 fps
  then wedged it. Test instruments must not cost frame time; a PNG encode on the present
  thread re-armed the second draw and changed the eyes it was dumping. (BS ARCH s33, s34; DH
  VERIFICATION gotcha 19)
- **A legacy diagnostic compiled into a play build** froze the game a quarter second per
  trigger pull; the build directory remembered a `-Legacy` flag and the build did not say so.
  Any `[legacy]` line in the banner means a legacy build. (DH TRAPS VR-180)
- **A crash recorder whose budget the mod's own probes spend cannot record the crash**, and
  handlers run while another thread is suspended, which can deadlock. (DH TRAPS VR-177)
- **Heap scanner lessons**: it swept thread stacks and found its own argument; it had no
  dormancy latch; 3-second game-thread stalls. Live-heap walk, exclude stacks, mask the
  needle, slice with a budget, no logging inside SEH, `HeapLock` inside the guard. (BS
  ENGINE_NOTES s27; BS RELEASE_NOTES 0.8.2)
- **A pointer can pass for a frame time.** Classify by what a value IS. (DH TRAPS VR-168)
- **A cache that stops remembering must refuse, not fall back to a 100 ms walk**; a cap that
  stops remembering must also stop logging (77,992 lines). (DH TRAPS VR-165)
- **The exit crash may be the game's own.** After WM_CLOSE, log one line and terminate. (BS
  ARCH s38)
- **Steam's crash helper displaces the crash filter**; re-arm it. (BS STATUS 08-23)
- **A 64-bit implicit OpenXR API layer (OBS) breaks 32-bit `xrCreateInstance` with -32.**
  Detect it and set the layer's disable variable in-process. SteamVR has no 32-bit OpenXR
  runtime, hence the shim. (BS commit e929cfe; DH TROUBLESHOOTING)
- **A performance number carries its GPU, build tag, config, render size and refresh rate.**
  Preserve DLL, PDB and ini identity; match saves and warmup; keep tails and populations;
  D3D9 GPU spans contain gaps; present wall time is not savings; flat fps does not predict
  VR throughput. (DH TRAPS VR-160; DH PERFORMANCE evidence rules)
- **A stutter can be the game's own** (a 30-second GC tick), found by an A-B-A config change.
  No fix attempts until the cause is guaranteed by evidence. (BS ARCH s43)
- **`-onethread` style launch arguments may not be parsed; poking an engine global crashed
  loads.** Measure whether the engine is multi-threaded before designing the re-entry. (BS
  ARCH 2026-07-24)
- **A related container is not the container under investigation.** (DH TRAPS 2026-09-20)

## 9. Config, install and tooling

- **A shipped `.ps1` needs a `.cmd` wrapper**; "the tool does not work" and "the tool was never
  invoked" produce the same report. (DH TRAPS 2026-09-20)
- **The packaged ini is a byte copy of the tested machine's ini.** The zip once shipped two
  keys the dev machine did not have, so every downloader ran a different head-injection path
  from the one being tested, for days. (DH GINGAS process rule 6)
- **The inis are CRLF**; a regex eats `\r`. Duplicate keys across sections: match the section
  first. Diff the full installed ini on every install. (DH GAME_CONFIG_MAP; DH TRAPS)
- **Nothing composes the data directory by hand**; one helper, per-game subdirectories. A
  leftover `command.txt` is skipped at boot. (BS ARCH s24, s35)
- **The agent's shell on the dev PC can virtualize writes under the user profile**; a file
  visible to the shell was absent to a game launched through Steam. Point both sides at a
  real location. (DH VERIFICATION gotcha 14)
- **An elevated shell ignores `XR_RUNTIME_JSON`**; a 64-bit simulator DLL is silently skipped
  by a 32-bit process; a BOM in `command.txt` corrupts the first token; foreground the window
  before any capture; the simulator force-grants focus so session-state bugs are invisible to
  it; the simulator's display clock leaps after a game-thread hitch. (DH VERIFICATION
  gotchas; BS VERIFICATION; DH TRAPS VR-170)
- **The simulator does not auto-continue into a level**, and the attract camera dispatches
  like gameplay; every instrument measured the attract camera for six runs. Look at a capture
  before believing a number. (DH VERIFICATION gotcha 15)
- **Host tests must find the toolchain through `vswhere`**, and the build must say which
  config it is. (DH TRAPS VR-117)
- **Do not attribute logs to machines by drive letter.** Check the build tag. (DH GINGAS rule 8)
- **The log is overwritten every launch**; archive before relaunching, and the crash file must
  carry the run identity. Rotation started one deep and cost a run. (DH GINGAS trap 4; DH
  ENGINE_NOTES evidence handling)
- **A game-derived file in the tree is a licence problem and a size problem.** Tools are
  committed; their output never is; `.gitignore` covers the output directories. (Both repos)
- **Code from a repo with no LICENSE file is all-rights-reserved.** Settle it in writing
  before porting. (BS CONTRIBUTING)

## 10. Process: how sessions went wrong

- **Branch before the first edit.** The most common failure was commits made directly on the
  integration branch. (BS CONTRIBUTING)
- **Never merge without the user.** A passing build, a finished feature or a kind word is not
  permission. (Both CLAUDE.md files)
- **"Tested" is not a verification claim.** State what was and was not verified, on what rig,
  which build. (BS CONTRIBUTING; BS PR template)
- **The build that produced the verdict must be the build that was installed.** A first
  "it's fixed" was measured on a binary compiled and never installed; a stale banner was read
  as a current playtest. Log banner and file hash settle it. (DH LINEAR_AND_GITHUB review;
  DH STATUS 2026-09-24)
- **A parked commit's message is not evidence.** (DH LINEAR_AND_GITHUB review)
- **Asking the user to put the headset on for something the simulator could have answered is
  a wasted evening; claiming a simulator pass settles a perceptual question is worse.** (BS
  VERIFICATION intro)
- **One question per launch, never a command typed mid-run.** A test that needs a command
  typed mid-run is a test that does not get run. (AGENTS.md; BS STATUS s74)
- **Every fix accepted on a single check, no full playthrough**: two of three regressions
  that night were found by the user playing. (BS ENGINE_NOTES s30)
- **A symptom described in feelings is not debuggable by reading code.** Four wrong hypotheses
  on one blur, each costing a headset run. Bisect builds instead. (BS ARCH s64)
- **A mechanism that explains the symptom is not evidence that it caused it.** If a fix comes
  from reading code, say so in the commit and ship the instrument that can refute it in the
  same build. Do not attempt a fourth fix from reasoning alone. Measure before the third
  attempt. (BS ARCH s28; BS STATUS s70; BS ENGINE_NOTES s72)
- **A diagnosis the telemetry refuses is not a diagnosis.** The contradicting number sat in
  the same log line as the promoted guess. (BS ARCH s34)
- **When a fix changes nothing at all, instrument; do not theorise again.** A symptom identical
  across changes to four subsystems means none of them is involved. (BS ARCH s68d)
- **Delete a fix whose premise is disproved**; three commits of settle machinery survived being
  "refined". When a hypothesis dies, the changes made for it die with it. (BS ARCH s68g; DH
  VR-33 section 8)
- **Define a revert against a build, not a commit.** "Reverting the hand stuff" left three
  other axes rewritten. (BS ARCH s68f)
- **A per-axis audit owes a per-axis conclusion**; a two-part finding must carry both halves
  in its summary. (BS ARCH s64; BS ENGINE_NOTES s71)
- **Check what exists before building.** "This is the third time a session has proposed
  building something the panel already had." Grep the graveyard before proposing an
  approach. (BS ARCH s64; DH STATUS 2026-09-19)
- **Do not port against this tree's own measurement.** An arm hide copied another mod's
  mechanism although this repo had measured it inert. (BS ARCH 2026-08-23)
- **A claimed-safe change asserted and never checked** survived one review. Verify a
  bit-identity claim by diffing against the base. (BS STATUS 08-23/24)
- **A "Fixes" line auto-closed a ticket that was not fixed.** The magic word belongs only on a
  PR whose base is the integration branch, and only once the pass criteria are measured. (DH
  STATUS process notes)
- **Treat behaviour a tester attributed to a specific commit as a regression test.** (BS ARCH
  s68g)
- **A session that ends without pushing STATUS.md is a failed handoff.** (Both CLAUDE.md)
- **Never quote a chat verbatim; never put a person's name in the repo.** Both are permanent
  on a public GitHub. (DH CLAUDE.md hard rules)
- **Two chat-sourced claims circulated as fact and were false** (a lever "fires once", a scan
  listing read as a timeline). A directory is not a timeline. (DH GINGAS section 9)
- **Diagnostics must pay out as they are discovered**, not batch into a report after a timer
  the operator did not wait for. Three runs were wasted. (DH GINGAS trap 8)
- **Commit hygiene**: one logical, independently revertable change per commit; do not bundle
  diagnosis levers with the fix. (BS KNOWN-ISSUES)

## 11. Failed plans from the siblings: do not retry here without new evidence

These are engine-specific dead ends. They are listed because the SHAPE of each is tempting
in any engine and each was tried at least once before it died.

| Plan | Why it failed | Source |
|---|---|---|
| A console `setres` to change the render size | Reached the engine, returned empty, changed nothing | DH TRAPS 3 |
| Writing the size into the game's own config | The game created a windowed device at its own size every run and never reset out of it | DH TRAPS 3 |
| A windowed device at the eye's size | Clamped to the desktop's rows | DH TRAPS 3 |
| Blaming the submit cadence for head-turn judder | Falsified in a headset: smooth at an inconsistent presentation rate | DH TRAPS 3 |
| Taking the weapon eye from the drawing pass | Executed zero times; wrong thread | DH TRAPS 3 |
| A per-bone "visibility" array poke to hide arms | It was an animation control; arms froze to the view | DH TRAPS 3 |
| Gating a readback to cut frame gaps | Changed the gap rate not at all | DH TRAPS 3 |
| Blaming the re-entry draw for a physics fall | Fell with re-entry off; the cause was a collector clearing collision | DH TRAPS 3 |
| A neutral pad when the sample is stale | The first fresh sample still held the button | DH TRAPS 3 |
| A name test as a licence to write a component | Reached world meshes | DH TRAPS 3 |
| Draw duplication plus vertex-shader stereo (the 3Dmigoto shape) | A long tail of per-shader fixes, nothing carries between games | BS ARCH |
| A depth-reprojection stereo fallback | Dropped as moot once re-entry worked | BS ROADMAP M4 |
| Actor pinning for the viewmodel | The pivot is the eye, the arms are one mesh, the weapon renders from its attach | BS ARCH 07-26 |
| Counter-modelling the foreground lens outputs | Three sessions passed flat tests, the headset did not move | BS ARCH 07-27 |
| Bounding an infinite engine wait | Freeze became crash | BS FREEZE_HANDOFF |
| A keepalive for an unfocused XR session | Wedged the game | BS ARCH s34 |
| A timer standing in for an engine state | The engine publishes the state | BS ARCH s68b |
| Hooking a script-entry thunk for an engine event | Zero calls from the engine's own path | BS ARCH 07-25 |
| A world-anchored F10 panel | Too big; reuse the screen placement if ever wanted | BS ARCH s63 |
| The OverlayScene / WorldScreen / EyeCant XR architecture (pre-41.0 Dishonored) | Rejected; the pipeline it belonged to is gone | DH XR_HANDOFF |
| A Vulkan translation layer under a D3D9 game | Removed; the game renders natively | DH CLAUDE.md |

Each has an obvious analogue in this engine (the Lua console for resolution, the config
file, a fixed-duration finisher hand-back, a script-side hook for a C++ event). The analogue
is not forbidden; it is a plan that starts with the measurement that killed its sibling.

## 12. This repo's own traps

None yet. Entries go here, newest first, in the same commit as the work that found them.

### Entry format

```
## <One sentence naming the trap> (VR-<n>, <date>)

What was seen: <the symptom, with the number or log line>
What was actually happening: <the mechanism, measured>
What it cost: <runs, sessions, a wrong elimination>
The rule: <one sentence a future session can apply>
Where the detail is: <ENGINE_NOTES section, STATUS run, the PR>
```
