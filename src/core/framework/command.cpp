// core/framework/command.cpp - see command.h. Shape adopted from the Dishonored VR mod.
#define D2VR_CAT ::d2vr::log::Cat::cmd
#include "core/framework/command.h"
#include "core/framework/status.h"
#include "core/framework/shot.h"
#include "core/input/inject.h"
#include "core/util/crash.h"
#include "core/util/log.h"
#include "core/util/paths.h"
#include "core/util/diag.h"

#include <windows.h>
#include <stdio.h>
#include <string.h>

namespace d2vr::command {
namespace {

GameHandler g_game = nullptr;
double      g_lastPollMs = 0.0;
uint32_t    g_seq = 0;
uint32_t    g_lines = 0;
uint32_t    g_unknown = 0;
FILETIME    g_startTime = {};
FILETIME    g_lastWrite = {};
bool        g_haveStart = false;

bool read_and_truncate(char* buf, size_t cap)
{
    char path[MAX_PATH];
    d2vr::paths::in_data_dir(path, "command.txt");
    HANDLE h = CreateFileA(path, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
                           nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return false;
    DWORD got = 0;
    BOOL ok = ReadFile(h, buf, (DWORD)cap - 1, &got, nullptr);
    buf[ok ? got : 0] = 0;
    // consume: a command must never re-apply on the next poll or the next boot
    SetFilePointer(h, 0, nullptr, FILE_BEGIN);
    SetEndOfFile(h);
    CloseHandle(h);
    return ok && got > 0;
}

void write_ack(const char* text)
{
    char path[MAX_PATH];
    d2vr::paths::in_data_dir(path, "ack.txt");
    FILE* f = fopen(path, "w");
    if (!f) return;
    fprintf(f, "seq %lu\n%s", (unsigned long)g_seq, text);
    fclose(f);
}

} // namespace

void set_game_handler(GameHandler h) { g_game = h; }
uint32_t sequence() { return g_seq; }
uint32_t lines() { return g_lines; }

bool core_command(const char* cmd, const char* args)
{
    if (!strcmp(cmd, "status")) {
        d2vr::status::write_now();
        D2VR_INFO("status written to %s", d2vr::status::path());
        return true;
    }
    if (!strcmp(cmd, "log")) {
        char a[32] = "", b[32] = "", c[32] = "";
        sscanf(args, "%31s %31s %31s", a, b, c);
        d2vr::log::Level lvl; d2vr::log::Cat cat;
        if (!strcmp(a, "flush")) { d2vr::log::flush(); return true; }
        if (!strcmp(a, "level") && d2vr::log::parse_level(b, &lvl)) {
            d2vr::log::set_all(lvl);
            D2VR_INFO("log level -> %s (all categories)", d2vr::log::level_name(lvl));
            return true;
        }
        if (!strcmp(a, "cat") && d2vr::log::parse_cat(b, &cat) && d2vr::log::parse_level(c, &lvl)) {
            d2vr::log::set_level(cat, lvl);
            D2VR_INFO("log category %s -> %s", d2vr::log::cat_name(cat), d2vr::log::level_name(lvl));
            return true;
        }
        D2VR_WARN("log: usage - log level <lvl> | log cat <cat> <lvl> | log flush");
        return true;
    }
    if (!strcmp(cmd, "cmd")) {
        D2VR_INFO("cmd: seq=%lu lines=%lu unknown=%lu data=%s log=%s",
                  (unsigned long)g_seq, (unsigned long)g_lines, (unsigned long)g_unknown,
                  d2vr::paths::data_dir(), d2vr::log::path());
        return true;
    }
    if (!strcmp(cmd, "skip")) {
        D2VR_INFO("skip: D2VR_SKIP says %s is %s", args, d2vr::diag::skip(args) ? "SKIPPED" : "installed");
        return true;
    }
    if (!strcmp(cmd, "mark")) {
        D2VR_WARN("mark: %s", args[0] ? args : "(no text)");
        d2vr::log::flush();
        return true;
    }
    if (!strcmp(cmd, "crash")) {
        if (!strcmp(args, "test")) { d2vr::crash::self_test(); return true; }
        D2VR_WARN("crash: usage - crash test (a deliberate fault; the game will exit)");
        return true;
    }
    if (!strcmp(cmd, "shot")) {
        d2vr::shot::request(args[0] ? args : "shot");
        return true;
    }
    if (!strcmp(cmd, "key") || !strcmp(cmd, "mouse") || !strcmp(cmd, "type") || !strcmp(cmd, "focus") ||
        !strcmp(cmd, "quit") || !strcmp(cmd, "window"))
        return d2vr::input::command(cmd, args);
    return false;
}

void dispatch_line(const char* line)
{
    char buf[512];
    strncpy(buf, line, sizeof(buf) - 1); buf[sizeof(buf) - 1] = 0;
    char* p = buf;
    while (*p == ' ' || *p == '\t') p++;
    char* end = p + strlen(p);
    while (end > p && (end[-1] == ' ' || end[-1] == '\t' || end[-1] == '\r' || end[-1] == '\n')) *--end = 0;
    if (!*p || *p == '#') return;
    char* args = p;
    while (*args && *args != ' ' && *args != '\t') args++;
    if (*args) { *args++ = 0; while (*args == ' ' || *args == '\t') args++; }
    for (char* q = p; *q; q++) if (*q >= 'A' && *q <= 'Z') *q += 32;
    g_lines++;
    D2VR_INFO("> %s %s", p, args);
    if (g_game && g_game(p, args)) return;
    if (core_command(p, args)) return;
    g_unknown++;
    D2VR_WARN("cmd: unknown command '%s' (see core/framework/command.h and game/darkness2/commands.cpp)", p);
}

void poll(double nowMs)
{
    if (nowMs - g_lastPollMs < 1000.0) return;
    g_lastPollMs = nowMs;
    if (!g_haveStart) { GetSystemTimeAsFileTime(&g_startTime); g_haveStart = true; }

    // EVERY REFUSED GUARD SAYS WHY. Rate-limited, and never on the path that
    // succeeds, so a healthy seam still costs nothing.
    char path[MAX_PATH];
    d2vr::paths::in_data_dir(path, "command.txt");
    WIN32_FILE_ATTRIBUTE_DATA fa;
    if (!GetFileAttributesExA(path, GetFileExInfoStandard, &fa)) {
        const DWORD err = GetLastError();
        if (err != ERROR_FILE_NOT_FOUND)
            D2VR_LOG_EVERY_MS(D2VR_CAT, ::d2vr::log::Level::Warn, 10000,
                "cmd: GetFileAttributesEx failed on %s (err %lu) - the seam is DEAF; no command can arrive",
                path, (unsigned long)err);
        return;
    }
    if (fa.nFileSizeLow == 0 && fa.nFileSizeHigh == 0) return;   // idle: normal
    if (CompareFileTime(&fa.ftLastWriteTime, &g_lastWrite) == 0) {
        D2VR_LOG_EVERY_MS(D2VR_CAT, ::d2vr::log::Level::Warn, 10000,
            "cmd: %s holds %lu byte(s) but its write time has not changed since the last poll - "
            "REFUSING to re-read it. Rewrite the file to bump its timestamp.",
            path, (unsigned long)fa.nFileSizeLow);
        return;
    }
    g_lastWrite = fa.ftLastWriteTime;
    if (CompareFileTime(&fa.ftLastWriteTime, &g_startTime) < 0) {
        D2VR_WARN("cmd: command.txt is older than this process - ignoring a stale file (cleared)");
        char dummy[8];
        read_and_truncate(dummy, sizeof(dummy));
        return;
    }
    static char text[8192];
    if (!read_and_truncate(text, sizeof(text))) {
        D2VR_LOG_EVERY_MS(D2VR_CAT, ::d2vr::log::Level::Warn, 10000,
            "cmd: %s reported %lu byte(s) but could not be read and cleared (err %lu) - the command is LOST",
            path, (unsigned long)fa.nFileSizeLow, (unsigned long)GetLastError());
        return;
    }
    g_seq++;
    char* ctx = nullptr;
    for (char* line = strtok_s(text, "\n", &ctx); line; line = strtok_s(nullptr, "\n", &ctx))
        dispatch_line(line);
    write_ack(text);
}

} // namespace d2vr::command
