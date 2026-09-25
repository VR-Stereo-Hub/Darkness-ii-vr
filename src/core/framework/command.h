// core/framework/command.h - the command seam: a text file the harness (or the
// player) writes and the mod polls, so the running game can be driven and
// inspected without a rebuild or a headset.
//
//   <data_dir>\command.txt   one command per line; consumed (truncated) once read
//   <data_dir>\ack.txt       "seq N" + the lines applied, written after each batch
//
// The poll runs on the PRESENT thread at 1 Hz. Game commands
// (game/darkness2/commands.cpp) are tried first so the game layer can shadow a
// core word; then the core vocabulary:
//   status                     write status.json now
//   log level <lvl>            error|warn|info|debug|trace for every category
//   log cat <cat> <lvl>        one category
//   log flush
//   cmd                        poll counters and paths
//   skip <subsystem>           what D2VR_SKIP disabled
//   mark <text>                stamp a felt event in the log (Warn)
//   crash test                 the deliberate fault: crash file + minidump
//   shot [tag]                 backbuffer -> <data_dir>\shots\<tag>_<n>.bmp
//   key|mouse|type|focus ...   input injection (core/input/inject.h)
//   quit [force]               WM_CLOSE to the game window (force: ExitProcess)
// Everything else: one "cmd: unknown" line.
#pragma once
#include <stdint.h>

namespace d2vr::command {
typedef bool (*GameHandler)(const char* cmd, const char* args);
void set_game_handler(GameHandler h);
void poll(double nowMs);              // call every frame; checks the file at 1 Hz
void dispatch_line(const char* line); // one line, game handler first
bool core_command(const char* cmd, const char* args);
uint32_t sequence();                  // batches applied so far
uint32_t lines();
} // namespace d2vr::command
