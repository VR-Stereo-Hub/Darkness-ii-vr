#include "core/util/paths.h"
#include <stdio.h>
#include <string.h>

namespace d2vr::paths {
namespace {
char g_game[MAX_PATH] = "";
char g_module[MAX_PATH] = "";
char g_exe[MAX_PATH] = "";
char g_data[MAX_PATH] = "";
char g_dumps[MAX_PATH] = "";
char g_shots[MAX_PATH] = "";

const char* sub_dir(char* slot, const char* name)
{
    if (!slot[0]) {
        snprintf(slot, MAX_PATH, "%s\\%s", data_dir(), name);
        CreateDirectoryA(slot, nullptr);
    }
    return slot;
}
}

void init(HINSTANCE self)
{
    GetModuleFileNameA(self, g_module, MAX_PATH);
    GetModuleFileNameA(nullptr, g_exe, MAX_PATH);
    strncpy(g_game, g_module, MAX_PATH - 1);
    g_game[MAX_PATH - 1] = 0;
    char* slash = strrchr(g_game, '\\');
    if (slash) *slash = 0;
}

const char* game_dir() { return g_game; }
const char* exe_path() { return g_exe; }
const char* module_path() { return g_module; }

const char* data_dir()
{
    if (!g_data[0]) {
        if (!GetEnvironmentVariableA("D2VR_DATA_DIR", g_data, MAX_PATH) || !g_data[0]) {
            char local[MAX_PATH] = "";
            if (GetEnvironmentVariableA("LOCALAPPDATA", local, MAX_PATH) && local[0])
                snprintf(g_data, MAX_PATH, "%s\\Darkness2VR", local);
            else
                snprintf(g_data, MAX_PATH, "%s\\Darkness2VR", g_game);
        }
        CreateDirectoryA(g_data, nullptr);
    }
    return g_data;
}

void set_data_dir(const char* dir)
{
    if (!dir || !dir[0]) return;
    strncpy(g_data, dir, MAX_PATH - 1);
    g_data[MAX_PATH - 1] = 0;
    size_t n = strlen(g_data);
    while (n > 0 && (g_data[n - 1] == '\\' || g_data[n - 1] == '/')) g_data[--n] = 0;
    CreateDirectoryA(g_data, nullptr);
    g_dumps[0] = 0;   // re-derived under the new root on the next call
    g_shots[0] = 0;
}

const char* dumps_dir() { return sub_dir(g_dumps, "dumps"); }
const char* shots_dir() { return sub_dir(g_shots, "shots"); }

const char* in_game_dir(char* out, const char* name)
{
    snprintf(out, MAX_PATH, "%s\\%s", g_game, name);
    return out;
}

const char* in_data_dir(char* out, const char* name)
{
    snprintf(out, MAX_PATH, "%s\\%s", data_dir(), name);
    return out;
}

} // namespace d2vr::paths
