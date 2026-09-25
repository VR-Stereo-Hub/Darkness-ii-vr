# The Darkness II VR

A VR mod for the Steam version of The Darkness II (2012). When it is done, the game will draw
in real stereo with full head and position tracking, each gun will sit in its own tracked
hand, and the Darkness demon arms will grow from your shoulders: grab with the left
controller, slash with a swing of the right.

**Status: pre-alpha. There is nothing to install yet.** This repository currently holds the
project's documentation, its engineering plan and its board. The first code tickets are a
research gate on the game's engine. Follow the [board](https://linear.app/vr-stereo-hub) or
`docs/STATUS.md` for where things are.

It is built the way the [Dishonored VR mod](https://github.com/VR-Stereo-Hub/Dishonored-VR)
was built, by the same org, and shares its OpenXR runtime layer, its simulator and its
process.

## What it will do

### Seeing the world

- Real stereo: the game draws its scene once for each eye, from that eye's position, and each
  image reaches the headset with the head pose it was drawn for.
- Full six-degree head tracking, with physical crouching and leaning.
- The game's camera shake, walk bob and weapon sway off by default. The view moves only when
  you do.
- Menus, loading screens and videos on a flat screen in front of you, opening where you look.
- The first-person cutscenes and executions keep your head free and the horizon level.

### Hands, guns and the Darkness

- Two guns, two controllers, each aimed down its own barrel. The shot, the tracer and the
  reticle come from one ray per hand. The left trigger always fires the left gun.
- The demon arms anchored to your shoulders instead of the camera, with their idles intact.
  The grab arm takes what your left controller points at; the slash arm swings the way you
  swing.
- Executions and heart eating play as the game authored them, with the camera following the
  scene but never taking your head.

### Comfort and control

- Snap turn, physical crouch, head- or hand-based movement.
- An in-headset panel (F10, driven from the controllers) with every setting, in Basic,
  Advanced and Debug tiers.

## What you will need

- The Darkness II on Steam (app 67370).
- A 64-bit Windows PC that runs the game comfortably on a monitor.
- One of: a Quest headset with [Virtual Desktop](https://www.vrdesktop.net/) (VDXR), a
  SteamVR headset (through the mod's bundled 32-bit shim), or any headset whose OpenXR
  runtime has a 32-bit build.

## Documentation

| File | What |
|---|---|
| [`docs/ROADMAP.md`](docs/ROADMAP.md) | The plan: the research gate, then the ladder from mono screen to release |
| [`docs/STATUS.md`](docs/STATUS.md) | Where things are right now |
| [`docs/darkness2/TENTACLES.md`](docs/darkness2/TENTACLES.md) | The demon arms in VR: what the assets are and how they will be driven |
| [`docs/darkness2/DUAL_WIELD.md`](docs/darkness2/DUAL_WIELD.md) | Two guns on two controllers |
| [`docs/LESSONS_FROM_SIBLING_MODS.md`](docs/LESSONS_FROM_SIBLING_MODS.md) | What the Dishonored and BioShock mods taught |
| [`CONTRIBUTING.md`](CONTRIBUTING.md) | How to report a problem, and how the team works |

## Credits

The runtime layer, the simulator and the process come from the Dishonored VR mod and the
BioShock trilogy VR mod, both in this org. Third-party components and their licences are in
`THIRD_PARTY_NOTICES.md`.

## Licence

To be decided before the first release. No game content is distributed by this repository.
