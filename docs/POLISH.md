# What finished means

The user's brief: a polished experience over a fast one, even if it takes longer. This file
says what polished means for this mod, item by item, and how each is closed: a measurement
where one exists, the user's headset judgement where it does not. Nothing here is a feature;
every item is a quality the features must have. M4 releases when this list is closed.

## The list

| What must be right | How it is closed |
|---|---|
| **World stability** | A fixed target under head rotation shows no double image; the frame-time histogram shows one tick per display period with under 1 percent misses at 90 Hz for a full level; the jump detector reports zero camera jumps over 5 cm per tick outside teleports; each image goes to the headset with the head pose it was rendered with |
| **Weapon stability, per hand** | Each gun's root within 1 cm of its own `WEAPON1` bone and its decal within 1 cm of its own ray at 5 m, measured while the other hand is firing, while reloading, and while the head turns. No one-frame jumps on menus, loads or weapon swaps |
| **Tentacle presence** | The shoulder anchor drifts under 2 cm through a 90-degree head turn; the idle amplitude matches flat on bone 16 of the chain; grab fetches the pointed object 19 times in 20; each of the 8 swing directions plays its slash; the user's written headset verdict that the arms feel like theirs (`docs/darkness2/TENTACLES.md` section 5) |
| **Comfort defaults** | Camera shake, walk bob and weapon sway measure zero at the source with the lever on; the cinematic horizon stays within 2 degrees while the head looks freely; snap turn and physical crouch on by default; every lever reachable from the F10 panel with its A/B state; the shipped ini is a byte copy of the headset-tested machine's |
| **UI placement** | Every one of the 61 HUD movies classified to an anchor or explicitly left head-locked, in a table; per-hand reticles on their rays; wrist elements readable at arm's length (text height measured at the wrist, the user's legibility judgement); each new menu opens where the player is looking; readers keep the local rotation they opened with |
| **Cinematics** | All 14 finishers and every scripted first-person scene keep the head free and the horizon upright; hand-back within one tick of the anim's end event with no pop in either direction; Bink videos on the quad without flicker at the transition |
| **Dual-wield fantasy** | Two targets hit simultaneously from two hands; the left trigger always fires the left gun whatever the game option says; no camera movement on aim; reload plays on the hand while the gun stays on the controller |
| **Performance** | The per-eye render size chosen by the cadence beat, not by taste; the per-eye cost of the tentacles and the HUD redirection logged and under 0.5 ms combined; diagnostics that cost frames ship turned off and the banner says so |
| **Fail soft** | Every hook byte-verified at load; a failed verify disables that feature, says so in `status.json` and the panel, and the game still plays flat; a wrong game build refuses code hooks and stays playable |
| **Support** | A run always produces a log, even one that dies in the first second; the crash file carries the run identity; one-click log collection bounded in size; the log can explain a failure without another run |

## The guidance that made Dishonored feel finished

Each of these is a rule with a measurement behind it in the Dishonored repo.

**World and head**

- Submit each image with the head pose it was rendered with; a pose one generation too new
  is the judder. Preserve the image's own head orientation rather than a guessed history age.
- Pick the render size so one tick is one display period at the target refresh rate; 1.05
  slots per frame doubles edges. Watch the pair rate, not the tick mean.
- Cancel the neck pivot with measured constants (standing and crouched differ); keep roll out
  of the neck arc.
- Remove camera shake, head bob and animation-driven sway at the source, per category, each
  with an A/B: "the view moves only when you do." Disable the collision-pop glide.
- Upright composition in cinematics: remove the authored tilt before applying the physical
  rotation, so the player can still look around.

**Hands and weapons**

- Normalise the hand against the SAME head the view was rendered from.
- Subtract in world, then rotate into the head frame; never ask whether a hand "looks stable".
- Keep the snapshot bound wide enough (100 ms, not 20) that a hitch does not blink the hands.
- A per-eye palette decision; a median of three for any speed; key on sample generations.
- Animation hand-back with no pop in either direction, keyed on the engine's own state edge.
- Aim from the weapon's own barrel line, measured per model. Two derivations that disagree by
  a small amount are invisible until a player misses a shot.
- Fill the unmodelled side of a one-sided weapon model.

**UI**

- Each new menu opens where the player looks; re-park per menu.
- Readers keep the local quaternion from when they opened; current head rotation must not
  swivel them.
- Mono screens are anchored in front and upright; rain, blood and vignette sit at a
  comfortable depth.
- Each HUD element has its own anchor; vitals on the wrists; the reticle stays visible behind
  the F10 panel; the cursor shows only over the panel.
- Text scale derives from eye height.

**Defaults and the panel**

- Ship the headset-tested machine's ini byte for byte.
- F10 tiers (Basic / Advanced / Debug) keep the fixes nobody should turn off out of the
  player's way; settings save on change; a reset-to-defaults that works.
- Every perceptual A/B lives in the panel, driven from the controllers, with the number next
  to the toggle.

**Gestures**

- Tune a swing threshold from a census of near misses in the log, not by feel.
- Time an attack to the game's own decision (the drop takedown held until the engine has
  found the target) rather than racing it.
- Hide effects that follow the animation instead of the hand.
