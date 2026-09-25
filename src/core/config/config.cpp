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
"LogEverySeconds=1\n",
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
    D2VR_INFO("config: [Canary] Cold=%d Tick=%d CallSite=%d Hot=%d LogEverySeconds=%d",
              g_cfg.canaryCold, g_cfg.canaryTick, g_cfg.canaryCallSite, g_cfg.canaryHot, g_cfg.canaryLogHz);
}

} // namespace d2vr::config
