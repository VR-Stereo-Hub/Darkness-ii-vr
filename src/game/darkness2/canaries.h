// game/darkness2/canaries.h - R1's instrument (docs/ROADMAP.md).
//
// Four in-memory code hooks that do nothing but count, so the question "does
// CEG tolerate in-memory hooks, where, and when" gets a measurement:
//   cold      a 6-byte detour at the D3D9 module init (runs once at startup;
//             tests whether cold code is re-checked periodically)
//   tick      a 5-byte detour at the per-tick message pump (game thread)
//   callsite  the `call` inside the pump rewritten to a counting stub (the
//             re-entry shape)
//   hot       a detour at the engine's frame-end function (derived at runtime
//             from the Present return address; refuses until patterns.h has it)
// All default OFF ([Canary] in the ini); `canary <name> on|off` flips them live;
// `canary status` prints them. Once a second each live canary logs its hits/s
// (a counter that reads 0 falsifies "this site runs every tick" on the line)
// and re-reads its bytes: intact, REVERTED to the original, or something else.
#pragma once
#include <stdint.h>

namespace d2vr::game::canaries {
void init_from_config();                 // installs the ones the ini turns on (present thread)
bool command(const char* cmd, const char* args);   // the `canary` word
void tick(double nowMs);                 // present thread: the 1 Hz lines and re-reads
struct State { const char* name; bool wanted; bool on; uintptr_t at; unsigned long hits; double hz; int reverts; int lastCheck; };
int  count();
const State& state(int i);
} // namespace d2vr::game::canaries
