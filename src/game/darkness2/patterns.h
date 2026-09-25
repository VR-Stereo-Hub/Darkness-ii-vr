// game/darkness2/patterns.h - EVERY fixed address and engine layout number the
// mod relies on, in one place. Its companion is docs/darkness2/ENGINE_NOTES.md
// section 8, which records HOW each number was derived (string xref, caller
// census, live measurement), on which build, and on what date.
//
// DarknessII.exe has no ASLR (ImageBase 0x400000), so these are absolute VAs.
// Every code hook byte-verifies its site against the prefix recorded here and
// refuses on a mismatch (core/hooks/detour.h): that refusal is the mod's build
// check. The fingerprint below gates every code hook as well.
//
// NEVER copy a number from another game. Nothing here came from the siblings.
#pragma once
#include <stdint.h>
#include <stddef.h>

static_assert(sizeof(void*) == 4, "DarknessII.exe is 32-bit; the mod module must be too");

namespace d2vr::game::pat {

// --- the host build fingerprint (ENGINE_NOTES s1, verified 2026-09-25) --------
constexpr uintptr_t kExeBase          = 0x00400000;
constexpr uint32_t  kExeTimeDateStamp = 0x4F68A875;   // 2012-03-20 15:55:33 UTC
constexpr uint32_t  kExeSizeOfImage   = 0x00DEC000;
constexpr uint64_t  kExeFileSize      = 14291512;
constexpr uintptr_t kTextBegin        = 0x00401000;   // .text
constexpr uintptr_t kTextEnd          = 0x00EEA730;

// --- R0: the D3D9 module init (ENGINE_NOTES s8 "kDx9InitFn") ------------------
// LoadLibraryA("D3D9.DLL") through the exe's wrapper, the registry DirectX
// version check, GetProcAddress("Direct3DCreate9Ex"), then Direct3DCreate9Ex
// (0x20, &out) when the Graphics.EnableDirect3D9Ex byte is set, else
// Direct3DCreate9(0x20). One static caller (0xC27176). Runs once at startup,
// which makes it the COLD canary site for R1.
constexpr uintptr_t kDx9InitFn        = 0x00CF2C30;
constexpr uint8_t   kDx9InitPrefix[]  = { 0x83, 0xEC, 0x1C, 0x56, 0x8B, 0xF1, 0x83, 0x7E, 0x04, 0x00 };
constexpr size_t    kDx9InitDetourLen = 6;            // sub esp,1Ch ; push esi ; mov esi,ecx
constexpr uintptr_t kDx9InitCaller    = 0x00C27176;
// The exe's LoadLibraryA wrapper (SEH frame + the import call): R0's second
// fallback hook site if the app-dir proxy ever stops being chosen.
constexpr uintptr_t kLoadLibraryWrapper = 0x00A556E0;
// Graphics.EnableDirect3D9Ex, a byte; default 1 (set at 0xEA7AC5). Read-only.
constexpr uintptr_t kD3D9ExEnableFlag = 0x010E3BDC;

// --- R1: the per-tick message pump (ENGINE_NOTES s8 "kMsgPumpFn") --------------
// PeekMessageA / TranslateAcceleratorA / TranslateMessage / DispatchMessageA
// loop, called once per game tick through a vtable (0 static callers). The
// TICK canary site. Its `call 0x924B80` at the end is the CALL-SITE canary.
constexpr uintptr_t kMsgPumpFn        = 0x00B2E5E0;
constexpr uint8_t   kMsgPumpPrefix[]  = { 0x83, 0xEC, 0x1C, 0x53, 0x55, 0x56, 0x57, 0x8B, 0xF9 };
constexpr size_t    kMsgPumpDetourLen = 5;            // sub esp,1Ch ; push ebx ; push ebp
constexpr uintptr_t kPumpCallSite     = 0x00B2E6E9;   // E8 92 64 DF FF
constexpr uintptr_t kPumpCallTarget   = 0x00924B80;

// --- R1: the HOT per-frame render site (ENGINE_NOTES s8 "kPresentWrapperFn") --
// Derived at runtime on 2026-09-25 (launch 1): IDirect3DDevice9Ex::PresentEx
// returns into the exe at 0x920F2E; the enclosing function starts at 0x920EE0
// (int3 padding before it). It is the engine's present wrapper: it calls the
// device vtable slot 0x1E4/4 = 121 (PresentEx) with (0,0,0,0, flags) once per
// frame, so its hits/s must equal the present rate. The HOT canary site.
// The EndScene wrapper at 0x654FA0 (return address 0x654FB1) is the alternative.
constexpr uintptr_t kPresentWrapperFn   = 0x00920EE0;
constexpr uint8_t   kPresentWrapperPrefix[] = { 0x55, 0x56, 0x57, 0x8B, 0xF9, 0xE8, 0xB6, 0xB1, 0xB6, 0xFF };
constexpr size_t    kPresentWrapperDetourLen = 5;    // push ebp ; push esi ; push edi ; mov edi,ecx
constexpr uintptr_t kPresentExReturn    = 0x00920F2E;
constexpr uintptr_t kEndSceneWrapperFn  = 0x00654FA0;
constexpr uintptr_t kEndSceneReturn     = 0x00654FB1;
constexpr uintptr_t kHotSite            = kPresentWrapperFn;

} // namespace d2vr::game::pat
