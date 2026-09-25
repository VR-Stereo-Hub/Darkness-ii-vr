# The game's own config

What is known about where The Darkness II keeps its settings, and the one rule: **the mod
does not write them.**

## Read this first

- The settings on disk are **obfuscated**. `%APPDATA%\DarknessII\EE.cfg` (8,206 bytes),
  `Editor.cfg` (363), `<steamid>\settings` (1,852) and the save `<steamid>\CONTINUE.SAV`
  (61,427) each start `00 00 xx xx` followed by scrambled bytes. A repeating-key XOR did not
  decode them (there is a weak 36-byte periodicity and no clean plaintext), so they may be
  XOR plus compression or a stream cipher. Nobody has needed to decode them yet, and the
  mod's design does not require it.
- **Do not write these files.** Even if the encoding were known, a settings write from
  outside the engine has two owners, and "a setting with two persistent homes has no owner"
  is the first trap in `docs/TRAPS.md`. Request settings through the engine's own apply path
  (the resolution apply the options menu calls) or through the Lua lane (R2) where a script
  already does it (`SetFov.lua` calls `camCtrl:SetBaseFovOverride`).
- The mod's own settings live in `darkness2_vr.ini` next to the exe (planned), with one home
  per key and the resolved value logged at startup.

## What the exe and the scripts say the settings are

| Setting | Where the name was seen | Owner script (from the menu Lua) |
|---|---|---|
| `Graphics.FullScreen`, `FullScreenSizeX/Y`, `DisplayMode`, `Resolution` | exe strings | `OptionsDisplayCustomize.lua` |
| `Graphics.AutoDetectGraphicsSettings` | exe | |
| `VSyncMode`, `VSyncSlack`, `VerticalSync` (Auto/On/Off) | exe, Lua | `OptionsDisplayCustomize.lua` |
| Aspect ratio: `AutoDetect, FullScreen4x3, WideScreen16x9, WideScreen16x10, Custom` | exe | |
| `TextureQuality`, `ShadowQuality`, `Decals`, `Brightness`, `EnableFXAA` | exe, Lua | `OptionsDisplayCustomize.lua` |
| `EnableNVidia3DVision`, `Stereo3DConvergence`, `Stereo3DSeperation` | exe | (no menu; R4) |
| `AimSensitivity`, `AimAssist`, `Vibration`, `InvertY`, `SouthPawControls` | Lua | `OptionsControls.lua` |
| `Hints`, `SubTitles`, `HUD`, `EssenceMessages` | Lua | `OptionsGame.lua` |
| FOV | no menu entry in the shipped scripts; a Steam patch added a slider (community reports about 85 degrees max); internally `mAllowCustomFov`, "User profile custom FOV will apply to this Avatar", `CmdSetBaseFOV`, `SetBaseFovOverride` | `SetFov.lua` (a script that sets it) |
| `App.DefaultCmdLine` | exe | |
| Config sections | `/Configs/EE.cfg/Windows_Config`, `/Configs/Editor.cfg/{MainGameWindowed, MainGameFullScreen, ...}` | |

## Free instruments the game ships (to confirm in R3)

- `-console` on the command line and a `Cmd*` dispatcher (`CmdSetBaseFOV`,
  `CmdToggleFreeCamera`, `CmdToggleCameraIndependant`, `CmdAIKillNPC`). If the console opens,
  it is the cheapest instrument in the game.
- `-log` / `-nolog` / `-verbose`: whether the engine writes its own log, and where.
- `mDebugDrawAnimationAndIK`, `mDrawIKTargetPoints`, `ShowIKStats`, "Draw the path of the
  original animation, the animation with IK, and other information": debug draws for the
  animation system, reachable by name once R2 lands.
- The in-exe editor (`-editor`, "Darkitect"). Not a mod instrument, but its existence means
  the engine's reflection is complete at runtime.

## What the mod sets on install (planned)

Nothing in the game's files. Dishonored sets four values in the game's ini on install
because UE3 reads a plaintext ini; here the equivalents (resolution, vsync, fullscreen) are
requested live through the engine's apply path and reported in the log, and the render size
is asked through whatever route S1 finds honoured.
