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

// QueryPerformanceCounter / QueryPerformanceFrequency: Win32 high-resolution
// timer API. Stubs are in src/posix/posix_stubs.cpp.
BOOL QueryPerformanceCounter(LARGE_INTEGER *count);
BOOL QueryPerformanceFrequency(LARGE_INTEGER *freq);

#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif

#endif // !_MSC_VER
