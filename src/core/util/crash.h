// core/util/crash.h - crash fingerprinting and minidumps.
//
// A vectored handler names the faulting module, address, thread and the first
// return addresses on the stack for every fatal exception code, into the log
// AND into darkness2_vr_crash.txt (written with WriteFile, no CRT, because the
// heap may be the thing that broke). Every run that faults gets a header line
// naming the build, pid and context, so a fingerprint can be attributed.
//
// The minidump goes to <data_dir>\dumps from EITHER handler, once per run. The
// unhandled filter is not the only path: the exe installs its own filters and a
// vectored handler of its own (it imports SetUnhandledExceptionFilter from six
// sites and AddVectoredExceptionHandler from one), so the VEH takes the dump too,
// gated on the instruction pointer resolving to no loaded module, a condition no
// recoverable exception can satisfy. The `crash test` seam word faults at a
// sentinel address the VEH recognises, so the dump path can be proven on demand.
//
// CEG note: a silent exit with no record here is itself the measurement R1
// looks for; the log's last lines and the canary state carry it.
#pragma once
#include <windows.h>
#include <stdint.h>

namespace d2vr::crash {
void install();                                    // idempotent; first Direct3DCreate9(Ex), not DllMain
void rearm();                                      // the Steam overlay and the game displace filters; call from Present
void register_thread(const char* name, DWORD tid); // "present", "input": named in the fingerprint
void set_context(const char* text);                // named in the crash file's run header
void note_teardown(const char* why);               // game announced exit: faults after this get one line, no dump
bool teardown_seen();
// A fault THIS thread raises inside a guarded probe of our own (a __try that
// expects to fault) is not a crash: between probe_begin and probe_end the
// fingerprinter ignores this thread's faults entirely.
void probe_begin();
void probe_end();
// The deliberate fault behind `crash test`: an access violation at a sentinel
// address the handler dumps on. Never returns normally.
void self_test();
// The sentinel the VEH recognises as the self-test.
constexpr uintptr_t kSelfTestAddress = 0xD2000000u;
} // namespace d2vr::crash
