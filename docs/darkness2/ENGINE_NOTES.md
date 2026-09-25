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

Consequence for the mod: the frame comes out of a D3D9Ex device and must be carried into a
D3D11 texture on the runtime's adapter for OpenXR, exactly the Dishonored arrangement.

### The create path (measured 2026-09-25, VR-231, two launches)

- The engine's D3D9 init (`kDx9InitFn`, 0xCF2C30, s8) calls `GetProcAddress("Direct3DCreate9Ex")`
  and, when the setting byte `Graphics.EnableDirect3D9Ex` (0x10E3BDC, default 1, registered at
  0xEA7AA0) is set, **`Direct3DCreate9Ex(0x20, &out)`**; `Direct3DCreate9(0x20)` is the fallback
  when the export is missing or the setting is 0. Measured: `Direct3DCreate9Ex(SDK=32)` entered
  from our module, caller 0xCF2CCC (inside the init), once per run.
- **`CreateDeviceEx`** on adapter 0, `D3DDEVTYPE_HAL`, behaviour flags 0x50
  (`HARDWARE_VERTEXPROCESSING | MULTITHREADED`), first as a 16x16 windowed X8R8G8B8 device
  (backbuffer count 1, no multisample, `D3DSWAPEFFECT_DISCARD`, no auto depth stencil,
  `D3DPRESENT_INTERVAL_IMMEDIATE`), then **one `Reset`** to 2560x1440 fullscreen with the same
  format and interval (the desktop's mode on the dev PC). `QueryInterface(IID_IDirect3DDevice9Ex)`
  succeeds: the device is a 9Ex device.
- The game presents with **`PresentEx(0, 0, 0, 0, flags)`**, never `Present`, from one thread
  (the main thread, which also ran DllMain), through its present wrapper at 0x920EE0 (s8).
  `EndScene` is called twice per present (two scene passes); its wrapper is 0x654FA0.
- Present rate with the mod loaded and vsync as the game left it: about 64 Hz at the title, in
  the pause menu and in the first alley in gameplay, over 2000 Hz on a loading screen (the
  interval is IMMEDIATE; the game paces itself).
- The window: class `EvolutionEvolutionGfxD3D`, title `The Darkness II`, 2560x1440 at 0,0,
  style 0x14000000 (a popup without a caption: exclusive fullscreen).

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

**Measured offline for R2 (VR-233, 2026-09-25; the addresses are in s8 and `patterns.h`):**

- **The VM.** One main `lua_State`, created by `lua_newstate` at startup and stored in a
  static holder (`*(lua_State**)0x10DD9C4`, written at 0x92DA25; the `ScriptSystem`
  singleton at 0x10DDAD8 keeps a second copy at +8). `lua_Number` is **float**, not double:
  `TValue` is 8 bytes (`lua_gettop` shifts by 3), `lua_pushnumber` loads with `movss`. Four
  engine bytes sit BEFORE each state (`L-4`, the engine's script-thread object; 0 on the main
  state), which is how `Sleep` decides whether it may yield. The engine installs its own panic
  handler (0xD502E0), so the stock `PANIC: unprotected error` string is absent. Pseudo-indices
  are stock (`LUA_GLOBALSINDEX` = -10002).
- **How scripts run.** Every shipped callback (`Initialize`, `Start`, `Update`, `On*`) runs as
  a coroutine through `ScriptSystem::Resume` (0xC51FA0, thiscall, 79 static callers), the only
  engine caller of `lua_resume` (0xB72770). `lua_pcall` (0x772F10) has **six** static callers,
  all at VM creation (the `Installed CheckGlobal` chunk in `ScriptSystem::Init`, 0x6DE010) or in
  debug paths, and none of the 430 shipped scripts calls `pcall`/`xpcall`. A wrap on
  `lua_pcall` therefore catches NOTHING during play: the engine's per-tick script entry is
  `ScriptSystem::Resume`, and the state pointer comes from the holder. The mod's Lua lane is
  designed on that (`docs/ARCHITECTURE.md` "The Lua lane"): the `lua_pcall` wrap stays as the
  negative control whose count must read 0.
- **The bindings are real SWIG 1.3** (`swig_runtime_data_type_pointer4`, custom error strings
  `SWIG_check_num_args`, `SWIG_fail_arg`, `SWIG_fail_ptr`): ten `luaopen_` modules (Engine,
  GraphicsRes, Effects, Game, Sound, Npc, UISys, Script, Framework, D2_Game), **177 classes,
  1143 methods, 208 attributes** plus the Script module's 44 globals (`IsNull`, `Lerp`,
  `Broadcast`, ...). The runtime `types` arrays live in BSS; the static `type_initial` arrays
  are what `tools/swig-dump.py` walks. The whole map with wrapper addresses is
  `docs/darkness2/swig-api.md`.
- **The FOV route the shipped script uses** (`/D2/Scripts/Player/SetFov.lua`):
  `gRegion:GetHumanPlayers()` -> `[1]:GetAvatar()` -> `:CameraControl()` (BaseAvatar, the
  avatar's vtable +0x318) -> `IsNull(c) or c:IsNullCameraController()` -> `c:SetBaseFovOverride(f)`.
  The wrapper (0xDBDB90) converts the object through its swig type, reads the float with
  `lua_tonumber` and calls the camera controller's **vtable +0xE4** with one float: a native
  route that bypasses script entirely once the controller pointer is known. `gRegion` exists
  only once a level is loaded; `Sleep` yields and must never be called from a chunk run on
  the main state.
- **Thread strings**: no `RenderThread`/`GameThread`/`MainThread`; the engine names
  `GraphicsWorker` (a `mGraphicsWorkerType` setting), `JobMgr`/`JobWorker`, `CacheThreadImpl`,
  `DefragWorker`. Whether the pump thread is the present thread is still a runtime measurement
  (s2: PresentEx is on the main thread; the pump canary counts once per present).

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

Format: heading with the ticket and date, what was tried, the measurement that killed it, and
what to do instead.

### Finding the Present call site statically (VR-232, 2026-09-25)

Tried: a census of `mov reg,[reg+0x44] ; call reg` shapes (the COM `Present` slot) over `.text`
and its intersection with the `EndScene` shape (+0xA8). 1306 hits; the engine wraps the device
in its own driver class whose vtable also has a slot at +0x44, so the shape is not the API
call. Killed by: the three candidates the intersection produced were all engine-internal.
Instead: the `Present` vtable hook logs its exe return address (0x920F2E on the first frame) and
the function start is read from the padding before it. Runtime derivation, then byte-verify.

## 7. Evidence handling

Format: what was lost, why, and the rule that prevents it.

- **A 64-bit PowerShell sees 7 of a 32-bit game's 110 modules** (2026-09-25). The first module
  census from outside said "no app-dir d3d9.dll in the process", a false zero, while the
  in-process loader list and the log said the opposite. `tools/module-census.ps1` now re-runs
  itself under the 32-bit PowerShell. Rule: enumerate a 32-bit process from a 32-bit process.
- **The backbuffer capture cannot show the Steam overlay.** `shot` reads the backbuffer inside
  our Present hook, before the call goes on to the overlay's own hook, so an overlay screenshot
  is always the plain frame. The overlay's presence is measured by its module
  (`GameOverlayRenderer.dll`, loaded in both runs) and by its displacement of the unhandled
  exception filter (re-armed once per run), not by a picture.
- **A 60 ms key tap is not seen by the title screen; 400 ms is.** The gameswf title screen
  polls slower than a frame. `tools/boot.ps1` holds Space for 400 ms; menus take 150 ms taps.
- **`crash test` leaves three fingerprints, not one.** After the deliberate fault the game's own
  handler path raises a stack overflow (0xC00000FD) and a second access violation inside our
  module before the process ends; the first fingerprint and the minidump are the evidence, the
  two follow-ons are the exit path.

## 8. Derivation records (addresses)

Every entry: the symbol name used in `patterns.h`, the VA (RVA = VA - 0x400000), the
byte-verify signature, HOW it was found (string xref, caller census, live measurement), the
date and the build. All entries below: build 2012-03-20 (TimeDateStamp 0x4F68A875, SizeOfImage
0xDEC000, 14,291,512 bytes), derived 2026-09-25 with `tools/disasm-rva.py` and
`tools/pe-xref.ps1` unless a line says "live".

| Symbol | VA | Signature (first bytes) | Derivation |
|---|---|---|---|
| `kExeTimeDateStamp`, `kExeSizeOfImage`, `kExeFileSize` | - | 0x4F68A875, 0xDEC000, 14291512 | PE header + file size; checked at every launch (`fingerprint:` line); a mismatch refuses every code hook |
| `kDx9InitFn` | 0xCF2C30 | `83 EC 1C 56 8B F1 83 7E 04 00` | xref of the string `D3D9.DLL` (0xF9DC80): pushed at 0xCF2C43 into the exe's LoadLibraryA wrapper. The function then runs the registry DirectX version check (0xAD5C50 -> 0xC471F0: `HKLM\SOFTWARE\Microsoft\DirectX\Version`, failure = `Menu/DirectXMissing`, too old = `Menu/DirectXOldVersion`), `GetProcAddress("Direct3DCreate9Ex")`, tests byte 0x10E3BDC, and creates. Detour length 6 (`sub esp,1Ch ; push esi ; mov esi,ecx`) |
| `kDx9InitCaller` | 0xC27176 | `E8 B5 BA 0C 00` | `pe-xref` caller census of 0xCF2C30: exactly one E8 caller, no vtable references |
| `kLoadLibraryWrapper` | 0xA556E0 | `55 8B EC 6A FE 68 28 66 09 01` | the target of the call at 0xCF2C48; an SEH frame around `call [LoadLibraryA]` (0xA55748) with no path manipulation; also loads `DINPUT8.DLL`, `xinput1_3.dll`, `KERNEL32.DLL` |
| `kD3D9ExEnableFlag` | 0x10E3BDC | data byte | xref of the string `EnableDirect3D9Ex` (0xF52FD4) at 0xEA7AA2, section `Graphics`; the registration sets the byte to 1 at 0xEA7AC5; read at 0xCF2CBB. Read-only; logged at every launch |
| `kMsgPumpFn` | 0xB2E5E0 | `83 EC 1C 53 55 56 57 8B F9` | the only reference to the `PeekMessageA` IAT slot (0xEEB3FC) is `mov ebx,[slot]` at 0xB2E651; the enclosing function begins after int3 padding at 0xB2E5E0 and is a `PeekMessageA` / `TranslateAcceleratorA` / `TranslateMessage` / `DispatchMessageA` loop. 0 E8 callers and 1 `.rdata` reference: a virtual method. Live: hits/s equals the present rate (64/s), so it runs once per tick. Detour length 5 |
| `kPumpCallSite` -> `kPumpCallTarget` | 0xB2E6E9 -> 0x924B80 | `E8 92 64 DF FF` | the last call inside the pump; the target reads and decrements a global counter (0x10E2130) and returns it (17 static callers). The call-site canary rewrites the rel32 |
| `kPresentWrapperFn` | 0x920EE0 | `55 56 57 8B F9 E8 B6 B1 B6 FF` | **live**: `PresentEx` returned into the exe at 0x920F2E on the first frame of both runs; the function begins after int3 padding at 0x920EE0 and calls device vtable slot 0x1E4/4 = 121 (`PresentEx`) with `(0,0,0,0,flags)`. 1 E8 caller (0xB36EF2). Detour length 5 |
| `kEndSceneWrapperFn` | 0x654FA0 | `8B 81 64 29 00 00 8B 08` | **live**: `EndScene` returned into the exe at 0x654FB1; a 17-byte thunk `mov eax,[ecx+0x2964] ; mov ecx,[eax] ; mov edx,[ecx+0xA8] ; push eax ; call edx ; ret`. 0 E8 callers, 1 `.rdata` reference (virtual). Alternative hot site; not hooked |
| `kLuaStateHolder` | 0x10DD9C4 | data | xref of the `lua_newstate` (0x8CED10) result: `mov [esi+4],eax` at 0x92DA25 with esi = the holder object at 0x10DD9C0 (its getter 0xAF02E0 zeroes +4 and +8 first); `ScriptSystem+8` (0x10DDAE0) holds a copy |
| `kScriptResumeFn` | 0xC51FA0 | `83 EC 7C 53 55 8B E9 8B 8C 24 88 00 00 00` | the enclosing function of the only `E8` call to `lua_resume` (0xB72770, at 0xC52095); thiscall, `ret 4`, 79 static callers; the argument at `[esp+4]` is the engine's script-thread object, the state passed to `lua_resume` is derived from it (+0x14 through 0xD50D40). Detour length 5 (`sub esp,7Ch ; push ebx ; push ebp`) |
| `kLuaResume` | 0xB72770 | `56 8B 74 24 08 8A 46 06 3C 01` | references `cannot resume non-suspended coroutine` and `C stack overflow`; the 5.1.3 `lua_resume` shape (status byte at L+6, `ci == base_ci` check at +0x14/+0x28). Detour length 5 |
| `kLuaPCall` | 0x772F10 | `8B 4C 24 10 83 EC 08 56 8B 74 24 10` | calls `luaD_pcall` (0x673160, which references `not enough memory` / `error in error handling` through `luaD_seterrorobj` 0xB504C0) with `f_call` 0xD5A6C0, then the `nresults == -1` adjust; 6 static callers (`ScriptSystem::Init` 0x6DE010, the debug paths, the `pcall` base function). Detour length 7 |
| `kLuaLLoadBuffer` | 0xAF8F40 | `83 EC 08 8B 44 24 10 8B 54 24 18` | pushes the `getS` reader (0xD5A7F0) and calls `lua_load` (0x922DE0, default chunkname `?` at 0xF8E714); called by `db_debug` with `=(debug command)` |
| `kLuaGetTop` / `kLuaSetTop` | 0xCFFEC0 / 0x4D2EF0 | `8B 4C 24 04 8B 41 08 2B 41 0C C1 F8 03 C3` / `8B 4C 24 08 8B 44 24 04 85 C9 7C 37` | `(top - base) >> 3` (an 8-byte TValue: float numbers); `lua_settop` is called as `lua_pop` at the end of every `SWIG_init`. `lua_gettop` is the first call of every SWIG wrapper |
| `kLuaToLString`, `kLuaType`, `kLuaPushString`, `kLuaPushNumber`, `kLuaGetField`, `kLuaSetField`, `kLuaError` | 0x706A80, 0x93B370, 0xAD0BB0, 0xAE58D0, 0x710020, 0x80D360, 0x517F30 | s8 `patterns.h` | from the SWIG wrappers' and the base library's call shapes (`lua_pushstring` pushes `"swig_type"` in every `SWIG_init`; `lua_setfield(L, GLOBALSINDEX, "INF")` in the engine's library opener 0xD50AF0; `lua_error` calls `luaG_errormsg` 0xD165B0). `lua_getfield` and `lua_setfield` share their first 24 bytes: verify by VA |
| The SWIG modules | s3, `swig-api.md` | data | each `SWIG_init` pushes `"swig_type"` (0xF35F4C) and `"swig_equals"` (0xFAEB9C) and passes its `swig_module_info` to 0xD8C3D0; the static `type_initial` arrays sit beside the module structs. Layouts: `swig_type_info` 24 bytes, `swig_lua_class` 32 bytes, methods `luaL_Reg` 8 bytes, attributes 12 bytes |
| `kSetDllDirectoryCall` (documented, not in patterns.h) | 0x452640 | - | xref of the string `SetDllDirectoryA` (0xF1A254): `GetProcAddress(kernel32, "SetDllDirectoryA")` then a call with the caller's string. Live: at our DllMain `GetDllDirectory` returned `<game dir>\/Tools/PhysX/x86/`, so it runs before the D3D9 load and points at the PhysX folder |

## 9. R0 verdict: the loading route (VR-231, 2026-09-25)

**Route (a) wins: a `d3d9.dll` next to `DarknessII.exe`, loaded by the exe's own bare-name
`LoadLibraryA("D3D9.DLL")`.** Measured, not assumed:

| Measurement | Result |
|---|---|
| How the exe loads the renderer | `LoadLibraryA("D3D9.DLL")` through the wrapper at 0xA556E0 (s8); no path prepended; `LoadLibraryExA/W` are not imported, so no `LOAD_LIBRARY_SEARCH_SYSTEM32` route exists |
| The search directory at that moment | The application directory (the exe's folder) is searched first; the exe HAD called `SetDllDirectoryA("<game dir>/Tools/PhysX/x86/")` before the load (the value `GetDllDirectory` returned inside our DllMain), which replaces the current-directory slot in the search order and cannot displace the application directory |
| Where the loader took our module from | `D:\...\Darkness II\D3D9.DLL` (`GetModuleFileName` of our own module), 529 ms after process creation, on the main thread, with the game's `Tools\` DLLs (`steam_api.dll`, PhysX) and `GameOverlayRenderer.dll` already in the loader list |
| The create call entered from our module | `Direct3DCreate9Ex(SDK=32)`, caller 0xCF2CCC inside the D3D9 init, call #1, in **2 of 2** launches so far (the count continues with every launch of the harness; each run's log has the line) |
| The system d3d9 beside us | `C:\WINDOWS\system32\d3d9.dll` (SysWOW64 under redirection) loaded by us as the backend; the 32-bit module census from outside shows both |
| The Steam overlay | `GameOverlayRenderer.dll` loaded in both runs; it displaced the unhandled exception filter once per run (our re-arm logged it), which is the overlay installing its own hooks. A picture of it is not obtainable from the backbuffer (s7) |
| Exports forwarded | all 23 of the system table, by ordinal for the six unnamed ones (`tests/golden/d3d9-exports.txt`); the game resolved `Direct3DCreate9Ex` by name |

**Fallbacks**, in order, if a future Windows or Steam change stops the app-dir DLL being chosen:
(b) an `xinput1_3.dll` proxy (delay-loaded by the exe from the same search order) that hooks
the exe's `LoadLibraryA` wrapper at 0xA556E0 (s8) or its `GetProcAddress` result for
`Direct3DCreate9Ex`; (c) a suspended-launch injector, which needs Steam's `-applaunch` and is
the last resort because it changes how the game is started.

**Consequences recorded**: the game is a D3D9Ex host (s2); every mod hook that must precede the
device is installed from the `Direct3DCreate9Ex` wrapper, never DllMain; nothing about the route
depends on the working directory.

## 10. R1 verdict: in-memory hooks under CEG (VR-232, 2026-09-25)

**CEG tolerated four in-memory code hooks for a 33-minute session across a menu round trip, a
checkpoint reload and a level restart: no exit, no byte revert, no exception.** One run, one
data point; every later launch with canaries on adds to it.

The instrument: four canaries (`src/game/darkness2/canaries.cpp`), each byte-verified against
`patterns.h` and installed from the FIRST PRESENT (about 7 s after process creation, after the
engine's startup had run), each counting hits and re-reading its own bytes once a second:

| Canary | Site | Shape | Hits over the run | Bytes |
|---|---|---|---|---|
| `cold` | `kDx9InitFn` 0xCF2C30 | 6-byte `jmp` + nop over the prologue of code that ran once at startup | 0 (expected: the init ran before the hook) | intact every second |
| `tick` | `kMsgPumpFn` 0xB2E5E0 | 5-byte `jmp` over the message pump's prologue (game thread) | 841,948 at 64/s in menus, 494/s in gameplay (equal to the present rate: it IS once per tick) | intact |
| `callsite` | `kPumpCallSite` 0xB2E6E9 | the `call` rel32 inside the pump rewritten to a counting tail-jump stub | 841,948 (equal to `tick`) | intact |
| `hot` | `kPresentWrapperFn` 0x920EE0 | 5-byte `jmp` over the present wrapper's prologue | 841,948 (equal to the present count) | intact |

The run (`tools/soak.ps1 -Minutes 30 -Attach`, log archived under `build/logs/`): canaries
live at tick 18783062; gameplay from the save; the pause menu opened and closed at minute 10;
`RESTART CHECKPOINT` confirmed at minute 15 (the alley reloaded); `RESTART LEVEL` confirmed
at minute 20 (the level's opening cinematic played: a full level load); movement and looks
every 20 s throughout; `quit` at minute 30 and a clean `WM_CLOSE` exit at tick 20785843. The
soak's own summary: `byte reverts/changes: 0`, `refused canaries: 0`, `exceptions recorded:
0`, `exit: clean`. The earlier crash-test run (s7) shows what the failure shape would look
like in the same files.

**What this does and does not say.** In-memory `E9` detours at a hot render site, a per-tick
game-thread site, a cold init site and a rewritten call site are not reverted and do not end
the process over 33 minutes and two loads, so no code range among these four needs avoiding
and hooks installed after the engine is up need no further wait. It does NOT say that a hook
installed BEFORE the engine's startup (at DllMain, or from the `Direct3DCreate9Ex` wrapper,
which runs 7 s earlier) survives; nothing resolves at init here anyway, and the rule stands.
It does not say anything about writes to the exe's data pages or its import table. If a later
site ever reverts, the per-second line names the tick and the bytes.

**Rule for the mod**: engine code hooks install from the present thread once the game is up
(the framework's `present_tick`), byte-verified, default OFF, with the 1 Hz re-read left on in
every build so a regression shows up as a `REVERTED` line, not a silent exit.

## 11. The runtime layer on this host (VR-241, 2026-09-25)

The Dishonored runtime layer (`core/vr/openxr_runtime`, verbatim behind the rename in
`docs/ARCHITECTURE.md`) came up on this game at the first launch that carried it. Measured on
the simulator (launch 3 of the R0 count; log build 31de821-dirty):

| Measurement | Result |
|---|---|
| The launch route for the simulator | **A direct `DarknessII.exe` start refuses**: a modal `Error` box, `Failed to initialize Steam. Make sure the Steam client is running and try again.`, with the Steam client running, BEFORE `d3d9` is asked for (no log line at all). The exe's `steam_api` needs the Steam-launched context. `xrsim-launch.ps1 -ViaSteam` is the only simulator route: the manifest travels in `[VR] XrRuntimeJson`, the layer sets `XR_RUNTIME_JSON` for the process and the loader property, and the launcher restores the ini once the runtime line appears |
| Implicit API layers | one registered, `XR_APILAYER_VIRTUALDESKTOP_oculus_compatibility` (HKLM 64-bit view, x64 DLL): the guard opted this process out through its own `DISABLE_...` variable. Registry untouched |
| Instance / system | `d2vr-xrsim` 1.0.0; `Meta Quest 3`, 2064x2208 per eye recommended, 16 layers |
| The D3D11 device | on `NVIDIA GeForce RTX 4060` LUID `00000000-0000CE56`, MATCHES the LUID the runtime asked for (the game's own 9Ex adapter 0 reports the same LUID); feature level 0xB000 |
| Session | IDLE -> READY -> SYNCHRONIZED -> VISIBLE -> FOCUSED about 200 ms after the first present; `xr: pipeline READY` |
| Pacing | the pace thread is ON by the layer's default (`xr: pace thread started`); the runtime period 11.11 ms (90 Hz); the game presents 57-60/s in the alley (its own IMMEDIATE pacing), so the `stereo: rate` line says UNDER-SUBMITTING 0.60-0.66x every window: display slots go unfilled because the game is slower than the display, which is expected on the mono screen and not a stall |
| Swapchains | a pair at the game's 2560x1440, format 29 (`R8G8B8A8_UNORM_SRGB`, the layer's first preference; the sim offers it), 3 images each |
| The frame texture | the mono method's `R8G8B8A8_UNORM` 2560x1440 texture from the capture's `B8G8R8A8` (X8R8G8B8 = D3D9 format 22 uploads byte-for-byte) |
| The quad | 2.4 m x 1.35 m at 1.75 m in VIEW space; in the sim's 1032x1104 eye it covers 58.6% x 27.0% in BOTH eyes (`bboxPctL == bboxPctR`), offset 197 px between the eyes (the parallax of one quad seen from two eyes); the bbox does not move under 0/10/25/45 degrees of head yaw (`headlook.xrs`) |
| Capture cost, `deferred` | 3.5-5.6 ms per present at 2560x1440: rtd 1 us, lock 0, copy 1.1-1.4 ms, upload 2.4-4.2 ms, blit 1 us; 14.1 MB each way; the readback waits on the previous present's copy |
| Capture probe | `IDirect3DDevice9Ex` confirmed at runtime; `CreateRenderTarget` with a shared handle OK; the shared surface opens on D3D11 as format 88 (`B8G8R8X8_UNORM`): `[Capture] Mode=shared` is available on this device |
| Both eyes at the game's rate for 5 minutes | `tools/xrsim-soak.ps1 -Minutes 5` in the first alley: presents 24,425, submits 24,425 (ratio 1.000), the simulator layered 24,404 of them (0.999), FOCUSED throughout, present rate 68-92 Hz (mean 79.9), capture 3.5-3.8 ms per present (deferred), five per-eye captures with both eyes equal (10.4, 10.7, 10.0, 5.3, 4.9 percent non-black; the last two are the idle camera on the night sky, a real dark frame), no SUBMISSION IDLE, EXCEPTION, POISONED or REVERTED line, `[Canary] Tick=1` intact every 5 s |
| The refusing stubs | `stereo reentry` and `stereo aer` each refuse with their note (`... is a design stub, not implemented - staying on 'mono'`); `stereo status` still reads `method=mono`; the beat continues at the present rate |
| Capture cost, `shared` (the A/B) | `capture mode shared` live: 11-13 us per present (lock 7-8 us = the fence wait, blit 4-5 us, no copy, no upload) against 3.4-3.8 ms deferred; the present rate rose from 79-84 to 90 Hz (`MATCHED 1.00x`: one submit per display slot) because the readback was the tick's cost; the picture kept flowing (9.6 percent non-black both eyes); switching back to deferred live restored the 3.4-3.6 ms cost. One `[E] capture: GetRenderTargetData failed (0x8876086c)` from the bbox sampler at the instant of the switch (found, not fixed) |
| Alt-tab | `key alt down; key tab tap 120; key alt up` from the input lane minimised the exclusive-fullscreen game and it STOPPED PRESENTING (the seam, polled from Present, went deaf; `game-cmd` timed out). A `ShowWindow(SW_RESTORE)` + `SetForegroundWindow` from outside brought presents back at once, the queued `focus` ran, both eyes showed the frame again (8.65 percent non-black), FOCUSED. **No `Reset` happened**: the 9Ex device keeps its fullscreen mode across the minimise. The Reset path (`stereo::on_reset` before the device resets) is therefore exercised only by the startup Reset (before the first present) on this run; an in-game resolution change remains the way to measure `Reset #2` |

**Rules this fixes.** The simulator is reached through Steam only (TRAPS s12). The game's own
9Ex device is what the shared capture shares: `[Device] Ex` here means "the shared path is
permitted", never "create as Ex" (ARCHITECTURE decision log). The 64-bit VDXR compatibility
layer is opted out per process on every launch; its log line is expected.
