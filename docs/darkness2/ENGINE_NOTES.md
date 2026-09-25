# Engine notes - The Darkness II (DarknessII.exe)

Single source of truth for everything known about **DarknessII.exe** internals. Everything
here was measured read-only on 2026-09-25 from the Steam install on the dev PC unless a line
says otherwise. Nothing is copied from another game.

Rules:

- Every address, IAT slot and struct offset used by code lives ONLY in
  `src/game/darkness2/patterns.h` (planned), and every one is documented here with its
  derivation method, so it can be re-derived after a game patch.
- **NEVER copy a number, an offset or a shape from the Dishonored or BioShock notes.** Those
  are Unreal Engine games. This is Digital Extremes' Evolution Engine and nothing transfers
  except method.
- No game-derived content in the repo. No decompiled Lua, no extracted assets, no shader
  dumps. Findings are summarized here; dumps stay in the gitignored scratch folders.
- The exe is CEG-linked. **Never write the exe on disk.** Hooks are in-memory, installed after
  the game is up, and R1's verdict governs where they may sit.

## 1. PE identity (verified 2026-09-25, read-only)

| Field | Value | Note |
|---|---|---|
| Path | `D:\SteamLibrary\steamapps\common\Darkness II\DarknessII.exe` | Resolve from `libraryfolders.vdf`, never hardcode the drive |
| Size | 14,291,512 bytes | |
| Machine | x86 `0x014C` | 32-bit |
| Optional header magic | `0x010B` (PE32) | |
| e_lfanew | `0x128` | |
| ImageBase | **`0x00400000`** | |
| DllCharacteristics | `0x8000` | Terminal-server-aware only. **No ASLR, no NX.** Loads at a fixed base every run |
| Characteristics | `0x0122` | **LARGE_ADDRESS_AWARE set** (the 4 GB scan-range rule applies: any memory walk must exclude stacks and TEBs explicitly) |
| PE TimeDateStamp | 2012-03-20 15:55:33 UTC | A post-launch patch build |
| Linker | MSVC 9.0 (VS2008) | So the CRT is `msvcr90`-era; debug-CRT dialog traps from BioShock do not apply, but RTC does not either |
| Sections | `.text` (11.4 MB), `.rdata`, `.data`, `.tls`, `.version`, `.rsrc`, `.reloc` | **No SteamStub `.bind` section; not packed** |
| DRM | **Valve CEG** linked in | `VS_VERSION_INFO` names "Valve CEG Library / ceglib.lib 1,1,0,2100"; PDB path `c:\Darkness2PC\Code\EE.SteamCEG.pdb` |
| Version info | Company "Digital Extremes", Description "The Darkness II", FileVersion 0001.00.00.00.00 | |
| Steam appid | **67370** (`steam_appid.txt`) | Manifest `appmanifest_67370.acf`: buildid **11598**, depots 67371 / 67372 / 67380, SizeOnDisk 7,308,809,470 |

These fields are the **host build fingerprint**. A wrong build must refuse code hooks, keep
whatever is pattern-derived, and stay playable (fail soft).

### Static imports

KERNEL32, USER32, GDI32, ADVAPI32, SHELL32, ole32, OLEAUT32, PSAPI, VERSION, WS2_32, WINMM,
WTSAPI32.

### Delay-loaded imports

`NxCharacter.dll`, `steam_api.dll`, `XINPUT1_3.dll`, `AVIFIL32.dll`, `binkw32.dll`.

### Runtime-loaded (by name, from strings; not in either import table)

| DLL | Evidence | Note |
|---|---|---|
| `D3D9.DLL` / `D3D9.dll` | strings, plus `Direct3DCreate9` and **`Direct3DCreate9Ex`** | **The renderer is loaded at runtime by name.** R0 decides how the mod module gets in front of it |
| `DINPUT8.DLL` | string | |
| `xinput1_3.dll` | string "XInput not found" | Also delay-imported |
| `X3DAudio1_6.dll` | string | XAudio2 path |
| `nvapi.dll` + `nvapi_QueryInterface` | strings | 3D Vision and NVIDIA-specific paths |
| `Tools\DebugHelp\x86\dbghelp.dll` | string | Not shipped; the engine's own crash path expects it under `Tools\` |

### The game's own DLLs live under `Tools\`, not next to the exe

| File | Size | Version |
|---|---|---|
| `Tools\BinkW32.dll` | 226,304 | Bink 1.99m (uses `_BinkOpenXAudio2@4`) |
| `Tools\steam_api.dll` | 124,712 | Steamworks 01.10.01.46 (2011) |
| `Tools\Physx\x86\PhysXLoader.dll`, `PhysXCore.dll`, `PhysXCooking.dll`, `NxCharacter.dll` | 58 KB / 3.5 MB / 378 KB / 102 KB | **PhysX 2.8.4.6** (legacy `Nx*` API) |
| `Tools\Physx\x86\cudart32_30_9.dll` | 290,408 | CUDA 3.0.9 runtime for GPU PhysX |

The exe carries the relative paths as strings: `/Tools/steam_api.dll`, `/Tools/BinkW32.dll`,
`/Tools/PhysX/x86/`. **No DLL of any kind sits next to the exe in a clean install, and there
is no launcher or settings exe.** R0 must measure the exe's actual DLL search directory before
assuming an app-dir `d3d9.dll` proxy is picked up.

## 2. Graphics API: Direct3D 9 only

- 0 string hits for `d3d10`, `d3d11`, `dxgi`, `opengl`, `CreateDXGIFactory`,
  `D3D11CreateDevice`.
- `/EE/Types/Drivers/Dx9Driver`, `EvolutionEvolutionGfxD3D`, `INTZ` (the D3D9 depth-texture
  format hack) present.
- All shaders are precompiled **SM3.0** bytecode (`vs_3_0` / `ps_3_0`, "Microsoft (R) HLSL
  Shader Compiler 9.29.952.3111") with **CTAB constant tables**, so constant registers can be
  matched by NAME at runtime. Names seen: `WorldViewProjection`, `VertexColorGamma`,
  `ShaderRegister::SS_Projection`, `SS_WorldViewProjection`, `SS_InverseViewProjection`,
  `SS_PrevViewProjection`, `SS_CameraLineOfSight`, `SS_StereoDepth`,
  `ShaderSampler::SS_SpecialStereoMap`.
- The renderer is deferred (`/EE/Shaders/Deferred.hlsl`, `Deferred.inl`) with FXAA
  (`EnableFXAA`) and a cel-shaded look.
- **A native NVIDIA 3D Vision path exists**: `EnableNVidia3DVision`, `Stereo3DConvergence`,
  `Stereo3DSeperation` (sic), `StereoDepth`, shader define `_NVIDIA_3D_VISION`, `m.mSetupStereo`,
  `mToStereo`. R4 decides whether it is engine-side (draws twice, a free per-eye source) or
  driver-side (useless to us). Helix Mod published a 3D Vision fix for this game in 2012, which
  suggests the stock path was imperfect.

Consequence for the mod: the frame comes out of a D3D9 (or D3D9Ex) device and must be carried
into a D3D11 texture on the runtime's adapter for OpenXR, exactly the Dishonored arrangement.
Whether the game creates its device through `Direct3DCreate9` or `Direct3DCreate9Ex`, and
whether it presents with `Present` or `PresentEx`, is an S0.5 measurement.

## 3. Engine identity: Evolution Engine (Digital Extremes)

- Strings: "The Evolution Engine", "Build label: Evolution %s", "Evolution Editor",
  `Evolution-v1.1-LANResponse`, `Evolution%04i.avi`, `Tools/wscite/evolution.dbg`.
- **The level editor ships inside the exe.** It is called "Darkitect": `/EE/Types/Darkitect/*`,
  `/EE/Editor/*`, "Loading Level in Evolution Editor...", command-line `-editor`.
- **Namespaces** (type paths are strings in the exe and asset paths in the caches): `/EE/` is
  the engine (161 type/path strings), `/D2/` is the game (`/D2/Types/Jackie/`,
  `/D2/Types/Darklings/`), `/DS/` is Dark Sector leftovers in the caches (74 environments, 30
  sounds, 14 weapons), `/DON/` also present. **No `/Lotus/`** (Warframe's later namespace),
  but the cache format and type system are the ones Warframe inherited, so Warframe-era tools
  (evoeng, LotusLib) are the closest prior art for the container.
- **Command-line switches** found as strings: `-config`, `-log`, `-nolog`, `-console`,
  `-editor`, `-allowmultiple`, `-silent`, `-verbose`, `-test`, `-randseed`, `-save`, `-server`,
  `-client`. Whether `-console` opens an in-game console and what `-log` writes is R3.

### Scripting: Lua 5.1.3 with SWIG bindings

- `$Lua: Lua 5.1.3 Copyright (C) 1994-2008` is in the exe.
- SWIG-generated wrappers: `_p_WeakResT_...`, "Wrong arguments for overloaded function
  'new_WeakResource'", and a Lua debugger type `/EE/Types/Editor/LuaDebugger`.
- About 435 `.lua` scripts live in the caches (`B.Script` / `H.Script`), stored as Lua 5.1
  bytecode (`\x1bLuaQ`) **with the full source text embedded**. Example:
  `/D2/Scripts/Player/SetFov.lua` contains `finalFov = 45 --0 to reset` and
  `camCtrl:SetBaseFovOverride(finalFov)`.
- What this means: engine objects are reachable BY NAME from script, and the shipped scripts
  show the calls. R2 (reach the VM, dump the SWIG API) is the highest-leverage research item
  in the whole project. `luaL_loadbuffer`, `lua_pcall` and friends are findable by their
  standard 5.1.3 strings.

### UI: gameswf (not Scaleform)

- "Compile gameswf with TU_ENABLE_NETWORK=1...", `/EE/Types/UISys/FlashMgrImpl`,
  `FlashInstance`. 61 `.swf` movies in the caches. The HUD redirect (S7) will recognise
  gameswf draws by their own signature, not Scaleform's.

### Other libraries

zlib "inflate 1.2.5", IJG libjpeg, BMP/EXR/PNG bitmap plugins. Audio is the engine's own
XAudio2 driver (`/EE/Types/Drivers/XAudio2Driver`, X3DAudio1_6); **no FMOD, Wwise or Miles**.
**No Havok** (0 hits); physics is PhysX 2.8.4.

## 4. Settings and config

- Config paths as strings: `/Configs/EE.cfg/Windows_Config`,
  `/Configs/Editor.cfg/{MainGameWindowed, MainGameFullScreen, ...}`.
- Keys seen: `Graphics.FullScreen=1`, `Graphics.AutoDetectGraphicsSettings=1`,
  `App.DefaultCmdLine=`, `VSyncMode`, `VSyncSlack`, `FullScreenSizeX/Y`, the aspect list
  "AutoDetect, FullScreen4x3, WideScreen16x9, WideScreen16x10, Custom", `TextureQuality`,
  `ShadowQuality`, `EnableFXAA`, `Brightness`, `EnableNVidia3DVision`, `Stereo3DConvergence`,
  `Stereo3DSeperation`.
- **On disk the config is obfuscated.** `%APPDATA%\DarknessII\EE.cfg` (8,206 bytes),
  `Editor.cfg` (363), `<steamid>\settings` (1,852), `<steamid>\CONTINUE.SAV` (61,427). Each
  starts `00 00 xx xx` then scrambled bytes; simple repeating-key XOR did not decode them
  (a weak 36-byte periodicity, no clean plaintext). **The mod does not write these files.**
  Settings are requested through the engine's own apply path (S1) or the Lua lane (R2).
- **The options menu has no FOV setting.** From the menu Lua scripts:
  `OptionsDisplayCustomize.lua` (Resolution, DisplayMode, TextureQuality, ShadowQuality,
  VerticalSync Auto/On/Off, Decals, Brightness, Apply), `OptionsControls.lua`
  (AimSensitivity, AimAssist, Vibration, InvertY, SouthPawControls), `OptionsGame.lua` (Hints,
  SubTitles, HUD, EssenceMessages). A Steam patch added an FOV slider (community reports say
  about 85 degrees maximum). Internally: "User profile custom FOV will apply to this Avatar",
  `mAllowCustomFov`.
- No Steam Cloud folder for 67370 was found under `Steam\userdata`. Nothing under
  `%LOCALAPPDATA%`, `Documents` or `Saved Games`.

## 5. String inventory by subsystem (the map for R2-R7)

Everything below is a string in the exe. Each is a property, a method, a type or a tooltip
the engine's reflection knows, which is what makes it reachable by name once R2 lands.

### Camera and FOV

`CmdSetBaseFOV`, `SetBaseFovOverride` / `GetBaseFovOverride`, `mBaseFovOverride`, "Base FOV,
leave at 0.0 to use default gFOV", `SetFov`, `SetFovDefaults`, `mFovAngle`, `mVerticalFov`,
`mViewFov`, `mHorizontalFOVHack` / "Use FOV angle as horizontal (temp)", `mForceGameFOV` /
"Force game FOV on camera", `mCinematicFOV` / "Cinematic FOV", `mAimFOVMultiplier`,
`mZoomFOV`, `mSlomoFov`, `mMeleeFov`, `mTargetingFov`, `mMinPlayerFov`, `mMaxFOV`, `mMinFOV`,
`mRunFOVBias` / "Attenuate FOV during running", "Frustum horizontal FOV [degrees]",
`CameraNearPlane`, `mGameCameraNearPlaneOverride`, `CmdToggleFreeCamera`,
`CmdToggleCameraIndependant` / "Camera control is independent to movement",
"Camera pitch clamp", "Camera heading clamp [degrees; 0,0 disables]", "Camera bank clamp",
"Camera smooth time", "Camera collision radius", `CameraController`, `CameraControllerBase`,
`NullCameraController`, `CinematicCamera`, `CameraSpot`, `SetCameraSpot`, `ForceCameraSpot`,
"Camera is offset above player position by this much (m)", `mCamAllowFirstPersonToggle` /
"Allow toggling between first and third person?", `mCamStartInFirstPerson`.

**Units**: at least one tooltip says metres ("offset above player position by this much
(m)"). Confirm the world unit before writing any positional tracking (S1).

### First-person rig

`mFirstPersonViewOffset`, `?mFirstPersonOffset`, `EnableFirstPersonEyeOffset` /
`DisableFirstPersonEyeOffset`, `SetFPEntityClampedToCameraPitch`,
`mFPEntityIgnoresShakeTranslationAndRotation`, `mFirstPersonProxy`, `mFirstPersonAnim`,
`mFirstPersonAnimControllerType`, `mFirstPersonFinisher`, `mFirstPersonDeathAnimation`,
`mPlayFirstPersonAnims`, `mShowFirstPerson`, `mIsFirstPersonAvatar`,
`mSupportsFirstPersonView`, `mUseFirstPersonInTightSpace`, `mFirstPersonFootSteps`,
`fs.mUseProceduralFirstPersonFootsteps`, `mMaxFirstPersonFallScreenShake`,
`mReplicateVisibilityInFirstPerson`. R6 decides which of these places the rig.

### Shake, bob and sway (the comfort levers, S6)

Bob: `ViewBob`, `mWalkBobDepth/Rate/Width`, `mRunBobDepth/Rate/Width`, `mAimBobDepth/Rate/
Width`, `mFallBobDepth`, `mBobActivationSpeed`, `mBobInterval`, `mBobPulseBlendRate`.
Sway: `mSwayAmplitude`, `mSwaySpeed`, `mDotSwayIntensity`, `mNoDotSwayWhenAiming`.
Shake: `ViewShake`, `mCameraShake`, `mCameraShakeStrength/Radius/Intensity`,
`mShakeFactorPos`, `mShakeFactorRot`, `mShakeRotation`, `mShakeStrength/Radius/Speed/Time/
Type`, `mHitShakeIntensity`, `HitShakeProbability/Strength`, `mImpactShake`, `mRumbleShake`,
`mWalkShake`, `mRunShake`, `mJogShake`, `mVelocityShake`, `mZoomShake`, `mFallScreenShakeBase/
Scaling`, `mShakeCameraWhileStopped`, `mFPEntityIgnoresShakeTranslationAndRotation`,
"Camera recoil", `CameraNoiseAmount`, `CameraNoiseSpeed`.

### Animation and IK

AnimTree nodes: `AnimTreeNodes::BaseIKNode`, `FootIK`, `LookIK`, `ReachIK`, `Ragdoll`,
`ProceduralAnimTreeNode`, `ProceduralBlend`. IK: "Dual Wield IK params", "Single Wield IK
params", `IKT_LEFT_HAND, IKT_RIGHT_HAND, IKT_LEFT_FOOT, IKT_RIGHT_FOOT`, `EnableReachIK` /
`DisableReachIK`, `mReachIKSpeed`, `mForceNoIK`, `mMinDistanceToUseIK`, "Jackie must be at
least this far from the swing animation's path or IK will not be used", "How fast to blend
grip IK in/out", `mLookIKFov`, `mLookIKSpeed`, `mDebugDrawAnimationAndIK`,
`mDrawIKTargetPoints`, `ShowIKStats`. The animation input-variable enum `IV_*` (about 90
entries: `IV_AIMING`, `IV_CROUCH`, `IV_FIRE`, `IV_RELOAD`, `IV_MELEE`, `IV_FINISHER_INDEX`,
`IV_FINISHER_STATE`, `IV_CINEMATIC_ANIMATION`, `IV_WEAPON_HAND`, `IV_WEAPON_GRIP`,
`IV_WEAPON_TYPE`, `IV_LOOK_IK`, `IV_REACH_IK`, `IV_PLAY_ANIMATION`, `IV_OVERRIDE_ANIMATION`,
...) is the state machine's input vocabulary and the first thing R7 should log.
Weapon type enum: `KALASHNIKOV, KEL_TEC, M4A1_CARBINE, COLT_COMMANDER, COLT_PYTHON,
DESERT_EAGLE, MODEL_1887, REMINGTON, WINCHESTER, MP7, UMP, UZI`.

### Dual wielding

`DualWielding`, `DualWieldIfPossible`, `mDualWieldCode`, `mCanAimAndDualWield`,
`mRequiresDualWield`, `mWasDualWielding`, `mOnlyEnableWhileDualWielding`,
`mDualWieldingAimViewOffset`, `SwapFireButtonsWhenDualWielding`,
`SetSwapFireButtonsWhenDualWielding`, `swapFireButtonsWhenDualWieldingDefault`, `mAkimbo`,
`mDualWieldWhenGivingItems`. See `DUAL_WIELD.md`.

### The demon arms

`DemonArmGrabStateBehavior`, `DemonArmSwipeStateBehavior`, `DemonArmSwipeFireBehavior`,
`BDemonArmGrabFireBehavior`, `mGrabDemonArmWeapon`, `mSwipeDemonArmWeapon`,
`DemonArmGrabEquipEnd/UnequipEnd`, `DemonArmSwipeEquipEnd/UnequipEnd`, `DemonArmGrabPart`,
`DemonArmHead`, `DemonArmTorso`, `DemonArmLimbs`, `DemonArmGrabHint`,
`DemonArmGrabFailedHint`, `DemonArmGrabTapXToBreakAction`, `mDemonArmGrabResistAngle`,
`mDemonArmGrabResistData`, `mDemonArmSlashResistAngle`, `mDemonArmSlashResistData`,
`mDemonArmKillSlomoDelay`, `mDemonArmTalent` / "Talent that activates bladed morphs",
`mDemonArmTargetColor`, `mDemonArmUntargetColor`, "Tint if targeted by DemonArm",
`mDemonArmEyeFlareDuration/MaxSize/Tint`, `mDemonArmTransitionInTime/OutTime`,
`mDemonArmAttenMultiplier`, `mDoDemonArmCheck`, `mRequiresDemonArm`,
`mRequiresSwipeDemonArm`, `mBlocksDemonArms`, `mStripDemonArms`, `mHoldingInDemonArm`,
`mStopDashOnDemonArm`, `mUseDemonArmRange` / "When available, will use the DemonArm's grab
range to determine glow visibility", "Radius of cylinder with length <HoldRange> that
determines reach of the demon arm", "Base grab range for the Demon Arm", "If no DemonArm is
found, a ray is cast to see what my target it", "If true, the DemonArms cannot grab or swipe
while the anim is playing", `Blades_BlendShaper.DemonArmSlashyBladedMorph`. Tentacles:
`SecondaryTentacle`, `mTentacleAttachBone` / "Tentacle attach bone [owner]", `mTentacleType`,
`mTentacleExtraExtension`, `TentacleUnequipEnd`. See `TENTACLES.md`.

### Attachments (how weapons and grabbed things ride bones)

`/EE/Types/Engine/AttachmentProxy`, `WeaponAttachment`, `WeaponAttachmentEx`, `AttachEntity`,
`AttachParent`, `GetAttachParent`, `SetAttachLocalSpace`, `mAttachBone` / "Attach bone",
"Attachment for specific hand", "Attachment for 1st or 3rd person", "Attachment offset",
"Attachment rotation", `DetachAttachment`, `GetAllAttachments`. Engine-side attachment
exists, which is the precondition for "engine-side writes let attachments follow for free".

### Ragdoll and gore

`AnimTreeNodes::Ragdoll`, `Ragdoll_HEAD/TORSO/PELVIS/ARM_*/FOREARM_*/THIGH_*/SHIN_*`,
`SlashRagdoll`, `mCanSliceRagdoll`, `mCensorRagdolls`, `mThrowRagdollVelocity`,
`mRagdollPartHoldData`, `severRagdollImpulse`. Relevant to what a grabbed body does at the
tentacle tip (T7).

## 6. Dead ends (do not re-hunt)

None yet. Format: heading with the ticket and date, what was tried, the measurement that
killed it, and what to do instead.

## 7. Evidence handling

None yet. Format: what was lost, why, and the rule that prevents it. (Dishonored's first two:
log rotation was one deep; the crash file carried no run identity.)

## 8. Derivation records (addresses)

None yet. Every entry: the symbol name used in `patterns.h`, the RVA, the byte-verify
signature, HOW it was found (string xref, CTAB match, caller census, live probe), the date and
the build fingerprint it was derived on.
