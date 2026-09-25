#define D2VR_CAT ::d2vr::log::Cat::cfg
#include "core/config/config.h"
#include "core/util/ini.h"
#include "core/util/log.h"
#include "core/util/paths.h"
#include <stdio.h>
#include <string.h>

namespace d2vr::config {
namespace {

Config g_cfg;
bool g_loaded = false;
char g_ini[MAX_PATH] = "";
const int kConfigVersion = 1;

// The default ini. tools/ini-golden.py extracts this literal: keep it one
// fprintf with the version as the only conversion.
bool write_default(const char* path)
{
    FILE* f = fopen(path, "wb");
    if (!f) return false;
    fprintf(f,
"; darkness2_vr.ini - The Darkness II VR mod. Written by the mod when absent.\n"
"; Lines starting with ; are comments. Every lever here defaults OFF or to the\n"
"; stock behaviour; the log (darkness2_vr.log) says what each run RESOLVED.\n"
"\n"
"[Config]\n"
"Version=%d\n"
"\n"
"[Log]\n"
"; error|warn|info|debug|trace for every category; the D2VR_LOG environment\n"
"; variable overrides this. Cats opens one lane: canary:debug,present:trace\n"
"Level=info\n"
"Cats=\n"
"\n"
"[Paths]\n"
"; Harness files (command.txt, ack.txt, status.json, dumps, shots). Empty =\n"
"; %%LOCALAPPDATA%%\\Darkness2VR; the D2VR_DATA_DIR environment variable overrides.\n"
"DataDir=\n"
"\n"
"[Canary]\n"
"; R1 (docs/ROADMAP.md): four in-memory code hooks that do nothing but count,\n"
"; to measure whether CEG tolerates them. Default OFF. A soak run sets them ON;\n"
"; the seam word `canary <cold|tick|callsite|hot> on|off` flips them live.\n"
"Cold=0\n"
"Tick=0\n"
"CallSite=0\n"
"Hot=0\n"
"; The per-canary hits/s and byte re-read line cadence, in seconds.\n"
"LogEverySeconds=1\n"
"\n"
"[Lua]\n"
"; R2 (docs/ROADMAP.md), the Lua lane: Enabled=1 installs three byte-verified wraps from the\n"
"; first present (ScriptSystem::Resume, lua_resume, lua_pcall) so that `lua run <text>` and\n"
"; `lua fov <deg>` run ONE chunk of the mod's own text on the engine's main lua_State, on the\n"
"; game thread, at the engine's own script entry. Default OFF. `lua on|off` flips it live;\n"
"; `lua status` prints the counters; the re-read line follows [Canary] LogEverySeconds.\n"
"Enabled=0\n"
"\n"
"[Overlay]\n"
"; The F10 panel (the simple form; Ref VR-275): one ImGui window drawn into the eye texture,\n"
"; hidden until F10 or the `overlay on|toggle` seam word. Enabled=0 disarms the key.\n"
"; UiScale=0 picks the text scale from the eye height; a value (0.8-3) overrides it.\n"
"Enabled=1\n"
"UiScale=0\n"
"\n"
"[Camera]\n"
"; The projection watch (VR-242): read the renderer's SS_Projection constant back at every\n"
"; upload and log the rendered FOV. Default OFF; `camera projwatch on|off` live; `camera\n"
"; eyetest` (which FOV write the renderer honours) switches it on for the test.\n"
"ProjWatch=0\n"
"\n"
"[VR]\n"
"; The OpenXR runtime layer. Runtime=auto takes the 32-bit runtime the system\n"
"; registers (Virtual Desktop's VDXR, Oculus) and falls back to the bundled SteamVR\n"
"; shim (d2vr_steamvr32.dll) when there is none and it ships; native|steamvr force one.\n"
"Runtime=auto\n"
"; XrRuntimeJson= a runtime manifest for THIS launch (the simulator, or a Steam launch\n"
"; that cannot carry XR_RUNTIME_JSON). Empty = the loader's choice. A stale value from\n"
"; a simulator run keeps the headset dark: tools\\xrsim-launch.ps1 restores it.\n"
"XrRuntimeJson=\n"
"; A 64-bit implicit OpenXR API layer (an OBS mirror, say) fails xrCreateInstance in\n"
"; this 32-bit process for every runtime. 1 = opt this process out of such a layer\n"
"; through the layer's own disable variable (the registry is never written); 0 = report only.\n"
"DisableBadApiLayers=1\n"
"\n"
"[Screen]\n"
"; Rung 1, the mono screen: the game frame on a quad DistanceMeters away and WidthMeters\n"
"; wide, the same image in both eyes. HeadLocked=1 keeps it in front of your eyes (turning\n"
"; your head turns the screen with it); 0 leaves it standing in the room. Live: `screen\n"
"; <distM> <widthM>` and `screen headlock on|off`.\n"
"DistanceMeters=1.75\n"
"WidthMeters=2.4\n"
"HeadLocked=1\n"
"\n"
"[Stereo]\n"
"; The stereo method the seam starts on: mono (rung 1, works) | aer | reentry (registered\n"
"; stubs that refuse and leave mono running). `stereo <name>` switches live; `stereo status`.\n"
"Method=mono\n"
"; Armed=0 parks the selected method (the game runs flat, the seam stays up).\n"
"Armed=1\n"
"\n"
"[Capture]\n"
"; How the D3D9 backbuffer reaches the D3D11 texture the runtime submits:\n"
";   sync     GetRenderTargetData + LockRect + upload, on the present thread, this present\n"
";   deferred the same readback queued one present behind (hides the CPU wait)\n"
";   shared   a fenced StretchRect into a D3D9Ex shared surface D3D11 opens (no readback);\n"
";            needs [Device] Ex=1 and the probe's AVAILABLE\n"
";   off      the live A/B: the picture freezes\n"
"; `capture mode <name>` switches live; `capture` prints the cost per present.\n"
"Mode=deferred\n"
"; SharedWait=0 delivers the previous present's shared slot (no wait); 1 waits for this one's fence.\n"
"SharedWait=0\n"
"; The content bounding-box sample cadence in ms (each is a full-frame CPU readback); 0 = off.\n"
"BboxMs=30000\n"
"\n"
"[Device]\n"
"; This game creates its own D3D9Ex device (ENGINE_NOTES s2), so Ex here does not create\n"
"; one: Ex=1 PERMITS the shared-surface capture on it, Ex=0 forces the readback modes.\n"
"Ex=1\n",
        kConfigVersion);
    fclose(f);
    return true;
}

} // namespace

const Config& get() { return g_cfg; }
bool loaded() { return g_loaded; }

const char* ini_path()
{
    if (!g_ini[0]) d2vr::paths::in_game_dir(g_ini, "darkness2_vr.ini");
    return g_ini;
}

void load()
{
    if (g_loaded) return;
    g_loaded = true;
    const char* ini = ini_path();
    if (GetFileAttributesA(ini) == INVALID_FILE_ATTRIBUTES) {
        if (write_default(ini)) D2VR_INFO("config: wrote the default ini to %s", ini);
        else D2VR_WARN("config: could not write the default ini to %s (err %lu) - running on defaults", ini, GetLastError());
    }
    g_cfg.version = d2vr::ini::read_int(ini, "Config", "Version", 0);
    d2vr::ini::read_string(ini, "Log", "Level", "", g_cfg.logLevel, sizeof(g_cfg.logLevel));
    d2vr::ini::read_string(ini, "Log", "Cats", "", g_cfg.logCats, sizeof(g_cfg.logCats));
    d2vr::ini::read_string(ini, "Paths", "DataDir", "", g_cfg.dataDir, sizeof(g_cfg.dataDir));
    g_cfg.canaryCold = d2vr::ini::read_int(ini, "Canary", "Cold", 0);
    g_cfg.canaryTick = d2vr::ini::read_int(ini, "Canary", "Tick", 0);
    g_cfg.canaryCallSite = d2vr::ini::read_int(ini, "Canary", "CallSite", 0);
    g_cfg.canaryHot = d2vr::ini::read_int(ini, "Canary", "Hot", 0);
    g_cfg.canaryLogHz = d2vr::ini::read_int(ini, "Canary", "LogEverySeconds", 1);
    if (g_cfg.canaryLogHz < 1) g_cfg.canaryLogHz = 1;
    g_cfg.luaEnabled = d2vr::ini::read_int(ini, "Lua", "Enabled", 0);
    g_cfg.overlayEnabled = d2vr::ini::read_int(ini, "Overlay", "Enabled", 1);
    g_cfg.overlayUiScale = d2vr::ini::read_float(ini, "Overlay", "UiScale", 0.0f);
    g_cfg.cameraProjWatch = d2vr::ini::read_int(ini, "Camera", "ProjWatch", 0);
    d2vr::ini::read_string(ini, "VR", "Runtime", "auto", g_cfg.vrRuntime, sizeof(g_cfg.vrRuntime));
    d2vr::ini::read_string(ini, "VR", "XrRuntimeJson", "", g_cfg.vrRuntimeJson, sizeof(g_cfg.vrRuntimeJson));
    g_cfg.vrDisableBadApiLayers = d2vr::ini::read_int(ini, "VR", "DisableBadApiLayers", 1);
    g_cfg.screenDistanceM = d2vr::ini::read_float(ini, "Screen", "DistanceMeters", 1.75f);
    g_cfg.screenWidthM = d2vr::ini::read_float(ini, "Screen", "WidthMeters", 2.4f);
    g_cfg.screenHeadLocked = d2vr::ini::read_int(ini, "Screen", "HeadLocked", 1);
    d2vr::ini::read_string(ini, "Stereo", "Method", "mono", g_cfg.stereoMethod, sizeof(g_cfg.stereoMethod));
    g_cfg.stereoArmed = d2vr::ini::read_int(ini, "Stereo", "Armed", 1);
    d2vr::ini::read_string(ini, "Capture", "Mode", "deferred", g_cfg.captureMode, sizeof(g_cfg.captureMode));
    g_cfg.captureSharedWait = d2vr::ini::read_int(ini, "Capture", "SharedWait", 0);
    g_cfg.captureBboxMs = d2vr::ini::read_int(ini, "Capture", "BboxMs", 30000);
    g_cfg.deviceEx = d2vr::ini::read_int(ini, "Device", "Ex", 1);

    if (g_cfg.dataDir[0]) d2vr::paths::set_data_dir(g_cfg.dataDir);
    // The environment wins over the ini for the log levels (set from DllMain).
    char env[8] = "";
    if (!GetEnvironmentVariableA("D2VR_LOG", env, sizeof(env)) || !env[0]) {
        char cats[8] = "";
        const bool envCats = GetEnvironmentVariableA("D2VR_LOG_CATS", cats, sizeof(cats)) && cats[0];
        d2vr::log::configure(g_cfg.logLevel, envCats ? "" : g_cfg.logCats);
    }
    D2VR_INFO("config: %s (version %d%s) Log.Level=%s Log.Cats='%s' Paths.DataDir='%s' -> data %s",
              ini, g_cfg.version, g_cfg.version == kConfigVersion ? "" : " - NOT the current version",
              g_cfg.logLevel, g_cfg.logCats, g_cfg.dataDir, d2vr::paths::data_dir());
    D2VR_INFO("config: [Canary] Cold=%d Tick=%d CallSite=%d Hot=%d LogEverySeconds=%d  [Lua] Enabled=%d  [Overlay] Enabled=%d UiScale=%.2f",
              g_cfg.canaryCold, g_cfg.canaryTick, g_cfg.canaryCallSite, g_cfg.canaryHot, g_cfg.canaryLogHz, g_cfg.luaEnabled,
              g_cfg.overlayEnabled, g_cfg.overlayUiScale);
    D2VR_INFO("config: [Camera] ProjWatch=%d", g_cfg.cameraProjWatch);
    D2VR_INFO("config: [VR] Runtime=%s XrRuntimeJson='%s' DisableBadApiLayers=%d  [Screen] DistanceMeters=%.2f WidthMeters=%.2f HeadLocked=%d",
              g_cfg.vrRuntime, g_cfg.vrRuntimeJson, g_cfg.vrDisableBadApiLayers,
              g_cfg.screenDistanceM, g_cfg.screenWidthM, g_cfg.screenHeadLocked);
    D2VR_INFO("config: [Stereo] Method=%s Armed=%d  [Capture] Mode=%s SharedWait=%d BboxMs=%d  [Device] Ex=%d",
              g_cfg.stereoMethod, g_cfg.stereoArmed, g_cfg.captureMode, g_cfg.captureSharedWait, g_cfg.captureBboxMs, g_cfg.deviceEx);
}

} // namespace d2vr::config
