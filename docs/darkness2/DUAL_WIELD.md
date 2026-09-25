# Two guns on two controllers

The flat game's "quad wielding" is two guns on the triggers plus the two demon arms on the
bumpers, with both guns aimed by one view and an aim-view offset that shifts the camera when
both are up. In VR each gun sits in its own tracked hand and is always aimed down its own
barrel. The design rule is the one Dishonored paid for, doubled: **one ray per hand**, and
nothing in the chain ever averages the two hands.

Precondition: S3 steps 1-4 (pad bridge, head/body decouple, floating hands, hands on the
controllers) and the animation layers A1, A3, A4.

## The ladder

| Step | What | Engine lever | Measurement |
|---|---|---|---|
| **W0 Read the state** | Name-resolved properties for: dual wielding active, the weapon in each hand, `SwapFireButtonsWhenDualWielding`, `mDualWieldingAimViewOffset`, the `IV_WEAPON_HAND` / `IV_WEAPON_TYPE` animation inputs | the property strings in ENGINE_NOTES 5 | `status.json` shows the values and they change when the player equips or drops a second gun |
| **W1 Per-hand placement** | Each human arm has a `WEAPON1` attach bone; with the hands on the controllers (S3.4) the guns follow for free if the attachment is engine-side. Verify each gun's draws match the engine component through the coordinate bridge | `WeaponAttachment`, `mAttachBone` | Each gun's origin in the overlay is within 1 cm of its hand's `WEAPON1`; moving one controller moves only that gun |
| **W2 Per-hand fire seam** | Find the pre-spawn fire function; in dual wield it must take or derive a hand or weapon index. Wrap it and replace the muzzle origin and direction with that hand's ray. The **model ray** latches the barrel axis per weapon model from `WEAPON1` in the gun's local frame, measured once per model and stored by model name, never a literal | the fire seam (R7's log of `IV_FIRE` starts is the first lead) | Bullet decal vs the debug ray within 1 cm at 5 m for each hand independently, including while the other hand fires |
| **W3 One ray per hand** | A hand's ray is the single source for: the bullet trace, muzzle flash placement, tracer, reticle and hit marker. No second ray is ever derived from the head | | Firing both guns at two different targets hits both (the flat game cannot); the two per-hand reticles sit on the two decals |
| **W4 Button ownership** | The mod owns the mapping: the left controller's trigger fires the gun in the left hand and the right fires the right, whatever `SwapFireButtonsWhenDualWielding` says. Read the option and mirror the injected pad buttons so the engine's own mapping produces this; expose "swap" as a mod option in F10 | `SwapFireButtonsWhenDualWielding`, the pad bridge | With the game option toggled either way, the left trigger fires the left gun in 10 of 10 tries |
| **W5 Aim view offset** | `mDualWieldingAimViewOffset` shifts the view when both guns are up in flat. In VR the view must never move for aiming. Zero it engine-side with an A/B | `mDualWieldingAimViewOffset` | Camera position delta on dual-wield aim is zero with the lever on |
| **W6 Reload and equip under hand control** | The DualPistol set (per-hand Aim / Fire / Reload / Equip / LetgoDualWield) keeps playing on the hand skeleton as an upper-body blend (A4) while the hand root stays on the controller. Fire recoil is allowed (small, sells the shot); Aim is suppressed (it would pull the gun to the flat aim pose); a gesture reload can come later | A1 mask, A3 pose override | During a reload the gun root stays within 3 cm of the controller; the reload is visibly playing on the fingers and slide; the Aim start is logged as suppressed |
| **W7 Every weapon class** | DualUzi, Pistol and TwoHanded get the same lanes. TwoHanded gets a two-hand grip (the off hand snaps to the foregrip when within 15 cm, else one-hand) and its own model ray | | Each class passes W2's decal test; the two-hand grip engages and disengages at the threshold |
| **W8 Weapon mirror** | For weapon models built for one side only, fill the unmodelled side by a mirrored draw (the Dishonored approach) | | A visual check in the headset that the off-hand pistol is not inside-out or mirrored-text |

## Rules this work runs under

- **Quaternions in the controller's local frame** for every offset. Euler adds after
  conversion are banned: correct at exactly one orientation, and BioShock measured 28 degrees
  of divergence from that mistake.
- **Grip pose for the model, aim pose for the bullet**, then everything moved to the aim pose
  plus a per-weapon trim on Dishonored. Start with the aim pose plus trim here.
- **Engine-side writes let attachments follow for free.** The muzzle flash, the shell eject
  and the tracer are attachments; if the gun is placed by writing the bone the engine reads,
  they come along. If it is placed by patching the draw's matrix, they do not.
- **A verified write is not an honoured one.** The fire seam's test is the decal, never the
  logged origin.
- **A constant is not a sample.** The model ray is measured per weapon model at runtime and
  latched by name; it is never typed into a header.
