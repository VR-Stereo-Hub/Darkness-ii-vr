# The demon arms in VR

The Darkness's demon arms are the fantasy of this game, alongside the two guns. Whether they
hold up in a headset was the question this project was allowed to fail on. This document
holds the verdict on the assets (measured 2026-09-25, before any code), the risks, and the
sub-ladder that turns them into something a player owns from the shoulders.

## 1. The verdict: the assets are good enough. The risk is placement, not quality.

**What is there** (details and sizes in `GAME_ASSETS.md`):

- Each demon arm is a **32-bone spine** (`GAME_x1_DEMONARM1..32`) off a clavicle
  (`GAME_x1_TENTACLE_CLAV`), with an upper and lower jaw and a **5-bone tongue**. A chain that
  long bends smoothly and can be driven by a pose override without looking segmented.
- Materials carry diffuse, normal, specular and emissive maps at 1K to 2K. The eyes have their
  own emissives; there are "power off" and red variants. The right arm has a **bladed morph
  target** (`Blades.DemonArmSlashyBladedMorph`) unlocked by a talent.
- The animation set is complete for the mechanics: 9 idles per side, agro idles, run cycles,
  **8 directional slash animations** (up, down, left, right and the four diagonals), **41 grab
  animations** (ready, loop, launch at three ranges, return, pull strong and weak, tug of war,
  throw, javelin, shield, hearts launch/return/eat, neck kill, waist kill, mobster, door,
  fail), and **14 first-person finishers** as cinematic animations.
- The engine already has the behaviours the mod needs to drive: `DemonArmSwipeFireBehavior`,
  `DemonArmGrabStateBehavior`, a grab target and range, `mTentacleAttachBone` (the bone a
  grabbed thing rides), resist angles, eye flare, targeting tints.
- Four **secondary tentacles** (18 + 9 bones each) exist as a separate skinned mesh with
  extend / idle / retract animations.

**What could still break it**, and the ticket that answers each:

1. **The arms hang off the camera.** The rig's root is `GAME_C1_ROOT` and the camera bone is
   `GAME_C1_CAMERA`; the clavicles are, on the evidence so far, children of the camera. With
   the head decoupled from the body, arms that follow the camera swing under the chin when the
   player looks down and drift off the shoulders on every head turn. They must be
   **re-anchored to body-locked shoulders** (T2), which needs the pose override layer (A3) and
   the rig-ownership verdict (R6).
2. **The motion is keyframed, not procedural.** 4,025 `_anim.fbx` blobs; the engine has
   `LookIK`, `ReachIK` and `ProceduralAnimTreeNode`, but the tentacle motion the player sees
   is authored. "Dynamic" tentacles in VR come from the mod: root retarget (A5) so the idles
   play from the shoulder, the swing direction mapped to the 8 slash anims (T5), and a
   procedural first 150 ms where the tip follows the controller before the animation carries
   through. The keyframes are the strength here: the authored idles are what make the arms
   feel alive, and they survive re-anchoring intact.
3. **Flat-only geometry.** A first-person rig built for one camera angle may lack backfaces,
   have stretched UVs on the far side, or hide seams the flat camera never saw. In stereo the
   player leans in and looks from 30 degrees off. T1 inspects both arms up close and off-axis
   in the simulator and writes the list of parts that read wrong; T10 hides or fixes them
   through the model control layers (M1, M2).

None of the three is an asset-quality problem. All three are ordinary mod work with a
measurement at the end.

## 2. Targeting: a deliberate choice, measured

The flat game aims the arms with the view. In VR the view is the head, and the head is not
where the player is looking to aim. Two sources are built and compared (T4):

- **Head ray** (the darkness sense: whatever you look at). Cheapest, always available, and the
  way the game was designed.
- **Controller rays**: the left controller's ray picks the grab target; the right controller's
  swing direction picks the slash. This is the fantasy ("my arms do what my hands do") and it
  is what ships by default for grab if the hit rate holds.

The measurement: on a shooting-range level, the selected target matches the intended enemy
in 19 of 20 tries for the shipped source. Both sources stay as an F10 option, because a
seated player may prefer the head.

## 3. The sub-ladder

Preconditions: R6 (rig ownership) and R7 (pose buffer) verdicts; layers A1-A5 and M0-M3 from
`ANIM_AND_MODEL_CONTROL.md`; S3 steps 1-4 (pad bridge, head/body decouple, floating hands,
hands on the controllers). Every step ships default OFF with a live A/B.

| Step | What | Engine lever | The measurement that closes it |
|---|---|---|---|
| **T0 Inspect** | Extract `FPDJackie_skel.fbx`, the idles, the 8 slashes and the 41 grabs with the R5 extractor; draw the live skeleton in the mod's debug overlay from the pose buffer | R5, R7 | The overlay draws `GAME_L1_DEMONARM1..32` and `GAME_R1_DEMONARM1..32` as chains in world space; a simulator capture shows both chains following the idle |
| **T1 VR fitness verdict** | In simulator stereo, look at each arm from 30 degrees off-axis (head turned) and up close (lean in); check texture density, the blade morph, jaw and tongue, and whether the mesh was built to be seen only from the flat camera's angle | none | A written verdict with captures: keep / re-anchor only / needs model work, listing every part that reads wrong in stereo |
| **T2 Re-anchor** | Move `GAME_x1_TENTACLE_CLAV` from following `GAME_C1_CAMERA` to a body-locked shoulder anchor: yaw from the body, position from the HMD minus a neck offset, pitch NOT from the head. First candidate: `SetFPEntityClampedToCameraPitch` off; otherwise a pose write on the clavicle bones through A3 | R6, A3, `SetFPEntityClampedToCameraPitch` | Turn the head 90 degrees: the logged shoulder position varies under 2 cm; look down: the arms do not swing under the chin |
| **T3 Idle** | Keep the 9 idles per side (they sell the presence) but retarget the root to the shoulder anchor; the blend weight of idle vs pose override is a live slider | A5 | The idle continues during head turns with no pop; the A/B shows the idle motion identical in amplitude to flat, measured on bone 16 of the chain |
| **T4 Targeting source** | Build head-ray and controller-ray targeting; ship controller for grab, evaluate slash both ways | the grab state's target (found via `DemonArmGrabStateBehavior`) | A table in this doc: target selection matches the intended enemy in 19 of 20 tries on the range for the shipped source |
| **T5 Slash by swing** | Edge/sustain detector on the right controller (median-of-3 speed, honoured check), swing vector in the body frame mapped to one of the 8 directional slash anims; the anim plays retargeted from the shoulder; later, a procedural blend where the tip follows the controller for the first 150 ms | A2, A5, `DemonArmSwipeFireBehavior` | A swing in each of the 8 directions triggers the matching anim in 8 of 8 tries; the honoured check confirms the swipe fire behaviour ran once per swing, never twice |
| **T6 Grab by ray** | The grab target comes from the left controller ray hit, written engine-side into the grab state; the 41 grab anims play from the shoulder anchor; `mDemonArmGrabResistAngle` re-based to the shoulder, not the camera | `DemonArmGrabStateBehavior`, `mDemonArmGrabResistAngle` | Pointing the left controller at an object and pressing grab fetches that object in 19 of 20 tries; a thrown object leaves along the controller's forward, not the head's |
| **T7 Grabbed things follow** | Because the grab holds the object at `mTentacleAttachBone` engine-side, the object follows the re-anchored tentacle for free; verify | `mTentacleAttachBone` | A grabbed enemy stays at the tentacle tip during a full head turn; the overlay shows the attach bone and the object's origin within 5 cm |
| **T8 Finishers and heart eating as hand-back** | The 14 finisher `_cin` anims and the eat-heart grabs run canned; the mod hands the rig back: arms and hands play their authored pose, the camera anchor follows the anim's root at a damped rate, HMD rotation still applies (the head is never locked), the horizon is kept upright; hand-back ends on the anim's end event | A4 | During every one of the 14 finishers the horizon stays within 3 degrees; the canned tentacle motion is visible in the headset; control returns within one tick of the logged end event |
| **T9 Secondary tentacles** | Decide by observation whether the four chains belong to the FP rig's darkness aura or to a darkling; if FP, anchor them to the body with the shoulder logic and give them a slow procedural sway | R6 | The overlay shows all four chains anchored to the body under head turns; the A/B shows no cost over 0.3 ms per eye |
| **T10 Look great** | The hero pass: per-part visibility for the flat-only geometry T1 found; a scale slider per arm; a "closer to the face" preset; optional emissive boost of the e map while in darkness (render-side, default OFF) | M1, M2 | The user's headset judgement, written here. Nothing in this row has a numeric close |

## 4. What the flat game does that VR must not copy

- The arms are aimed with the view, and the view is offset for dual-wield aiming
  (`mDualWieldingAimViewOffset`). In VR the view never moves for aiming; see `DUAL_WIELD.md`
  W5.
- The arms retract in light. That stays: it is the game. But the retract animation is an arm
  animation on the camera rig; after T2 it plays from the shoulder.
- "If true, the DemonArms cannot grab or swipe while the anim is playing" is an engine gate on
  full-body animations. The mod's hand-back (T8) should respect the same gate rather than
  re-implement it: read the flag, do not race it.

## 5. Verdicts

| Date | Step | Build | Verdict |
|---|---|---|---|
| 2026-09-25 | asset survey | none (offline, read-only) | Assets sufficient for VR; risks are anchoring, keyframed motion and off-axis geometry; all ticketed |
