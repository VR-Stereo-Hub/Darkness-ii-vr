# Research

Sourced facts about the target game, the engine, prior art and the VR runtime situation.
Anything measured from the install is marked so and detailed in `docs/darkness2/`; anything
from the web carries its URL. Engine strings quoted here are identifiers, not content.

## The target

The Darkness II (2012), Digital Extremes for 2K Games, Steam app **67370**, buildid 11598 on
the dev PC. 32-bit `DarknessII.exe`, 14.3 MB, fixed base, no ASLR, CEG-linked, not packed.
Direct3D 9 only. Measured 2026-09-25; `docs/darkness2/ENGINE_NOTES.md` section 1.

## The engine

- **Evolution Engine**, Digital Extremes' in-house engine, debuted with Dark Sector (2008),
  used for The Darkness II, and later the base of Warframe.
  [Wikipedia: Digital Extremes](https://en.wikipedia.org/wiki/Digital_Extremes) ·
  [Engadget, 2011: The Darkness 2 uses Digital Extremes' Evolution Engine](https://www.engadget.com/2011-02-09-the-darkness-2-uses-digital-extremes-evolution-engine.html) ·
  [Giant Bomb: Evolution Engine](https://www.giantbomb.com/evolution-engine/3015-6676/)
- The exe carries "The Evolution Engine", the editor "Darkitect" and the `/EE/` and `/D2/`
  type namespaces (measured).
- **Lua 5.1.3** scripting with SWIG bindings; the shipped scripts are bytecode with source
  embedded (measured). A community decompilation of the scripts exists:
  [FromDarkHell/TheDarknessIIDecompiled](https://github.com/FromDarkHell/TheDarknessIIDecompiled)
  and an info dump at [fdh.one/Darkness](https://fdh.one/Darkness/). That repo is a reference
  for what the scripts do; nothing from it is committed here (game-derived).
- **gameswf** for UI, XAudio2 for audio, PhysX 2.8.4, Bink 1.99m (measured).
- **Native NVIDIA 3D Vision support** in the exe (`EnableNVidia3DVision`, `_NVIDIA_3D_VISION`
  shader define). Helix Mod published a community 3D Vision fix in 2012, which suggests the
  stock path needed shader-level corrections:
  [Helix Mod: The Darkness II 3D Vision fix update](https://helixmod.blogspot.com/2012/04/darkness-ii-3d-vision-fix-update.html).
  R4 decides whether the engine-side half is a usable per-eye source.

## The asset container

`.toc` / `.cache` pairs, TOC magic `0x1867C64E` v16, 96-byte entries, LZF blocks (measured
and parsed, `docs/darkness2/GAME_ASSETS.md`). Prior art for the same family:

- [jleclanche/evoeng](https://github.com/jleclanche/evoeng): Python tools for Evolution
  Engine `.cache`/`.toc` pairs.
- [Puxtril/LotusLib](https://github.com/Puxtril/LotusLib): C++17 library for the Warframe-era
  format (adds Oodle, which this build does not use).
- QuickBMS scripts on ZenHAX for [Dark Sector](https://www.zenhax.com/viewtopic.php@t=4765.html)
  and [Darkness II](https://www.zenhax.com/viewtopic.php@t=4793.html).
- A Facepunch thread on extracting the models:
  [The Darkness 1 and or 2 models?](https://forum.facepunch.com/f/fbx/qfwa/The-Darkness-1-and-or-2-models/1).

The mod's own extractor (R5) is written from the measured format, not from these, and reads
only.

## Prior VR and display work on this game

- **VorpX** runs the game (a community video: "THE DARKNESS 2 in VR with VorpX", Rift S,
  [Steam Community](https://steamcommunity.com/sharedfiles/filedetails/?id=2392399801)). That
  is a geometry-injection approach with no engine integration and no motion controls; it
  establishes only that the frame can be captured and the head can drive the mouse.
- **FOV**: the launch build had a broken FOV
  ([WSGF: The Darkness II demo FOV is broken (same as Dark Sector)](https://www.wsgf.org/phpBB3/viewtopic.php?t=23523));
  a Steam patch added an FOV slider with a maximum of about 85 degrees
  ([N4G: The Darkness 2 PC FOV Patch Released](https://n4g.com/news/970507/the-darkness-2-pc-fov-patch-released),
  [Steam discussion: FOV](https://steamcommunity.com/app/67370/discussions/0/846960628370899519/));
  **Widescreen Fixer** had a profile with aspect-ratio and FOV fixes
  ([The Koalition: How to fix The Darkness II FOV](https://thekoalition.com/2012/fix-darkness-ii-fov)).
  The exe exposes `CmdSetBaseFOV` and `SetBaseFovOverride` (measured); the mod's FOV lever
  goes through those, not through a patch.
- **Frame rate**: community reports of a cap around 60 to 65 fps in places and heavy drops
  in others ([Steam: The game is locked at 65 fps](https://steamcommunity.com/app/67370/discussions/0/1606022547919231366/),
  [Steam: FPS cap?](https://steamcommunity.com/app/67370/discussions/0/1697168437879982611/),
  [Steam: Strange FPS drops](https://steamcommunity.com/app/67370/discussions/0/594820656479127955/)).
  A community guide suggests a CPU affinity workaround on modern PCs
  ([Steam guide: How to FIX performance/crash problems in modern PC](https://steamcommunity.com/sharedfiles/filedetails/?id=3035615127)).
  Whether a cap exists and where it lives is an S2 measurement (the cadence beat depends on
  it); `VSyncMode` and `VSyncSlack` are the first strings to follow.
- **PCGamingWiki** has a page ([The Darkness II](https://www.pcgamingwiki.com/wiki/The_Darkness_II));
  it could not be fetched by the tool during this session and should be read by hand for the
  save and config table.
- **Quad wielding** as designed: right and left triggers fire the guns, the bumpers drive the
  demon arms; the left arm grabs (four glowing grab points on an enemy: head, chest, either
  leg), the right arm slashes; the arm animations were designed not to obstruct the view
  ([Kotaku: The Murderous Math of The Darkness II's Quad Wielding](https://kotaku.com/the-murderous-math-of-the-darkness-iis-quad-wielding-5796971),
  [GameSpot Q&A on quad-wielding](https://www.gamespot.com/articles/the-darkness-ii-qanda-on-graphic-noir-and-quad-wielding/1100-6348671/),
  [Destructoid hands-on](https://www.destructoid.com/hands-on-quad-wielding-with-the-darkness-ii/)).

## VR runtime facts (from the siblings, still true)

- 32-bit OpenXR runtimes: Virtual Desktop's VDXR (the most tested path on both siblings); any
  native runtime with a 32-bit build. **SteamVR has no 32-bit OpenXR runtime**, hence the
  OpenXR-on-OpenVR shim the siblings ship.
- A 64-bit implicit OpenXR API layer (OBS is the known one) breaks 32-bit `xrCreateInstance`
  with error -32; the runtime layer detects it and disables the layer in-process.
- `xrWaitFrame` has no timeout; it runs on a pace thread with a deadline. An unfocused session
  must keep submitting or the runtime never re-grants focus.

## Legal posture

- The mod distributes no game content: no extracted assets, no decompiled scripts, no shader
  dumps. Tools that read the game's files are committed; their output is gitignored.
- No code from UEVR (all rights reserved; concepts only). REFramework (MIT) may be adapted
  with attribution. The runtime layer and simulator come from the sibling mods in this org.
- The exe is never modified on disk. Hooks are in-memory, in a process the player launched
  themselves, on a game they own.

## Open research questions (each is a ticket)

R0 the loading route; R1 CEG and in-memory hooks; R2 the Lua lane and the SWIG API; R3 the
console and command dispatcher; R4 the projection builder and 3D Vision; R5 the cache
extractor and blob layouts; R6 FP rig ownership; R7 animation entry. `docs/ROADMAP.md`.
