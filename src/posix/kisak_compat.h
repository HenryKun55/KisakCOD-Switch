// MSVC compatibility shims for POSIX/Switch builds.
//
// Force-included before every .cpp/.h via the compiler's -include flag (see
// scripts/posix/CMakeLists.txt). Lets upstream code that uses MSVC keywords
// and types (__cdecl, __declspec, __int16, etc.) compile on clang/gcc
// without invasive changes to the source.
//
// Kept minimal: only what actually appears in upstream KisakCOD code. When a
// new shim is needed, add it here instead of ifdef'ing the use site.

#pragma once

#if !defined(_MSC_VER)

// === Calling conventions ====================================================
// MSVC uses __cdecl/__stdcall/__fastcall to control argument-passing
// conventions. On non-Windows ARM64 and x86_64, AAPCS/SysV ABI is imposed
// by the compiler regardless of these keywords — they become no-ops.
#ifndef __cdecl
#define __cdecl
#endif
#ifndef __stdcall
#define __stdcall
#endif
#ifndef __fastcall
#define __fastcall
#endif
#ifndef __thiscall
#define __thiscall
#endif

// === Forced inline ==========================================================
#ifndef __forceinline
#define __forceinline inline __attribute__((always_inline))
#endif

// === __declspec(...) ========================================================
// Used in MSVC for DLL visibility, alignment, thread-local storage, etc. In
// KisakCOD upstream it shows up as __declspec(noreturn) and
// __declspec(align). A generic no-op covers current usage; refine if any
// site breaks.
#ifndef __declspec
#define __declspec(x)
#endif

// === Standard headers upstream assumes without including explicitly ========
// q_shared.h uses INT_MIN/INT_MAX in DvarLimits without including <limits.h>.
// The MSVC build picks those up transitively from some other MS CRT header;
// force availability here.
#include <climits>

// === MSVC underscore-prefixed CRT functions =================================
// MSVC CRT prefixes various functions with `_` (_vsnprintf, _snprintf,
// _stricmp, etc.) to avoid clashing with user namespaces. POSIX/glibc/newlib
// use the unprefixed names. Map the ones upstream uses.
#define _vsnprintf vsnprintf
#define _snprintf  snprintf
#define _stricmp   strcasecmp
#define _strnicmp  strncasecmp
// MSVC "secure" CRT variants: signature is (dst, dstSize, _TRUNCATE, fmt, va).
// On POSIX vsnprintf already truncates safely; _TRUNCATE is a no-op sentinel.
#ifndef _TRUNCATE
#define _TRUNCATE ((size_t)-1)
#endif
#define _vsnprintf_s(dst, dstSize, count, fmt, va) vsnprintf((dst), (dstSize), (fmt), (va))
#define _snprintf_s(dst, dstSize, count, ...)      snprintf((dst), (dstSize), __VA_ARGS__)
// MSVC: sprintf_s(buf, sizeOfBuf, fmt, ...) — POSIX equivalent is snprintf
// with the same destination size; truncation behavior matches our needs.
#define sprintf_s(dst, dstSize, ...)               snprintf((dst), (dstSize), __VA_ARGS__)
// MSVC: sscanf_s(buf, fmt, ...) — POSIX sscanf has the same contract for
// the format specifiers KisakCOD uses (no %s/%c width pairs). Aliasing is
// safe.
#define sscanf_s(src, ...)                         sscanf((src), __VA_ARGS__)
// Win32 BYTE/WORD/DWORD typedefs — referenced by hex-rays decompiled bit-field
// accessors. POSIX/clang has no <windows.h> so define them as plain integer
// aliases here.
#ifndef BYTE
typedef unsigned char  BYTE;
#endif
#ifndef WORD
typedef unsigned short WORD;
#endif
// Win32 HIWORD / LOWORD / HIBYTE / LOBYTE macros.
#ifndef HIWORD
#define HIWORD(x) (static_cast<WORD>((static_cast<DWORD>(x) >> 16) & 0xFFFF))
#endif
#ifndef LOWORD
#define LOWORD(x) (static_cast<WORD>(static_cast<DWORD>(x) & 0xFFFF))
#endif
#ifndef HIBYTE
#define HIBYTE(x) (static_cast<BYTE>((static_cast<WORD>(x) >> 8) & 0xFF))
#endif
#ifndef LOBYTE
#define LOBYTE(x) (static_cast<BYTE>(static_cast<WORD>(x) & 0xFF))
#endif
// DWORD typedef defined later in this header as `unsigned long`.
// _ctime64: MSVC's 64-bit time formatter. On POSIX time_t is already 64-bit;
// the upstream caller pairs the result with free(), so return a strdup'd
// copy instead of ctime's static buffer.
#include <ctime>
#include <cstring>
#include <cstdlib>
static inline char *_ctime64(const long long *t)
{
    if (!t) return nullptr;
    ::time_t tt = (::time_t)(*t);
    char *s = std::ctime(&tt);
    return s ? strdup(s) : nullptr;
}
// __debugbreak: MSVC intrinsic that triggers a debugger breakpoint.
// On clang/gcc the equivalent is __builtin_trap (or __builtin_debugtrap
// on clang specifically, but trap works everywhere as a fallback).
#define __debugbreak() __builtin_trap()

// Win32 critical-section API as no-op shims. The CRITICAL_SECTION type
// itself is already defined as a small struct above; callers just need
// these four entry points to exist. The single-threaded port has no real
// contention; once the engine actually spawns worker threads, these get
// replaced by pthread_mutex-backed Sys_EnterCriticalSection variants
// already in posix_stubs.cpp.
#ifndef _WIN32
struct _RTL_CRITICAL_SECTION;
static inline void EnterCriticalSection(_RTL_CRITICAL_SECTION * /*cs*/) {}
static inline void LeaveCriticalSection(_RTL_CRITICAL_SECTION * /*cs*/) {}
static inline void InitializeCriticalSection(_RTL_CRITICAL_SECTION * /*cs*/) {}
static inline void DeleteCriticalSection(_RTL_CRITICAL_SECTION * /*cs*/) {}
#endif

// _BitScanReverse: MSVC intrinsic that finds the index of the most
// significant set bit. Returns 0 if mask is 0, else sets *index and
// returns nonzero. Implemented via __builtin_clz on clang/gcc.
#ifndef _WIN32
static inline unsigned char _BitScanReverse(unsigned long *index, unsigned long mask)
{
    if (!mask) return 0;
    *index = 31u - (unsigned long)__builtin_clz((unsigned int)mask);
    return 1;
}
static inline unsigned char _BitScanForward(unsigned long *index, unsigned long mask)
{
    if (!mask) return 0;
    *index = (unsigned long)__builtin_ctz((unsigned int)mask);
    return 1;
}
#endif

// __rdtsc: x86/x64 cycle counter intrinsic. On ARM64 we don't have a
// user-space cycle counter readily exposed; approximate with steady_clock
// nanoseconds. Off by a constant factor vs real cycles but adequate for
// frame-time stats.
#ifndef __rdtsc
#include <chrono>
static inline unsigned long long __rdtsc()
{
    using namespace std::chrono;
    return (unsigned long long)duration_cast<nanoseconds>(
        steady_clock::now().time_since_epoch()).count();
}
#endif

// PF_NON_TEMPORAL_LEVEL_ALL + PreFetchCacheLine: Xbox 360-style prefetch
// hint. No-op outside Win32; the constant just needs to exist for parse.
#ifndef PF_NON_TEMPORAL_LEVEL_ALL
#define PF_NON_TEMPORAL_LEVEL_ALL 4
#endif
#ifndef PreFetchCacheLine
#define PreFetchCacheLine(level, addr) ((void)(level), (void)(addr))
#endif


// Upstream's basic byte typedef lives in q_shared.h; expose it
// project-wide so headers that use 'byte' before q_shared.h is included
// (e.g. r_gfx.h reached through scr_const.h's chain) still parse. Mirror
// the upstream typedef exactly to avoid ODR issues.
typedef unsigned char byte;

// ARRAYSIZE: Win32 macro for compile-time array element count.
#ifndef ARRAYSIZE
#define ARRAYSIZE(a) (sizeof(a) / sizeof((a)[0]))
#endif

// Win32 InterlockedIncrement / InterlockedDecrement / InterlockedCompareExchange:
// atomic primitives. Map to GCC/clang __atomic_* builtins with seq-cst.
// Templated so int*/long*/etc. call sites match without overload juggling.
template <typename T>
static inline T InterlockedIncrement(T volatile *p)
{
    return __atomic_add_fetch(p, T{1}, __ATOMIC_SEQ_CST);
}
template <typename T>
static inline T InterlockedDecrement(T volatile *p)
{
    return __atomic_sub_fetch(p, T{1}, __ATOMIC_SEQ_CST);
}
template <typename T>
static inline T InterlockedCompareExchange(T volatile *p, T newval, T expected)
{
    T e = expected;
    __atomic_compare_exchange_n(p, &e, newval, false,
                                __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
    return e; // returns the value that was in *p before the call
}
// InterlockedExchangeAdd: returns the *previous* value at *p, then adds.
template <typename T>
static inline T InterlockedExchangeAdd(T volatile *p, T addend)
{
    return __atomic_fetch_add(p, addend, __ATOMIC_SEQ_CST);
}
// InterlockedExchange: atomically writes a new value and returns the
// previous value.
template <typename T>
static inline T InterlockedExchange(T volatile *p, T newval)
{
    return __atomic_exchange_n(p, newval, __ATOMIC_SEQ_CST);
}
// fopen_s: MSVC's "secure" fopen variant. Returns 0 on success and stores
// the FILE* in *out; POSIX has plain fopen that returns FILE* or NULL.
// Bridge: do the plain fopen and map to fopen_s's contract.
#include <cstdio>
#include <cerrno>
static inline int fopen_s(::FILE **out, const char *path, const char *mode)
{
    if (!out) return EINVAL;
    *out = std::fopen(path, mode);
    return *out ? 0 : errno;
}

// _time64/_localtime64: MSVC's explicit 64-bit time_t variants. POSIX
// time_t is already 64-bit on every platform we target (macOS arm64, Linux
// x86_64/arm64, Switch arm64), but isn't the same type as `long long`
// (typically `long`). Inline bridges convert.
#include <ctime>
inline long long _time64(long long *out)
{
    ::time_t now = ::time(nullptr);
    if (out) *out = (long long)now;
    return (long long)now;
}
inline ::tm *_localtime64(const long long *in)
{
    if (!in) return nullptr;
    ::time_t t = (::time_t)(*in);
    return ::localtime(&t);
}

// === POSIX libc collision: random()/srandom() ===============================
// POSIX <stdlib.h> declares `long random(void)` (BSD-derived). CoD4 has its
// own `float random()` in com_math.h, which clang treats as "functions that
// differ only in return type" and refuses to compile. Solution: rename
// CoD4's `random`/`crandom` via macros applied after <cstdlib> has been
// processed. The libc function remains accessible by the `random` symbol;
// CoD4 code becomes `kisak_random`.
#include <cstdlib>
#define random  kisak_random
#define crandom kisak_crandom

// === Fixed-size integer types ===============================================
// MSVC has __int8/16/32/64 as builtins, which allows `unsigned __int8`,
// `signed __int8`, etc. On POSIX we use #define (not typedef!) to preserve
// that property — the preprocessor swaps the token early, leaving `unsigned`
// to combine with the underlying type. A typedef would break
// `unsigned __int8 x`.
//
// Note: mapping __int8 to plain `char` means implementation-defined
// signedness on this token by itself, whereas MSVC guarantees signed. The
// upstream usage is almost entirely via `unsigned __int8` (bytes), so the
// difference rarely matters.
#define __int8  char
#define __int16 short
#define __int32 int
#define __int64 long long

// === __pragma ===============================================================
// Function-like #pragma used in MSVC to embed pragmas inside macros. The
// clang/gcc equivalent would be _Pragma() — for now a no-op, since the
// upstream usages are almost all warning-disables already covered by the
// POSIX build flags.
#ifndef __pragma
#define __pragma(x)
#endif

// === Basic Win32 types ======================================================
// Some upstream headers (qcommon/threads.h and friends) use DWORD/HANDLE/
// BOOL/HWND/LPCSTR in extern declarations instead of standard types. Rather
// than force-including <Windows.h> (only available in the MS SDK), we
// provide opaque equivalents. HANDLE = void* works as a generic pointer-
// shaped handle; the linker resolves at the right moment when the owning
// subsystem is ported.
typedef unsigned long DWORD;
typedef long          LONG;
typedef long long     LONGLONG;
typedef unsigned long long ULONGLONG;
typedef void         *HANDLE;
typedef void         *HWND;
typedef void         *HINSTANCE;
typedef int           BOOL;
typedef const char   *LPCSTR;
typedef char         *LPSTR;
typedef unsigned int  UINT;
typedef unsigned long ULONG;
typedef wchar_t       WCHAR;
typedef WCHAR        *LPWSTR;
typedef const WCHAR  *LPCWSTR;
typedef void         *LPVOID;
typedef const void   *LPCVOID;

// LARGE_INTEGER: Win32 union for 64-bit values. Upstream uses only
// .QuadPart (for QueryPerformanceCounter), so the anonymous-struct
// alternative form is enough.
typedef union {
    struct {
        DWORD LowPart;
        long  HighPart;
    };
    long long QuadPart;
} LARGE_INTEGER;

// OVERLAPPED: Win32 async I/O state. Only stored in upstream structs
// (database file-loader, etc.); never actually used on POSIX. Placeholder
// definition matches Win32's nominal size.
// D3DFORMAT subset upstream codes use. Defined as an enum so they're usable
// as switch labels and direct integer constants. Values match the Win32 DX9
// FOURCC encoding so persisted data and hex-rays artifacts stay valid.
#ifndef D3DFMT_L8
enum D3DFormatShim : unsigned int {
    D3DFMT_UNKNOWN  = 0,
    D3DFMT_A8       = 28,
    D3DFMT_L8       = 50,
    D3DFMT_A8L8     = 51,
    D3DFMT_R32F     = 114,
};
#endif

// D3DCUBEMAP_FACES — Win32 DX9 cubemap face enum. Upstream code only uses
// it as an opaque int-typed parameter to Image_UploadData. We declare it
// here as an enum alias so the parse succeeds; values match DX9 ordering.
#ifndef D3DCUBEMAP_FACE_POSITIVE_X
enum D3DCUBEMAP_FACES : int {
    D3DCUBEMAP_FACE_POSITIVE_X = 0,
    D3DCUBEMAP_FACE_NEGATIVE_X = 1,
    D3DCUBEMAP_FACE_POSITIVE_Y = 2,
    D3DCUBEMAP_FACE_NEGATIVE_Y = 3,
    D3DCUBEMAP_FACE_POSITIVE_Z = 4,
    D3DCUBEMAP_FACE_NEGATIVE_Z = 5,
};
#endif

typedef struct _OVERLAPPED {
    unsigned long long Internal;
    unsigned long long InternalHigh;
    union {
        struct { DWORD Offset; DWORD OffsetHigh; };
        void *Pointer;
    };
    void *hEvent;
} OVERLAPPED;

// QueryPerformanceCounter / QueryPerformanceFrequency: Win32 high-resolution
// timer API. Stubs are in src/posix/posix_stubs.cpp.
BOOL QueryPerformanceCounter(LARGE_INTEGER *count);
BOOL QueryPerformanceFrequency(LARGE_INTEGER *freq);

// === VirtualAlloc / VirtualFree shim ========================================
// Win32 separates address-space reservation from page commit:
//   VirtualAlloc(addr, size, MEM_RESERVE,             PAGE_READWRITE)
//   VirtualAlloc(addr, size, MEM_COMMIT,              PAGE_READWRITE)
//   VirtualAlloc(addr, size, MEM_RESERVE|MEM_COMMIT,  PAGE_READWRITE)
//   VirtualFree (addr, size, MEM_DECOMMIT)
//   VirtualFree (addr, 0,    MEM_RELEASE)
//
// POSIX has no reserve/commit split. We back the shim with anonymous mmap
// for RESERVE (and the combined RESERVE|COMMIT path) and munmap for
// RELEASE. COMMIT on an existing mapping becomes a no-op; DECOMMIT becomes
// madvise(MADV_DONTNEED) so the kernel can drop the backing pages without
// invalidating the address range. Sizes are page-rounded by mmap itself.
#ifndef MEM_RESERVE
#define MEM_RESERVE  0x2000u
#define MEM_COMMIT   0x1000u
#define MEM_DECOMMIT 0x4000u
#define MEM_RELEASE  0x8000u
#define PAGE_READWRITE 4u
#endif
void *VirtualAlloc(void *addr, size_t size, unsigned int flags, unsigned int prot);
BOOL  VirtualFree(void *addr, size_t size, unsigned int flags);

// Win32 UI helpers that show up in dialog-style error paths. On POSIX/
// Switch we have no native message-box; stubs return MB_YES (6) so the
// upstream code's "user accepted the change" branches keep working.
// MB_OK = 0, MB_OKCANCEL = 1, MB_YESNO = 4, MB_YESNOCANCEL = 3, MB_YES = 6.
HWND GetActiveWindow();
int  MessageBoxA(HWND hWnd, const char *text, const char *caption, unsigned int type);

// More Win32 types used in win_local.h declarations. Most exist only to
// allow the header to parse on POSIX — call sites should never execute on
// non-Windows.
typedef long          LRESULT;
typedef unsigned long long WPARAM;  // UINT_PTR equivalent
typedef long long          LPARAM;  // LONG_PTR equivalent
#define WINAPI

typedef struct tagOSVERSIONINFO {
    DWORD dwOSVersionInfoSize;
    DWORD dwMajorVersion;
    DWORD dwMinorVersion;
    DWORD dwBuildNumber;
    DWORD dwPlatformId;
    char  szCSDVersion[128];
} OSVERSIONINFO;

typedef struct _RTL_CRITICAL_SECTION {
    void        *DebugInfo;
    long         LockCount;
    long         RecursionCount;
    HANDLE       OwningThread;
    HANDLE       LockSemaphore;
    unsigned long SpinCount;
} _RTL_CRITICAL_SECTION, RTL_CRITICAL_SECTION, CRITICAL_SECTION;

// === DX9 opaque forward declarations ========================================
// Renderer headers (gfx_d3d/r_*.h) declare structs/globals of DX9 types
// (IDirect3DDevice9, vertex/index buffers, textures, _D3DFORMAT enum).
// On POSIX/Switch those interfaces have no implementation — the renderer
// is replaced by gfx_gl/. We forward-declare the types as opaque structs
// so headers parse; functions that take them are never called off Windows.
struct IDirect3D9;
struct IDirect3DDevice9;
struct IDirect3DVertexBuffer9;
struct IDirect3DIndexBuffer9;
struct IDirect3DBaseTexture9;
struct IDirect3DTexture9;
struct IDirect3DVolumeTexture9;
struct IDirect3DCubeTexture9;
struct IDirect3DSurface9;
struct IDirect3DStateBlock9;
struct IDirect3DVertexDeclaration9;
struct IDirect3DVertexShader9;
struct IDirect3DPixelShader9;
struct IDirect3DSwapChain9;
struct IDirect3DQuery9;
typedef int  _D3DFORMAT;  // enum in DX9 SDK; opaque int here
typedef int  D3DFORMAT;
typedef int  _D3DCUBEMAP_FACES;
typedef int  _D3DDISPLAYMODE;        // struct in DX9 SDK; opaque int here
typedef int  _D3DMULTISAMPLE_TYPE;   // enum
typedef int  _D3DTEXTUREFILTERTYPE;  // enum
struct _D3DCAPS9;             // big struct in DX9 SDK; opaque here
struct _D3DPRESENT_PARAMETERS_;
struct _D3DSURFACE_DESC;
struct _D3DVIEWPORT9;

// === Miles Sound System opaque types =======================================
// snd_local.h declares members/functions of these MSS types. On POSIX/
// Switch Miles is unavailable (proprietary) and the audio path is
// replaced; forward-declare so headers parse.
struct _SAMPLE;
struct _3DSAMPLE;
struct _DIG_DRIVER;
struct _REDBOOK;
struct _STREAM;
struct _DLSDEVICE;
struct _DLSFILEID;
struct _ASISTREAM;
typedef char MSS_FILE;           // upstream uses `const MSS_FILE *` as a path
#ifndef FAR
#define FAR                       // Win16 segment-attribute relic; no-op here
#endif
typedef int           S32;        // signed 32-bit
typedef unsigned int  U32;
typedef unsigned int  UINTa;      // Miles' uintptr_t-equivalent on 32-bit
typedef short         S16;
typedef unsigned short U16;
typedef signed char   S8;
typedef unsigned char U8;
typedef float         F32;
struct HWND__;        // Win32 HWND is `struct HWND__ *`
struct HINSTANCE__;   // Win32 HINSTANCE is `struct HINSTANCE__ *`
typedef long HRESULT;

// D3DFORMAT constants used in renderer headers. Real values don't matter
// outside Windows — the code paths that consume them never run.
#ifndef D3DFMT_D24S8
#define D3DFMT_D24S8 0
#endif
#ifndef D3DFMT_D24X8
#define D3DFMT_D24X8 0
#endif
#ifndef D3DFMT_UNKNOWN
#define D3DFMT_UNKNOWN 0
#endif
#ifndef D3DFMT_D16
#define D3DFMT_D16 0
#endif
#ifndef D3DFMT_A8R8G8B8
#define D3DFMT_A8R8G8B8 0
#endif
#ifndef D3DFMT_X8R8G8B8
#define D3DFMT_X8R8G8B8 0
#endif
#ifndef D3DFMT_DXT1
#define D3DFMT_DXT1 0
#endif
#ifndef D3DFMT_DXT3
#define D3DFMT_DXT3 0
#endif
#ifndef D3DFMT_DXT5
#define D3DFMT_DXT5 0
#endif

#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif

// Windows GDI POINT — minimal shim so cursor-position helpers compile.
// The POSIX/Switch input path is a stub; nothing reads x/y for real yet.
typedef struct tagPOINT { long x, y; } tagPOINT, POINT;

#endif // !_MSC_VER
