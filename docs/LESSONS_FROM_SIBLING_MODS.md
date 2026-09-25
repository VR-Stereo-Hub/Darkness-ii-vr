# Lessons from the sibling mods

This mod is the third in a line. The BioShock trilogy mod came first (three engines, one
repo, about 74 sessions, shipped 0.8.x as early access for two of them). The Dishonored mod
came second (one engine, adopted BioShock's runtime layer and simulator, shipped 1.0.0 and
1.0.1 in 24 sessions) and is the most polished thing the org has made. This file is what a
session here should know about both before writing a plan: the order their features landed,
what made one polished and the other rough, and the rules that transfer.

The engines were Unreal (Vengeance 2.5, UE3 twice) and this one is Digital Extremes'
Evolution Engine. **Nothing numeric transfers.** What transfers is the method, the order and
the process. Traps are catalogued separately in `docs/TRAPS.md`.

## 1. The order Dishonored delivered its features

The dates are 2026; the PR numbers are the Dishonored repo's. Read the shape, not the speed:
a 24-day run on an engine with a decompiled script layer and an original author's handoff.

| Phase | Dates | What landed |
|---|---|---|
| 0 Inheritance | 08-30 to 09-01 | The original author's alpha 38.92 imported (a DXVK fork doing side-by-side stereo, two runtime backends, three head-motion paths). Their handoff at 39.4 joined the repo with its dead ends and traps |
| 1 Framework | 09-02 | CMake/MSVC, the unity split, logging and crash handling from `DllMain`, the command seam and `status.json`, the BioShock simulator ported. **Nothing about VR yet** |
| 1 S0 foundation | 09-02 to 09-03 (PR #1) | The DXVK fork removed one commit at a time. The BioShock OpenXR runtime layer adopted verbatim behind two D3D9 seams. The stereo seam with rung 1, the **mono screen** (the game frame on a head-locked quad in both eyes). The per-eye camera seam with `camera eyetest`, an instrument that writes each candidate camera field and reports which one the renderer honoured. Headset-verified the next day |
| 1 S2b reentry | 09-03 (PRs #3, #8, #10) | The scene-draw root found by caller census and a live "mover", its ONE call site patched to draw twice per tick with the other eye's camera: **true stereo**, fusion confirmed in a headset at run 40. Positional tracking on the camera seam. Neck pivot cancel with measured constants. Render size asked through the engine's own command line. The D3D9Ex device and shared-surface capture for performance |
| 2 Correctness | 09-04 to 09-05 | The swapped eyes (the tag ring realigned by the per-present camera step). Crouch height. **Ghosting solved as a cadence beat**: 2750x2850 at 90 Hz puts the tick at one display period. The Linear and GitHub flow adopted (PR #16). The black-texture bug |
| 3 Motion controls | 09-05 to 09-08 (PRs #18-25) | **VR-30 head yaw no longer turns the body** (the yaw INPUT to the body-rotation function changed, not the result). VR-31 the hands cut from the arms by bone influence: floating hands. VR-33 hands placed at the controllers through the bone palette, then held weapons on the tracked controllers matched to engine components through a coordinate bridge. The property resolver (fields found by NAME). Fired-bolt identity, the pistol |
| 4 Stability | 09-09 to 09-11 (PRs #34-36) | Head-turn judder (the submitted pose was one generation too new). Weapon judder (the hand normalised against a head the view was not rendered from). The tested ini shipped as the defaults. The intro boat fall (a collector clearing collision). The mirror pinned to the drawn eye |
| 5 Aiming | 09-12 to 09-13 | **VR-57 the crossbow aimed from the controller, then from its own barrel** (the model ray, latched per weapon). The pistol's fire seam derived offline by re-walking the crossbow's route first. **VR-36 Blink redirected at its INPUT aim vector**. Hand size adjust. Crouched neck pivot. Animation hand-back during scripted moves. Roll kept out of the neck arc |
| 6 Cinematics and comfort | 09-13 to 09-14 (PRs #53-61) | Load transitions. Cinematic head tracking with upright pitch and roll. Dialogue stereo. Mono UI anchored where the player looks. **World smoothness at 120 Hz** by submitting the image's own head orientation |
| 7 HUD | 09-15 to 09-17 (PRs #63-73) | The Scaleform HUD on anchors, then per element (the 2D vertex transform found, an element table, six anchors, captured alpha). The weapon dial in world space. Reading panels on the hand. Controller modifiers |
| 8 Features and polish | 09-19 to 09-22 (PRs #82-103) | Animation controls, rain and lens effects at depth, weapon mirror, reticle customization, **the motion sword** (edge/sustain detectors, honoured check), sneak kill by gesture, **camera shake removed at its source**, the F10 panel driven from the controllers, throwing, sleeve presets, F10 tiers and a painted theme, the chain-camera fix |
| 9 Shipping | 09-23 to 09-24 (PRs #105-112) | Installer, takedown assist, walk speed, launcher, **1.0.0**, then a 1.0.1 hotfix (FOV feedback, launcher self-update, support logs, mirror default) |

The ladder as designed was S0 foundation, S1 mono screen accepted, S2 two stereo methods
built by two developers in parallel, S3 compare and bring the features back on the winner.
The practical order that came out: **runtime and mono screen, true stereo, correctness and
judder, head decoupled from body, floating hands, hands and weapons on the controllers,
aiming, cinematics and comfort, HUD, gestures, comfort polish, F10 UX, installer and release.**
Every one of those steps was gated on a measurement, and the milestone description said
"the order is not negotiable: decoupling comes first because everything else sits on it, and
scaling comes before weapon tracking because scaling moves the pivot the tracking is fitted
to".

## 2. The order BioShock delivered its features

| Phase | Dates | What landed |
|---|---|---|
| M0-M1 | 07-23 | Repo, CMake Win32, an `xinput1_3.dll` proxy, deferred init, logger, minidump, a D3D11 Present hook, ImGui on F10. De-risk: a 32-bit OpenXR hello-world on VDXR, the view hook found by name-chain scan |
| M2-M3 | 07-23 to 07-24 | OpenXR session on the game's device, the game frame on a head-tracked quad; 6DOF head camera, FOV claim, world scale, recenter |
| M4 stereo | 07-24 | The frame map built with an in-tree inspector; AlternateEye proved the geometry; SequentialReentry (the scene-build function called twice); structural single-threading; pair pacing |
| M5-M7 | 07-25 to 07-27 | Synthetic XInput from OpenXR actions; the console-exec seam; decoupled aim at the fire seam; actor pinning (dead end) then **bone-level viewmodel drive**; the foreground lens match; the body follows the head; **v0.1.0** |
| M8-M9 | 07-28 to 07-31 | HUD to a floating quad; one trim algebra; record and replay; per-weapon profiles; cinematics; snap and smooth turn; crash diagnostics; **the wrench swing gesture**; v0.5 and v0.6 |
| BioShock 2 | 07-29 to 08-05 | The adapter, FOV, stereo on the threaded substrate, a hard freeze fixed, resolution, motion controls, the animation-preserving drive, HUD; **v0.7.0** |
| The simulator | 08-01 | **xrsim, a simulated 32-bit Quest 3 runtime.** It became the first merge gate three weeks later and Dishonored started with it on day one |
| Infinite | 07-31 to 08-13 | I0-I9 in BioShock 2's order; **v0.8.0**, all three games in one zip |
| Comfort and consolidation | 08-13 to 09-11 | Hand and weapon scaling, the SteamVR shim, head-bob removal, anchored screens, the F10 panel from the controllers, scripted events, the viewmodel desync fixed by the drive architecture, the off hand, arm IK (unmerged), v0.8.3 |

## 3. What made Dishonored the polished one

1. **The simulator from day one.** Every non-perceptual question was answered without a
   headset. Headset runs were spent on judgement, not on finding out whether a build works.
2. **One behavioural change per build, and acceptance as a measured effect.** No "it
   compiles"; no "the write was verified". A ticket closed on a number, a log line or a
   written headset verdict.
3. **The ticket discipline.** Every change from a ticket with pass criteria; the PR body's
   first line the link; "what is deliberately not here" handing over every fault found and
   not fixed; measurements on the ticket because the ticket outlives the branch.
4. **Engine-side writes.** Hands placed by correcting what the engine reads, so weapons and
   effects followed. Blink redirected at its input. The camera changed at its request. Every
   render-side patch was the interim step, replaced when the engine-side seam was found.
5. **One ray.** The crosshair, the laser, the model and the shot from the identical ray,
   measured from the weapon's own barrel.
6. **The decision log and the graveyard.** Every non-obvious choice dated; every dead end
   recorded so it was eliminated once. Sessions were told to grep the graveyard before
   proposing.
7. **Headset-judged defaults shipped byte for byte.** The packaged ini was a copy of the
   tested machine's; per-key migrations with markers instead of a version bump that wipes
   tuning.
8. **Comfort at the source.** Camera shake, head bob and animation-driven sway removed where
   the engine produces them, not filtered afterwards; the view moves only when the player
   does.
9. **The F10 panel as the instrument.** Every lever an A/B a tester can flip in-session, tiered
   so the fixes nobody should turn off stay out of the way, driven from the controllers.
10. **An original author's handoff** with dead ends and traps, read before writing code.

## 4. Why BioShock stayed rougher

The BioShock repo's own docs say it, in different places:

- **Three engines, one repo, nothing carrying over.** Vengeance 2.5 twice, UE3 once, with
  compensation machinery ("forced by BS1-specific limitations, not because it is the right
  design") that was deliberately duplicated across adapters and never consolidated. The
  "healing session" was deferred and never happened.
- **The simulator arrived at session 34.** Thirty-three sessions of headset-only verification
  came first, and the "Rules carried over" block at the top of the Infinite notes is, in its
  own words, the distilled cost of those sessions.
- **No full playthrough.** Every fix accepted on a single check; two of three regressions in
  one night found by the user playing.
- **Branch protection, CI and delete-on-merge existed only on paper.** The most common
  process failure was commits directly on the integration branch.
- **The F10 panel and the ini were never unified**, so a saved preset silently overrode the
  shipped defaults and produced a recurring support case.
- **Infinite shipped as early access** with hand jank, aim not tuned and cinematic beats
  unfinished, because the third engine got the least time.
- **Fixes from reasoning alone.** The docs record four wrong hypotheses on one blur, three
  whole-arm corrections failing the same way, and a standing rule that arrived late: measure
  before the third attempt.

None of that is a criticism of the work; the rules that made Dishonored fast were paid for
there. It is the reason this repo starts with the docs, the flow, the simulator plan and the
control layers before any feature.

## 5. The ten rules that transfer

1. **Acceptance is a measured effect, not landed code.** A verified write is not an honoured
   one.
2. **Simulator first, headset last.** Build the simulated runtime before the first feature
   that needs judging.
3. **Never copy a number between games**, and never copy a shape either.
4. **An instrument that cannot fail its own hypothesis is not evidence.** Ship a positive
   control with every negative result.
5. **Identify a render pass by making it move**; identify a flag by watching it change.
6. **Engine-side writes let attachments follow for free; render-side patches do not.**
7. **One ray**, and with two guns, one ray per hand. Quaternions in the controller's local
   frame; euler adds banned.
8. **Every render lever ships default OFF with a live A/B**, reachable from the F10 panel.
9. **Fail soft.** A failed hook logs why, with the values, and the game runs flat.
10. **Write the graveyard.** A falsified hypothesis is a result; a session that ends without
    pushing STATUS is a failed handoff.

## 6. Reading the sibling repos without drowning

From BioShock's `docs/NAVIGATION.md`: **grep an anchor, read a window. Never open a doc whole
above about 400 lines.** Both repos carry multi-hundred-kilobyte notes (Dishonored's
ENGINE_NOTES is 608 KB, its FLICKER_REFERENCE 258 KB, STATUS 558 KB; BioShock's ARCHITECTURE
decision log alone is about 2,600 lines). Use the headings tables in `docs/TRAPS.md` as the
index and read only the entry.

| Question | Where in the siblings |
|---|---|
| How the runtime layer is structured, and the two host seams | Dishonored `docs/ARCHITECTURE.md` "The runtime layer"; `src/core/vr/openxr_runtime.cpp` |
| How the simulator presents as a Quest 3 and what `.xrs` can assert | Dishonored `docs/VERIFICATION.md` section 2; `tools/xrsim/*.xrs`; `src/tools/xrsim/` |
| How the scene-draw root was found and the call site patched | Dishonored `docs/dishonored/ENGINE_NOTES.md` "The scene-draw root, derived live"; `docs/ROADMAP.md` S2b |
| How hands were cut from arms and placed at the controllers | Dishonored `docs/dishonored/ARM_HAND_SPLIT.md`, `VR-33-HANDS-AND-WEAPONS.md` |
| How the viewmodel was driven at the bone level with animation preserved | BioShock `docs/ARCHITECTURE.md` decision log sessions 41, 67, 68 |
| How the fire seam and the model ray were derived | Dishonored `docs/dishonored/VR-57-AIM-PIPELINE.md`, `VR-57-MODEL-RAY.md`, `VR-82-PISTOL-FIRE-SEAM.md` |
| How a power was redirected at its input | Dishonored `docs/dishonored/VR-36-BLINK-RAY.md` |
| How the HUD was recognised, captured and anchored per element | Dishonored `docs/dishonored/HUD_ANCHORS.md`, `HUD_ELEMENTS_HOWTO.md`, `HANDOFF-HUD-ELEMENTS.md` |
| How the motion sword decides an attack | Dishonored `docs/dishonored/PHYSICAL_SWING.md`; BioShock `core/input/swing` |
| How the F10 panel is driven from the controllers | Dishonored `docs/dishonored/F10_MOTION_CONTROLS.md`, `F10_AUDIT.md`, `F10_ART_THEME.md` |
| How camera shake and bob were removed at the source | Dishonored VR-172 in `docs/ROADMAP.md` and `docs/TRAPS.md` |
| How animation hand-back works during full-body moves | Dishonored `docs/dishonored/ANIM-HANDOFF-PLAN.md`; BioShock decision log s64 "the rig during a scripted scene" |
| How the installer and launcher were built and tested | Dishonored `docs/INSTALLER.md` |
| The whole dev flow | Dishonored `docs/LINEAR_AND_GITHUB.md` on branch `claude/vr-218-staging-flow` |
