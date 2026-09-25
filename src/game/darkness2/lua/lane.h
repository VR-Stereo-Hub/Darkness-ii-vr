// game/darkness2/lua/lane.h - the Lua lane: R2's in-game half (VR-233).
//
// The engine runs every script callback as a coroutine through
// ScriptSystem::Resume -> lua_resume on the game thread, and keeps its one main
// lua_State in a static holder (ENGINE_NOTES s3, patterns.h). The lane lets the
// mod execute ONE chunk of its own text on that main state at the engine's own
// script entry, so engine objects can be reached by NAME the way the shipped
// scripts reach them (`camCtrl:SetBaseFovOverride(f)`).
//
// Three byte-verified prologue wraps, installed from the present tick, default
// OFF ([Lua] Enabled), re-read every [Canary] LogEverySeconds like the canaries:
//   ScriptSystem::Resume  the live entry: records the thread, runs the queued
//                         chunk on the MAIN state when it is idle (ci == base_ci,
//                         nCcalls == 0, status 0)
//   lua_resume            the cross-check: the resumed coroutine's global_State
//                         must equal the holder's; the lane runs nothing until
//                         that has matched, and recently
//   lua_pcall             the negative control: the engine calls it at VM
//                         creation only, so its engine-caller count must read 0
//                         during play (the lane's own pcalls are filed apart by
//                         return address)
// Seam words: lua status | on | off | run <text> | fov <deg> | swigcheck.
#pragma once
#include <stdint.h>

namespace d2vr::status { class Writer; }

namespace d2vr::game::lua {
void init_from_config();                 // present thread, first present
void tick(double nowMs);                 // present thread: the cadence line and re-reads
bool command(const char* cmd, const char* args);   // the `lua` word
void status(d2vr::status::Writer& w);    // the status.json "lua" object
bool on();                               // the three wraps are live
// Queue one chunk (present thread). False when the lane is off, poisoned or busy.
// `tag` names it in the log. The chunk should `return` a string: it is logged.
bool run(const char* text, const char* tag);
// The FOV chunk (the shipped SetFov.lua's object path): 0 = reset to the game's own.
bool fov(float deg);
// Chunk accounting for instruments that wait on a write: the count of chunks
// that finished (ok or not) and the last result text.
uint32_t chunks_done();
uint32_t chunks_failed();
const char* last_result();
} // namespace d2vr::game::lua
