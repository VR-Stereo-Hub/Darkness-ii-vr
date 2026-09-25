// core/util/log.h - the mod's log: darkness2_vr.log next to DarknessII.exe.
//
// Every line carries a tick ([%10lu], GetTickCount), a level letter and a
// subsystem tag, so a tester's log can be filtered to the lane under
// investigation without a rebuild. Lines also land in a fixed ring buffer that
// the crash handler (and later the F10 overlay's log tab) reads.
//
// Thread contract: write() may be called from any thread; it takes one
// critical section. flush() is batched (200 ms) except for Error lines.
// init() is loader-lock safe: kernel32 + the static CRT only, so DllMain can
// call it before anything else can fail. A run ALWAYS produces a log.
#pragma once
#include <stdarg.h>
#include <stdint.h>
#include <stddef.h>

namespace d2vr::log {

enum class Level : uint8_t { Error = 0, Warn, Info, Debug, Trace };

// One tag per subsystem. Keep in sync with kCatNames in log.cpp.
enum class Cat : uint8_t {
    core, proxy, cfg, d3d, present, hooks, canary, cmd, status, crash, input, capture, game, xr, sim,
    COUNT
};

// Opens <dir>\<base>.log, keeping ten sessions: current, .prev.log, .prev2.log
// through .prev9.log. The run that crashed survives the relaunch that reports it.
void init(const char* dir, const char* base);
void shutdown();

void write(Cat cat, Level lvl, const char* fmt, ...);
void writev(Cat cat, Level lvl, const char* fmt, va_list ap);
void flush();

// Per-category thresholds. set_all() applies one level everywhere;
// configure() parses "info" and "canary:debug,present:trace" (ini or env form).
void  set_all(Level lvl);
void  set_level(Cat cat, Level lvl);
Level level(Cat cat);
void  configure(const char* levelSpec, const char* catsSpec);
bool  parse_level(const char* s, Level* out);
bool  parse_cat(const char* s, Cat* out);
const char* level_name(Level lvl);
const char* cat_name(Cat cat);
const char* path();                 // full path of the open log, "" if none

// The ring: newest last. Copies up to max lines under the log lock.
struct RingLine { uint32_t tick; uint8_t cat, level; char text[212]; };
size_t ring_copy(RingLine* out, size_t max);

extern uint8_t g_levels[(int)Cat::COUNT];
inline bool enabled(Cat cat, Level lvl) { return (uint8_t)lvl <= g_levels[(int)cat]; }

} // namespace d2vr::log

// Each source file names its subsystem before including this header; anything
// else logs as core.
#ifndef D2VR_CAT
#define D2VR_CAT ::d2vr::log::Cat::core
#endif

// Never pay for a line you do not print: the work stays INSIDE the call, gated
// by the per-category threshold.
#define D2VR_LOG(cat, lvl, ...) \
    do { if (::d2vr::log::enabled(cat, lvl)) ::d2vr::log::write(cat, lvl, __VA_ARGS__); } while (0)
#define D2VR_LOG_ONCE(cat, lvl, ...) \
    do { static bool d2vr_once_ = false; if (!d2vr_once_) { d2vr_once_ = true; D2VR_LOG(cat, lvl, __VA_ARGS__); } } while (0)
#define D2VR_LOG_EVERY_MS(cat, lvl, ms, ...) \
    do { static unsigned long d2vr_last_ = 0; unsigned long d2vr_now_ = GetTickCount(); \
         if (d2vr_last_ == 0 || d2vr_now_ - d2vr_last_ >= (unsigned long)(ms)) { d2vr_last_ = d2vr_now_; D2VR_LOG(cat, lvl, __VA_ARGS__); } } while (0)
#define D2VR_LOG_FIRST_N(cat, lvl, n, ...) \
    do { static int d2vr_cnt_ = 0; if (d2vr_cnt_ < (n)) { d2vr_cnt_++; D2VR_LOG(cat, lvl, __VA_ARGS__); } } while (0)

#define D2VR_ERROR(...) D2VR_LOG(D2VR_CAT, ::d2vr::log::Level::Error, __VA_ARGS__)
#define D2VR_WARN(...)  D2VR_LOG(D2VR_CAT, ::d2vr::log::Level::Warn,  __VA_ARGS__)
#define D2VR_INFO(...)  D2VR_LOG(D2VR_CAT, ::d2vr::log::Level::Info,  __VA_ARGS__)
#define D2VR_DEBUG(...) D2VR_LOG(D2VR_CAT, ::d2vr::log::Level::Debug, __VA_ARGS__)
#define D2VR_TRACE(...) D2VR_LOG(D2VR_CAT, ::d2vr::log::Level::Trace, __VA_ARGS__)
