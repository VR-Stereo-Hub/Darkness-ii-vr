// core/util/mem.h - safe reads of game memory. Every pointer the mod pulls out
// of an engine object goes through these before it is dereferenced. The exe is
// LARGE_ADDRESS_AWARE, so a 4 GB scan range applies: nothing here assumes the
// top bit is clear.
#pragma once
#include <stdint.h>
#include <stddef.h>

namespace d2vr::mem {
// True when every page of [p, p+n) is committed and readable. Rejects p + n
// wrapping past zero (a sentinel of 0xFFFFFFFF once read as "readable" on a
// sibling mod and the next dereference faulted).
bool range_readable(const void* p, size_t n);
// One aligned dword, or false without touching the page.
bool safe_read32(uintptr_t p, uint32_t* out);
} // namespace d2vr::mem
