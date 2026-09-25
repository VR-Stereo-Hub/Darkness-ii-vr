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

**Measured by the extractor (R5, 2026-09-25, `tools/cache/extract.py skel`)**:

| Rig | Bone table (skin) | Hierarchy table | `GAME_C1_CAMERA` | Tentacle roots |
|---|---|---|---|---|
| `FPDJackie_skel.fbx` (H 10,929 bytes) | 150 names | 164 records, `GAME_C1_ROOT` first with 163 descendants | **absent** | absent (the demon arm chains hang under the body: `GAME_x1_DEMONARM1..32`, `UPJAW1`, `LOWJAW1`, `TONGUE1..5` per side) |
| `FPDCameraDArms_skel.fbx` (H 6,568 bytes) | 126 names | 127 records | present: h[1], **parent 0 = `GAME_C1_ROOT`, 0 descendants** | `GAME_L1_TENTACLE_CLAV` h[46] and `GAME_R1_TENTACLE_CLAV` h[86], **both parent 0 = `GAME_C1_ROOT`**, 39 descendants each |

So in the shipped skeleton data the camera bone and the two tentacle roots are **siblings under
the root**, not parent and children; if the demon arms follow the camera in play, the engine
does it at runtime (the FP entity following the camera transform), not through the bone
hierarchy. That is half of R6's question answered from data; the runtime half (who writes the
FP entity's transform) is still R6.

The LOD words: after `/GrabbyHead/DemonArmGrabbyRevised` the header holds five ascending u32
(11766, 62178, 99894, 127737, 147600) followed by five descending u32 (15438, 11475, 7719,
5163, 3003): **five LOD levels**, each with what reads as a data offset and a count. Whether
the count is vertices or triangles is still opaque (the B part has no names to anchor it).
The three floats 10, 20, 30 right after the bone table read as LOD distances.

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

`tools/cache/extract.py` (VR-236, 2026-09-25): read-only, Python, an LZF decoder and nothing
else. Commands: `list [--filter]` (every path with its caches and sizes), `extract <path>
[--out]` (the H, B and F parts as separate files, into `tools/cache-out/`, gitignored),
`lua-dump` (every `.lua` as readable source into `tools/lua/`, gitignored), `skel <path>`
(the bone tables of a `_skel.fbx` blob). Under Git Bash set `MSYS_NO_PATHCONV=1` or the
`/D2/...` argument is rewritten into a Windows path.

### R5 verdict (measured 2026-09-25)

- **`list` prints 47,425 unique paths** over 102,721 file entries in 24 caches, the survey's
  number. The `.toc` `parentDirIndex` is a **1-based index into the directory entries only**
  (offset -1), 0 meaning the root; all-entry numbering gives 83,651 nonsense paths.
- **`lua-dump` writes 430 scripts as readable source, 0 failures.** The B part of a `.lua` is
  a Lua 5.1 chunk (`1B 4C 75 61 51 00 01 04 04 04 04 00`) whose top-level function's SOURCE
  NAME field (`u32 len` then the bytes, NUL-terminated) holds the entire script text; the H
  part holds `[u32 1][u32 len][asset path][u32 len][the same text]`. `SetFov.lua` line 18 is
  `camCtrl:SetBaseFovOverride(finalFov)`. (The survey counted about 435 `.lua` entries; 430
  unique paths exist in the B caches.)
- **`skel` parses two tables** in a `_skel.fbx` H part: the skin bone table (consecutive
  `[u32 len][name]` records, then the count as a u32, then the floats 10, 20, 30) and the
  hierarchy table (`[u32 count]` then `[u32 len][name][u16 parentIndex][u16 descendantCount]`
  records). The FP rig facts are in section 5.
- **Still opaque**: everything in the B part (vertex and index streams, skin weights, the
  animation curves) and the meaning of the per-LOD count. The H part after the tables holds
  the `Materials={...}` text block, the part list (`JackieHands`, `JackieBody`, the demon arm
  heads, the `Blades.DemonArmSlashyBladedMorph` morph) and the LOD words; the field order
  between them is not decoded.

It unblocks: reading the shipped Lua (R2, R3, S1, S6), the FP rig and tentacle inspection (T0,
T1), the offline animation inventory (A0), and later the loose-file override (M4) and
re-import (M6), which need the blob layouts.
