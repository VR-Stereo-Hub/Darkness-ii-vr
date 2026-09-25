# The game's assets: the caches and what is in them

Measured read-only on 2026-09-25 from `D:\SteamLibrary\steamapps\common\Darkness II`. The
tool that reads these (R5, `tools/cache/extract.py`) is ours and is committed; **its output is
game-derived and never is.** Asset PATHS and NAMES are identifiers and may be quoted here; the
assets themselves may not be committed.

## 1. Install layout

```
Darkness II\                       6.9 GB, 317 files
  DarknessII.exe                   14,291,512
  steam_appid.txt                  "67370"
  installscript.vdf                runs VC++ 2008 and DirectX Jun-2010 redists
  Cache.Windows\                   6.7 GB, 24 .toc + 24 .cache pairs: all game assets
  Cache.DLC\PCPatchManifests\      65 MB: Header.dat ("CacheName=PCPatchManifests, PackageId=0,
                                   DlcVersion=1") + Cache.Windows\ overlay in the same format
  Drivers\                         DirectX9 and VCRedist2008 redistributables only
  Tools\                           BinkW32.dll, steam_api.dll, Physx\x86\*
```

## 2. The container format (confirmed by parsing, not guessed)

**`.toc`**: an 8-byte header (magic `4E C6 67 18` = `0x1867C64E`, version **16**) followed by
**96-byte entries**:

| Offset | Type | Field |
|---|---|---|
| 0 | int64 | offset into the `.cache` (**-1 = directory**) |
| 8 | int64 | FILETIME |
| 16 | int32 | compressedLen |
| 20 | int32 | len (decompressed) |
| 24 | int32 | reserved |
| 28 | int32 | parentDirIndex |
| 32 | char[64] | name |

Every `.toc` size divides exactly (e.g. `H.Misc.toc` = 8 + 28,693 x 96).

**`.cache`**: entries stored back to back. A compressed entry is a series of blocks, each
`[u16 BE compLen][u16 BE rawLen][payload]`; the payload is **LZF** when the two lengths differ
and raw when they are equal. A 15-line LZF decoder decompressed sample entries correctly.

**Prefixes**: **H** = header/metadata (materials, bone names, properties), **B** = binary body
(mesh and animation data, lower texture mips), **F** = full-resolution data (top mips, large
audio). The same asset path appears in the H, B and F caches; a whole asset is the union.

Prior art for the format: jleclanche/evoeng (Python), Puxtril/LotusLib (C++, Warframe-era,
adds Oodle which this build does not use), QuickBMS scripts on ZenHAX for Dark Sector and
Darkness II. Filenames inside conflict with directory names occasionally; those tools append
`~`.

## 3. The caches

| Cache | `.cache` size | TOC files | Content |
|---|---|---|---|
| F.Misc | 2,581,251,187 | 589 | `.wav`, stored uncompressed |
| F.Texture | 2,289,478,197 | 6,984 | top mips of `.png`-named textures (about 4.5 GB decompressed) |
| B.Misc | 1,029,879,078 | 18,147 | 12,038 `.fbx` (meshes and anims), 3,694 wav, 1,572 obj, 82 bsp |
| B.Texture | 246,668,228 | 9,568 | lower mips |
| F.VideoTextureSlow / F.VideoTexture / F.VideoTexture_en / B.VideoTexture | 218 / 186 / 169 / 20 MB | 57 / 19 / 2 / 5 | Bink `.bik` (`BIKi` headers) |
| F.Misc_en | 216,493,056 | 8,524 | English VO |
| B.AnimRetarget / H.AnimRetarget | 61 / 7.5 MB | 861 / 981 | 695 `.level`, 61 `.swf`, 57 `.animtree`, 43 `.animfsm` |
| H.Misc | 30,612,157 | 26,480 | metadata for everything; `/Languages.cl` |
| B.Misc_en | 26,942,732 | 8,768 | wav |
| B.Font / H.Font | 15 / 0.3 MB | 1,112 / 1,111 | despite the name: 1,083 compiled `.hlsl` shader permutations, plus fonts |
| B.Script / H.Script | 1.76 / 0.84 MB | 481 / 555 | **435 `.lua`** (bytecode with embedded source), plus `.hlsl` |
| smaller | | | H.Texture, `*_en` textures |

Totals: 102,721 entries, **47,425 unique asset paths**, all readable.

## 4. Naming conventions

Meshes and animations are **compiled blobs that keep their source filenames**: `.fbx` / `.x`
/ `.lwo` / `.obj` are not real FBX/X/LWO files. Suffixes: `_anim.fbx` (4,025), `_skel.fbx`
(296; a skinned mesh with its skeleton), `_cin.fbx` (241; cinematic), `_a.fbx` (919),
`_c.fbx` (521), `_physics.fbx` (5). Textures: `_d` / `_n` / `_s` / `_e` = diffuse / normal /
specular / emissive; `MaxResolution=MR_2048` style flags in the H metadata.

Namespaces: `/EE/` engine, `/D2/` this game, `/DS/` Dark Sector leftovers, `/DON/`.

## 5. The first-person rig

`/D2/Characters/Camera/`:

| Asset | B / H decompressed | What |
|---|---|---|
| **`FPDJackie_skel.fbx`** | 961,112 / 10,929 | The full first-person Jackie with both demon arms. Parts: `JackieHands`, `JackieBody`, `DemonArmGrabbyRevised`, `DemonArmSlashy`, morph target **`Blades.DemonArmSlashyBladedMorph`**. Materials: JackieClothes, JackieFlesh, DemonArmGrabbyRevised, DemonArmGrabbyEyeRevised, DemonArmSlashyRevised, DemonArmSlashyEyeRevised, DemonArmSlashyBlades, DemonArmTilingBody |
| **`FPDCameraDArms_skel.fbx`** | 352,566 / - | Camera + arms + demon arms rig, material `JackieFPArmsDarkness` |
| `FPCameraDArms_skel`, `FPDCamera_skel`, `FPJackie_skel`, `FPJackieArms_skel` | | Variants without the darkness, and Asylum / Wounded / Crucified story variants |

**Skeleton (from `FPDCameraDArms_skel`)**:

- Root `GAME_C1_ROOT`, camera bone **`GAME_C1_CAMERA`**.
- Human arm, per side: `CLAV1`, `ARM1`, `ARM2`, `ARM3`, `TWIST1`, `TWIST2`, `FINGER1..15`,
  **`WEAPON1`** (the weapon attach bone).
- Demon arm, per side (L1 / R1): `GAME_x1_TENTACLE_CLAV`, **`GAME_x1_DEMONARM1..32`** (32
  spine segments), `UPJAW1`, `LOWJAW1`, `TONGUE1..5`.
- In the game the **left** demon arm is the **Grabby** head and the **right** is the **Slashy**
  head (bladed; the morph target is the talent-unlocked blades).

Unconfirmed: after `/GrabbyHead/DemonArmGrabbyRevised` the FPDJackie header holds 15438 /
11475 / 7719 / 5163 / 3003, which look like **five LOD levels with about 15k vertices or
triangles at LOD0**. R5 confirms.

### Secondary tentacles and the cinematic arm

- `/D2/Characters/DemonArm/SecondaryTentacles_skel.fbx` (B 79,518, AutoLOD disabled): **4
  tentacles** (L1, L2, R1, R2), each `TENTACLE1..18` + `TENTACLETIP1..9` (+ `TENTACLEEND` in
  the animation). Animations `SecondaryTentaclesExtend / Idle / Retract_anim.fbx`.
- `CinematicDemonArm_skel.fbx`: 22 segments plus jaws, `CinematicDemonArmMesh`.

### Demon arm textures and materials

`/D2/Characters/DemonArm/`: `DemonArm_d/n/s`, `DemonArmGrabbyRevised_d/n/s/e`,
`DemonArmSlashyRevised_d/n/s/e`, `DemonArmSlashyBlades_d/n/s/e`, `DemonArmTilingBody_d/n/s/e`,
eye emissives, `PowerOff_e`, `*Red` and `*NoEmissive` material variants.
`/D2/Characters/DemonArm/Secondary/SecondaryTentacleRevised_*`.
`/D2/Characters/Jackie/JackieFPArmsDarkness_*` (`MaxResolution=MR_2048`), `JackieFPArmsHand*`,
`JackieFPArmsSleeveRevised_*`.

Estimated resolutions from mip-chain byte sizes (B holds mips from 256 squared down, F the top
mips): `DemonArm_d` about **1024x1024 DXT1** (F = 655,360 = 1024^2 + 512^2 DXT1);
Grabby/Slashy about 1024x512; tiling body 512 squared; `JackieFPArmsDarkness` **2048 squared**
(F = 2,752,512).

## 6. Animations

`/D2/Animations/FirstPerson/DemonArm/...`:

| Folder | Count | Names |
|---|---|---|
| base | 21 | Left/Right Idle01-09, Spawn, Hide, Pain |
| `FP/` | 15 | AgroLeft/RightIdle01-03, RightSliceAndDice, RunLeft/Right, SwarmStart/Loop/End L/R |
| **`Slash/`** | **8** | SlashUp, SlashDown, SlashLeft, SlashRight, SlashLeftUp, SlashLeftDown, SlashRightUp, SlashRightDown |
| `Grab/` | 41 | GrabReadyStart/Loop, Grab{Short,Medium,Long}Launch/Return, GrabPullStrong/Weak, GrabTugOfWar, GrabThrow, GrabJavelinIdle/Throw, GrabShield/ShieldThrow/ShieldAway, GrabHeartsLaunch/Return/Eat (+Fast/B), GrabNeckKill, GrabWaistKill, GrabMobster, GrabDoorLaunch/Return, GrabFail, GrabIdle/Low/Middle |
| `Finisher/` | 14 `_cin.fbx` | FinisherThroat, FinisherBackThroat, Finisher_Quartered, Finisher_SplitzFast, Finisher_LegSpinalTap, Finisher_TorsoFourSquare, FinisherHeadStomachWhip, ... |

Enemy side: GenericMale `HitReacts/DemonArm/` (GrabNeckLoop01, GrabWaistLoop02, GrabLegLoop01,
Flail01, KnockBack*). Vehicles: `{SUV,Sedan,Van,UHaul}/DemonArmPullDoor*`.

Weapons: `FirstPerson/DualPistol` (55: per-hand Aim/Fire/Reload/Equip/`LetgoDualWield`),
`DualUzi` (13), `Pistol` (78), `TwoHanded` (163).

Engine-side: 57 `.animtree` and 43 `.animfsm` assets in `B.AnimRetarget`; these are the state
machines and blend trees R7 reads.

## 7. FX, sound, script

- FX: `/D2/FX/DemonArm/` (DroolString, SpitFlip, GroundPound),
  `/D2/FX/Darkness/ProjectileDemonArmMat`, `/D2/FX/LensFlares/DemonArmLensFlareMaterial`.
- Sound: `/D2/Sounds/DarknessPowers/DemonArm/{Grab,Slash,Throw,Idles,EatHeart,Finishers,
  MobsterGrab}`, `/D2/Sounds/Impacts/DemonArm/<surface>`.
- Lua: 435 scripts under `/D2/Scripts/...` and `/EE/...`; e.g. `/D2/Scripts/Player/SetFov.lua`,
  `/D2/Scripts/Brothel/TentacleAnger.lua`, the menu scripts under `/D2/Menus/`.
- UI: 61 `.swf`.

## 8. The extractor (R5) and what it unblocks

Read-only, Python, no third-party dependency beyond an LZF decoder. Commands: `list` (every
path with cache, sizes, FILETIME), `extract <path> [--out]` (H+B+F joined, into
`tools/cache-out/`, gitignored), `lua-dump` (every `.lua` as readable source into
`tools/lua/`, gitignored), `skel <path>` (parse the bone table of a `_skel.fbx` blob by
searching for the known bone-name strings and report the layout as far as understood).

It unblocks: reading the shipped Lua (R2, R3, S1, S6), the FP rig and tentacle inspection (T0,
T1), the offline animation inventory (A0), and later the loose-file override (M4) and
re-import (M6), which need the blob layouts.
