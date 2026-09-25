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

// --- R2: the engine's Lua 5.1.3 (ENGINE_NOTES s3, s8 "Lua") ---------------------
// Derived offline on 2026-09-25 from the 5.1.3 library strings and the SWIG
// wrappers' call shapes. Every entry is cdecl with a plain `ret` unless noted;
// lua_Number is FLOAT here (TValue is 8 bytes: lua_gettop shifts by 3). The
// prefixes are what a caller byte-verifies before the first call; the detour
// lengths are for the wraps the Lua lane installs (default OFF, present tick).
constexpr uintptr_t kLuaStateHolder    = 0x010DD9C4;   // *(lua_State**): the main state (written at 0x92DA25 from lua_newstate)
constexpr uintptr_t kScriptSystem      = 0x010DDAD8;   // the ScriptSystem singleton (getter 0xC84CA0); +8 also holds the state
constexpr uintptr_t kScriptResumeFn    = 0x00C51FA0;   // ScriptSystem::Resume, thiscall ret 4: the only engine caller of lua_resume (79 callers)
constexpr uint8_t   kScriptResumePrefix[] = { 0x83, 0xEC, 0x7C, 0x53, 0x55, 0x8B, 0xE9, 0x8B, 0x8C, 0x24, 0x88, 0x00, 0x00, 0x00 };
constexpr size_t    kScriptResumeDetourLen = 5;        // sub esp,7Ch ; push ebx ; push ebp
constexpr uintptr_t kLuaResume         = 0x00B72770;   // lua_resume(L, narg)
constexpr uint8_t   kLuaResumePrefix[] = { 0x56, 0x8B, 0x74, 0x24, 0x08, 0x8A, 0x46, 0x06, 0x3C, 0x01 };
constexpr size_t    kLuaResumeDetourLen = 5;           // push esi ; mov esi,[esp+8]
constexpr uintptr_t kLuaPCall          = 0x00772F10;   // lua_pcall(L, nargs, nresults, errfunc); 6 static callers, all init/debug
constexpr uint8_t   kLuaPCallPrefix[]  = { 0x8B, 0x4C, 0x24, 0x10, 0x83, 0xEC, 0x08, 0x56, 0x8B, 0x74, 0x24, 0x10 };
constexpr size_t    kLuaPCallDetourLen = 7;            // mov ecx,[esp+10h] ; sub esp,8
constexpr uintptr_t kLuaLLoadBuffer    = 0x00AF8F40;   // luaL_loadbuffer(L, buff, sz, name)
constexpr uint8_t   kLuaLLoadBufferPrefix[] = { 0x83, 0xEC, 0x08, 0x8B, 0x44, 0x24, 0x10, 0x8B, 0x54, 0x24, 0x18 };
constexpr uintptr_t kLuaLoad           = 0x00922DE0;   // lua_load; default chunkname "?" at 0xF8E714
constexpr uintptr_t kLuaGetTop         = 0x00CFFEC0;
constexpr uint8_t   kLuaGetTopPrefix[] = { 0x8B, 0x4C, 0x24, 0x04, 0x8B, 0x41, 0x08, 0x2B, 0x41, 0x0C, 0xC1, 0xF8, 0x03, 0xC3 };
constexpr uintptr_t kLuaSetTop         = 0x004D2EF0;
constexpr uint8_t   kLuaSetTopPrefix[] = { 0x8B, 0x4C, 0x24, 0x08, 0x8B, 0x44, 0x24, 0x04, 0x85, 0xC9, 0x7C, 0x37 };
constexpr uintptr_t kLuaToLString      = 0x00706A80;
constexpr uint8_t   kLuaToLStringPrefix[] = { 0x56, 0x8B, 0x74, 0x24, 0x08, 0x57, 0x8B, 0x7C, 0x24, 0x10, 0x85, 0xFF, 0x7E, 0x13 };
constexpr uintptr_t kLuaType           = 0x0093B370;
constexpr uint8_t   kLuaTypePrefix[]   = { 0x8B, 0x4C, 0x24, 0x08, 0x85, 0xC9, 0x7E, 0x16, 0x8B, 0x54, 0x24, 0x04 };
constexpr uintptr_t kLuaPushString     = 0x00AD0BB0;
constexpr uint8_t   kLuaPushStringPrefix[] = { 0x55, 0x8B, 0x6C, 0x24, 0x0C, 0x85, 0xED, 0x75, 0x10 };
constexpr uintptr_t kLuaPushNumber     = 0x00AE58D0;   // (L, float)
constexpr uint8_t   kLuaPushNumberPrefix[] = { 0x8B, 0x44, 0x24, 0x04, 0x8B, 0x48, 0x08, 0xF3, 0x0F, 0x10, 0x44, 0x24, 0x08 };
constexpr uintptr_t kLuaGetField       = 0x00710020;   // the first 24 bytes equal lua_setfield's: verify by VA, not by prefix alone
constexpr uintptr_t kLuaSetField       = 0x0080D360;
constexpr uintptr_t kLuaError          = 0x00517F30;
// lua_State layout (5.1.3, float numbers, 4 engine bytes BEFORE each state):
constexpr size_t    kLuaStateTop       = 0x08;
constexpr size_t    kLuaStateBase      = 0x0C;
constexpr size_t    kLuaStateGlobal    = 0x10;         // l_G
constexpr size_t    kLuaStateCi        = 0x14;
constexpr size_t    kLuaStateBaseCi    = 0x28;
constexpr size_t    kLuaStateNCcalls   = 0x34;         // u16
constexpr int       kLuaGlobalsIndex   = -10002;
constexpr int       kLuaRegistryIndex  = -10000;
// The SWIG module structs (tools/swig-dump.py, docs/darkness2/swig-api.md):
// { types, size, next, type_initial, cast_initial, clientdata }; size at +4.
constexpr uintptr_t kSwigModuleEngine    = 0x010B8F94;   // 127 types, 72 classes
constexpr uintptr_t kSwigModuleD2Game    = 0x010C5AB4;   // 154 types, 40 classes
constexpr uintptr_t kSwigCameraControllerBase = 0x010B93D8;   // swig_lua_class; SetBaseFovOverride wrapper 0xDBDB90 -> vtable +0xE4 (float)

} // namespace d2vr::game::pat
