// core/input/inject.h - keyboard and mouse injection from INSIDE the game
// process, so the harness can play the flat game: menus, Continue, movement,
// Esc, the checkpoint reload. Actions queue to a worker thread (a held key must
// not block the present thread) and go out through SendInput with hardware
// scancodes, which is what a DirectInput reader (the game loads DINPUT8.DLL)
// and RawInput both see as a real keyboard.
//
// Seam words (core/framework/command.cpp forwards them here):
//   key <name> [tap|down|up] [ms]     tap holds for ms (default 60)
//   type <text>                       one tap per character (letters, digits, space)
//   mouse move <dx> <dy>              relative, in mickeys
//   mouse click left|right [ms]
//   focus                             bring the game window to the front (logs whether it worked)
//   window                            log the game window's handle, class, title, rect, style
//   quit [force]                      WM_CLOSE to the game window; force = ExitProcess(0)
#pragma once
#include <windows.h>

namespace d2vr::input {
bool command(const char* cmd, const char* args);   // true when the word was ours
HWND game_window();                                 // the process's main top-level window, or null
unsigned long actions_done();
} // namespace d2vr::input
