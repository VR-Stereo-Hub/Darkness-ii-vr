#define D2VR_CAT ::d2vr::log::Cat::input
#include "core/input/inject.h"
#include "core/util/crash.h"
#include "core/util/log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

namespace d2vr::input {
namespace {

enum Kind { kKeyTap, kKeyDown, kKeyUp, kMouseMove, kMouseClick, kFocus, kQuit, kSleep };
struct Action { Kind kind; WORD vk; int ms; int dx, dy; bool right; };

const int kQueue = 256;
Action g_queue[kQueue];
volatile LONG g_head = 0, g_tail = 0;   // head = next to run, tail = next to write
HANDLE g_event = nullptr;
HANDLE g_thread = nullptr;
volatile LONG g_done = 0;
HWND g_window = nullptr;

struct KeyName { const char* name; WORD vk; };
const KeyName kKeys[] = {
    {"space", VK_SPACE}, {"enter", VK_RETURN}, {"return", VK_RETURN}, {"esc", VK_ESCAPE}, {"escape", VK_ESCAPE},
    {"tab", VK_TAB}, {"backspace", VK_BACK}, {"delete", VK_DELETE}, {"insert", VK_INSERT},
    {"home", VK_HOME}, {"end", VK_END}, {"pageup", VK_PRIOR}, {"pagedown", VK_NEXT},
    {"up", VK_UP}, {"down", VK_DOWN}, {"left", VK_LEFT}, {"right", VK_RIGHT},
    {"shift", VK_LSHIFT}, {"lshift", VK_LSHIFT}, {"rshift", VK_RSHIFT}, {"ctrl", VK_LCONTROL}, {"lctrl", VK_LCONTROL},
    {"rctrl", VK_RCONTROL}, {"alt", VK_LMENU}, {"lalt", VK_LMENU}, {"ralt", VK_RMENU},
    {"f1", VK_F1}, {"f2", VK_F2}, {"f3", VK_F3}, {"f4", VK_F4}, {"f5", VK_F5}, {"f6", VK_F6},
    {"f7", VK_F7}, {"f8", VK_F8}, {"f9", VK_F9}, {"f10", VK_F10}, {"f11", VK_F11}, {"f12", VK_F12},
    {"minus", VK_OEM_MINUS}, {"plus", VK_OEM_PLUS}, {"comma", VK_OEM_COMMA}, {"period", VK_OEM_PERIOD},
    {"tilde", VK_OEM_3}, {"grave", VK_OEM_3}, {"capslock", VK_CAPITAL}, {"pause", VK_PAUSE},
    {"numpad0", VK_NUMPAD0}, {"numpad1", VK_NUMPAD1}, {"numpad2", VK_NUMPAD2}, {"numpad3", VK_NUMPAD3},
    {"numpad4", VK_NUMPAD4}, {"numpad5", VK_NUMPAD5}, {"numpad6", VK_NUMPAD6}, {"numpad7", VK_NUMPAD7},
    {"numpad8", VK_NUMPAD8}, {"numpad9", VK_NUMPAD9},
};

bool parse_key(const char* s, WORD* vk)
{
    if (!s || !*s) return false;
    if (s[1] == 0) {
        char c = s[0];
        if (c >= 'a' && c <= 'z') { *vk = (WORD)(c - 'a' + 'A'); return true; }
        if (c >= 'A' && c <= 'Z') { *vk = (WORD)c; return true; }
        if (c >= '0' && c <= '9') { *vk = (WORD)c; return true; }
    }
    if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) { *vk = (WORD)strtoul(s, nullptr, 16); return *vk != 0; }
    for (const KeyName& k : kKeys) if (!_stricmp(k.name, s)) { *vk = k.vk; return true; }
    return false;
}

bool is_extended(WORD vk)
{
    switch (vk) {
    case VK_INSERT: case VK_DELETE: case VK_HOME: case VK_END: case VK_PRIOR: case VK_NEXT:
    case VK_UP: case VK_DOWN: case VK_LEFT: case VK_RIGHT: case VK_RCONTROL: case VK_RMENU:
    case VK_NUMLOCK: case VK_DIVIDE: case VK_SNAPSHOT:
        return true;
    default: return false;
    }
}

void send_key(WORD vk, bool up)
{
    INPUT in = {};
    in.type = INPUT_KEYBOARD;
    in.ki.wVk = vk;
    in.ki.wScan = (WORD)MapVirtualKeyA(vk, MAPVK_VK_TO_VSC);
    in.ki.dwFlags = KEYEVENTF_SCANCODE | (up ? KEYEVENTF_KEYUP : 0) | (is_extended(vk) ? KEYEVENTF_EXTENDEDKEY : 0);
    if (!SendInput(1, &in, sizeof(in)))
        D2VR_LOG_EVERY_MS(D2VR_CAT, d2vr::log::Level::Warn, 2000, "SendInput(key) failed (err %lu)", GetLastError());
}

void send_mouse(DWORD flags, int dx, int dy)
{
    INPUT in = {};
    in.type = INPUT_MOUSE;
    in.mi.dx = dx; in.mi.dy = dy; in.mi.dwFlags = flags;
    if (!SendInput(1, &in, sizeof(in)))
        D2VR_LOG_EVERY_MS(D2VR_CAT, d2vr::log::Level::Warn, 2000, "SendInput(mouse) failed (err %lu)", GetLastError());
}

struct FindCtx { DWORD pid; HWND best; int bestArea; };
BOOL CALLBACK enum_windows(HWND h, LPARAM lp)
{
    FindCtx* c = (FindCtx*)lp;
    DWORD pid = 0; GetWindowThreadProcessId(h, &pid);
    if (pid != c->pid || !IsWindowVisible(h) || GetWindow(h, GW_OWNER)) return TRUE;
    RECT r; GetWindowRect(h, &r);
    int area = (r.right - r.left) * (r.bottom - r.top);
    if (area > c->bestArea) { c->bestArea = area; c->best = h; }
    return TRUE;
}

void log_window(HWND h)
{
    if (!h) { D2VR_WARN("window: the process has no visible top-level window yet"); return; }
    char cls[64] = "", title[128] = ""; RECT r = {};
    GetClassNameA(h, cls, sizeof(cls)); GetWindowTextA(h, title, sizeof(title)); GetWindowRect(h, &r);
    D2VR_INFO("window: %p class '%s' title '%s' rect %ld,%ld-%ld,%ld (%ldx%ld) style 0x%08lx exstyle 0x%08lx foreground=%d",
              (void*)h, cls, title, r.left, r.top, r.right, r.bottom, r.right - r.left, r.bottom - r.top,
              (unsigned long)GetWindowLongA(h, GWL_STYLE), (unsigned long)GetWindowLongA(h, GWL_EXSTYLE),
              GetForegroundWindow() == h);
}

// SetForegroundWindow from a background process is refused by Windows unless
// the caller has "input rights"; the standard workaround attaches to the
// foreground window's input thread first, and a synthetic Alt tap unlocks it
// as a fallback. The result is measured (GetForegroundWindow) and logged.
void focus_window(HWND h)
{
    if (!h) { D2VR_WARN("focus: no game window"); return; }
    HWND fg = GetForegroundWindow();
    if (fg == h) { D2VR_INFO("focus: already foreground (%p)", (void*)h); return; }
    DWORD fgThread = fg ? GetWindowThreadProcessId(fg, nullptr) : 0;
    DWORD me = GetCurrentThreadId();
    if (fgThread && fgThread != me) AttachThreadInput(me, fgThread, TRUE);
    if (IsIconic(h)) ShowWindow(h, SW_RESTORE);
    BOOL ok = SetForegroundWindow(h);
    BringWindowToTop(h);
    if (fgThread && fgThread != me) AttachThreadInput(me, fgThread, FALSE);
    Sleep(150);
    if (GetForegroundWindow() != h) {
        send_key(VK_LMENU, false); send_key(VK_LMENU, true);
        SetForegroundWindow(h);
        Sleep(150);
    }
    HWND now = GetForegroundWindow();
    D2VR_INFO("focus: SetForegroundWindow(%p) returned %d; foreground is now %p (%s)", (void*)h, (int)ok, (void*)now,
              now == h ? "the game: input will land" : "NOT the game: input would land elsewhere");
}

void run(const Action& a)
{
    switch (a.kind) {
    case kKeyTap:   send_key(a.vk, false); Sleep(a.ms > 0 ? a.ms : 60); send_key(a.vk, true); break;
    case kKeyDown:  send_key(a.vk, false); break;
    case kKeyUp:    send_key(a.vk, true); break;
    case kSleep:    Sleep(a.ms); break;
    case kMouseMove: send_mouse(MOUSEEVENTF_MOVE, a.dx, a.dy); break;
    case kMouseClick:
        send_mouse(a.right ? MOUSEEVENTF_RIGHTDOWN : MOUSEEVENTF_LEFTDOWN, 0, 0);
        Sleep(a.ms > 0 ? a.ms : 80);
        send_mouse(a.right ? MOUSEEVENTF_RIGHTUP : MOUSEEVENTF_LEFTUP, 0, 0);
        break;
    case kFocus:    focus_window(game_window()); break;
    case kQuit: {
        HWND h = game_window();
        d2vr::crash::note_teardown("quit requested through the seam");
        if (a.right) {   // force
            D2VR_WARN("quit force: ExitProcess(0) now");
            d2vr::log::flush();
            ExitProcess(0);
        }
        D2VR_INFO("quit: posting WM_CLOSE to %p", (void*)h);
        if (h) PostMessageA(h, WM_CLOSE, 0, 0);
        else D2VR_WARN("quit: no game window to close; use `quit force`");
        break;
    }
    }
    InterlockedIncrement(&g_done);
}

DWORD WINAPI worker(void*)
{
    d2vr::crash::register_thread("input", GetCurrentThreadId());
    for (;;) {
        WaitForSingleObject(g_event, INFINITE);
        while (g_head != g_tail) {
            Action a = g_queue[g_head % kQueue];
            InterlockedIncrement(&g_head);
            run(a);
        }
    }
}

bool enqueue(const Action& a)
{
    if (!g_event) {
        g_event = CreateEventA(nullptr, FALSE, FALSE, nullptr);
        g_thread = CreateThread(nullptr, 0, worker, nullptr, 0, nullptr);
        if (!g_thread) { D2VR_ERROR("input: no worker thread (err %lu) - injection unavailable", GetLastError()); return false; }
    }
    if (g_tail - g_head >= kQueue) { D2VR_WARN("input: queue full (%d) - dropping", kQueue); return false; }
    g_queue[g_tail % kQueue] = a;
    InterlockedIncrement(&g_tail);
    SetEvent(g_event);
    return true;
}

} // namespace

HWND game_window()
{
    if (g_window && IsWindow(g_window)) return g_window;
    FindCtx c = { GetCurrentProcessId(), nullptr, 0 };
    EnumWindows(enum_windows, (LPARAM)&c);
    g_window = c.best;
    return g_window;
}

unsigned long actions_done() { return (unsigned long)g_done; }

bool command(const char* cmd, const char* args)
{
    char a[64] = "", b[64] = "", c[64] = "";
    sscanf(args, "%63s %63s %63s", a, b, c);
    if (!strcmp(cmd, "key")) {
        WORD vk;
        if (!parse_key(a, &vk)) { D2VR_WARN("key: unknown key '%s' (a letter, a digit, 0xVK or a name: space enter esc tab up down left right f1..f12 shift ctrl alt ...)", a); return true; }
        Action act = {};
        act.vk = vk; act.ms = c[0] ? atoi(c) : (b[0] && isdigit((unsigned char)b[0]) ? atoi(b) : 0);
        if (!strcmp(b, "down")) act.kind = kKeyDown;
        else if (!strcmp(b, "up")) act.kind = kKeyUp;
        else act.kind = kKeyTap;
        D2VR_INFO("key: %s vk=0x%02x %s%s%d ms", a, vk, act.kind == kKeyDown ? "down" : act.kind == kKeyUp ? "up" : "tap",
                  act.kind == kKeyTap ? " " : " ", act.ms ? act.ms : 60);
        enqueue(act);
        return true;
    }
    if (!strcmp(cmd, "type")) {
        int n = 0;
        for (const char* p = args; *p; p++) {
            char s[2] = { *p, 0 }; WORD vk;
            if (*p == ' ') vk = VK_SPACE; else if (!parse_key(s, &vk)) continue;
            Action act = {}; act.kind = kKeyTap; act.vk = vk; act.ms = 40; enqueue(act);
            Action gap = {}; gap.kind = kSleep; gap.ms = 40; enqueue(gap);
            n++;
        }
        D2VR_INFO("type: %d key(s) queued", n);
        return true;
    }
    if (!strcmp(cmd, "mouse")) {
        Action act = {};
        if (!strcmp(a, "move")) { act.kind = kMouseMove; act.dx = atoi(b); act.dy = atoi(c); D2VR_INFO("mouse: move %d %d", act.dx, act.dy); enqueue(act); return true; }
        if (!strcmp(a, "click")) { act.kind = kMouseClick; act.right = !strcmp(b, "right"); act.ms = c[0] ? atoi(c) : 80; D2VR_INFO("mouse: click %s", act.right ? "right" : "left"); enqueue(act); return true; }
        D2VR_WARN("mouse: usage - mouse move <dx> <dy> | mouse click left|right [ms]");
        return true;
    }
    if (!strcmp(cmd, "focus")) { Action act = {}; act.kind = kFocus; enqueue(act); return true; }
    if (!strcmp(cmd, "window")) { log_window(game_window()); return true; }
    if (!strcmp(cmd, "quit")) { Action act = {}; act.kind = kQuit; act.right = !strcmp(a, "force"); enqueue(act); return true; }
    return false;
}

} // namespace d2vr::input
