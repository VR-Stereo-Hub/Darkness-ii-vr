# Animation and model control: the layers

The user's brief for this mod: complete control over animations and complete control over
models, built before the features that need them, even if it takes longer, so that no later
feature hits a wall. This document is the layer list, in the order each one unblocks
something, with what "done" means. All of it is engine-side; render-side patches appear only
as the interim step for floating hands.

Precondition for everything: R2 (the Lua lane and the SWIG API map, which names the engine's
own animation and mesh methods) and R7 (where animations start, evaluate and output).

## Animation

| Layer | What | Unblocks | Done when |
|---|---|---|---|
| **A0 Read** | Log every animation start and stop with its name, target entity, blend time and the `IV_*` inputs that triggered it | everything below; the state flags in S5 | A named log for a full level, including idles, slashes, grabs, pistol fires, finishers |
| **A1 Allow / suppress mask** | By name and glob (`DemonArm_Slash_*`, `DualPistol_*_Aim`), applied at the play request | W6 (suppress Aim), S6 (suppress camera-driving anims the mod replaces) | A suppressed anim never appears in A0's log and its effect is visibly absent |
| **A2 Trigger** | Play any anim by name on any entity from `command.txt` and the F10 panel, through the same request function (or the SWIG method when one exists) | T5 (slash by swing), T8 (finisher preview), W6 testing | `anim play DemonArm_Slash_LeftUp` visibly plays |
| **A3 Pose override** | Per-bone write into the pose buffer after the AnimTree evaluates and before the palette is built, in the buffer's own space (R7 names it), with a per-bone weight | hands on the controllers engine-side (S3.4), T2 re-anchor, T5 procedural blend, T9 | Writing identity to `ARM2` visibly straightens the arm, and a gun attached to `WEAPON1` follows: attachments follow the write |
| **A4 Blend-back** | Hand-back is a weight ramp keyed on the anim's END EVENT (found in A0), not a fixed duration | T8, S5 hand-back during full-body moves | Control returns within one tick of the logged end event across all 14 finishers, with no pop in either direction |
| **A5 Root retarget** | Re-express an anim's root motion relative to a chosen anchor (shoulder, body) instead of `GAME_C1_CAMERA` | T3, T5, T6 | The idle plays identically from the shoulder anchor with the head turned |
| **A6 Replace** | Serve a modified anim blob from a loose override directory by hooking the cache read (M4) | any animation the user wants to author for VR | An edited idle (one keyframe moved) is visibly different in-game without touching the cache files |
| **A7 Lua chunk override** | Wrap `luaL_loadbuffer` and substitute source text for a named chunk (the source is embedded in the shipped bytecode, and Lua 5.1 loads text directly) | changing which anims scripts trigger, cutscene pacing, HUD script hooks | A one-line edit in a named script prints to the mod's log; the original chunk is untouched on disk |

## Models

| Layer | What | Unblocks | Done when |
|---|---|---|---|
| **M0 Enumerate** | List every mesh part of the FP rig at load with its name, material and bone set | everything below | A table in the log for `FPDJackie`: JackieHands, JackieBody, DemonArmGrabbyRevised, DemonArmSlashy, the morph, their materials |
| **M1 Per-part visibility** | Skip the draw by mesh-part identity: an engine-side flag if one exists (`mShowFirstPerson` is a whole-rig switch, not a part switch), a render-side skip otherwise | floating hands (hide JackieBody's sleeves), T10 (hide flat-only geometry) | Hiding `JackieBody` leaves the hands and arms floating |
| **M2 Scale** | Per-part and per-bone-chain scale through A3 | hands scaled independently (S3.8), tentacle scale (T10) | A 0.9 hand scale measured on the palm width in-world |
| **M3 Re-anchor** | Choose a part's parent frame: camera, body, shoulder, controller | T2 | The same measurement as T2 |
| **M4 Loose file override** | A cache-read hook serving files from `<mod>/override/<asset path>` | A6, model replace | A re-coloured texture from the override directory appears in-game |
| **M5 Offline extract and inspect** | The R5 extractor plus a viewer that renders a `_skel.fbx` blob's bone tree and, once the mesh layout is understood, the mesh | T1 done offline, M6 | The bone tree of `FPDJackie` rendered offline matches the live overlay of T0 |
| **M6 Re-import** | Compile a mesh back to the blob layout once M5 understands it. Long; only if a feature needs it | replacing a model | A mesh with one vertex moved shows the move in-game through M4 |

## Rules carried from the siblings, applied here

- **Drive at the bone level, engine-side.** Actor pinning and render-side matrix patches were
  dead ends on BioShock: the pivot was wrong, attachments did not follow, effects stayed
  behind. The pose override (A3) is the seam everything else uses.
- **Compose on the authored pose, never over it.** Discarding the anchor frame gave BioShock 2
  a 90-degree offset.
- **Never adopt the scale channel.** It compounds geometrically. Scale is its own layer (M2).
- **Stopping and handing back are different operations.** Stopping is always safe. Handing
  state back writes engine memory and needs a live-world interlock: BioShock hung a save load
  by releasing bones through the previous level's freed skeleton, and SEH did not catch it
  because the pages were still mapped.
- **The idle is a random draw.** The 9 idles per side are chosen by the engine; do not assume
  a canonical rest pose exists. Capture one per rig if a rest pose is needed.
- **Freeze at the state edge, not on a timer.** A timer is a guess about a state; read the
  state the engine publishes (`IV_*` inputs, the end event).
- **Arm visibility and animation ownership are different questions.** Hiding a pass is not
  hiding a mesh, and an arm the game animates can still be invisible.
- **"Stop adopting" is not "go back".** Leaving a pose mask frozen leaves the model at the
  last adopted pose (BioShock's gun stuck at the recoil apex). Blend back (A4).
- **IK is opt-in.** A slightly wrong IK arm reads worse than floating hands. The engine's own
  `ReachIK` and `LookIK` are candidates for later, behind a lever.
