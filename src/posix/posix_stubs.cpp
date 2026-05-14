// Stubs for upstream symbols referenced by the first ported files, whose
// definitions live in sources that do not yet compile on POSIX (xanim/ode,
// qcommon/threads.cpp using Windows.h, etc.).
//
// Each stub here is provisional: it prints a [stub] marker on stderr when
// called at runtime, returns a neutral value, and *must be removed* as soon
// as its owning file is properly ported. See docs/SWITCH_PORT.md.

#include <cctype>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <strings.h> // strcasecmp

// Forward-decl to avoid pulling all of qcommon.h into the stubs file. Must
// match the definition in src/qcommon/qcommon.h:154.
enum errorParm_t : int;

// I_stricmp: declared in universal/q_shared.h, defined in q_shared.cpp.
// Direct POSIX equivalent.
int I_stricmp(const char *s0, const char *s1)
{
    return ::strcasecmp(s0 ? s0 : "", s1 ? s1 : "");
}

int I_strnicmp(const char *s0, const char *s1, int n)
{
    if (n <= 0) return 0;
    return ::strncasecmp(s0 ? s0 : "", s1 ? s1 : "", (size_t)n);
}

// I_strncpyz: Quake3 "safe strncpy" — copies up to destsize-1 bytes and
// always null-terminates. Defined in q_shared.cpp upstream.
void I_strncpyz(char *dest, const char *src, int destsize)
{
    if (!dest || destsize <= 0) return;
    if (!src) { dest[0] = '\0'; return; }
    int i = 0;
    for (; i < destsize - 1 && src[i]; ++i) {
        dest[i] = src[i];
    }
    dest[i] = '\0';
}

// AxisToQuat: declared in universal/com_math.h (line 292), defined in
// com_math.cpp (which does not yet compile on POSIX due to xanim/ode).
// Stub returns the identity quaternion.
void AxisToQuat(const float (*mat)[3], float *out)
{
    (void)mat;
    out[0] = 0.0f;
    out[1] = 0.0f;
    out[2] = 0.0f;
    out[3] = 1.0f;
    static bool warned = false;
    if (!warned) {
        std::fprintf(stderr, "[stub] AxisToQuat: identity — com_math.cpp port pending\n");
        warned = true;
    }
}

// Vec2Normalize: declared in universal/com_math.h (line 230), defined in
// com_math.cpp line 559. Stub computes the normalize manually (without
// using vec2r to avoid dragging in the whole header) — should be correct
// enough that removing it when com_math.cpp ports causes no behavior diff.
float Vec2Normalize(float *v)
{
    const float lensq = v[0] * v[0] + v[1] * v[1];
    if (lensq <= 0.0f) {
        return 0.0f;
    }
    const float len = std::sqrt(lensq);
    const float inv = 1.0f / len;
    v[0] *= inv;
    v[1] *= inv;
    return len;
}

// === common / log / thread subsystem stubs =================================
// Demanded by src/universal/q_parse.cpp. Real implementations land when we
// port qcommon/common.cpp and qcommon/threads.cpp.

// Com_Printf: generic console log. Upstream has channels (CON_CHANNEL_*)
// — ignored for now, everything goes to stdout.
void Com_Printf(int /*channel*/, const char *fmt, ...)
{
    if (!fmt) return;
    va_list ap;
    va_start(ap, fmt);
    std::vfprintf(stdout, fmt, ap);
    va_end(ap);
}

// Com_PrintError: same but prefixed and routed to stderr.
void Com_PrintError(int /*channel*/, const char *fmt, ...)
{
    if (!fmt) return;
    std::fputs("[error] ", stderr);
    va_list ap;
    va_start(ap, fmt);
    std::vfprintf(stderr, fmt, ap);
    va_end(ap);
}

// Com_Error: fatal error. Upstream may be ERR_DROP (recoverable, longjmp)
// or ERR_FATAL (abort). We have no error recovery in the port yet; abort.
void Com_Error(errorParm_t /*code*/, const char *fmt, ...)
{
    std::fputs("[fatal] ", stderr);
    if (fmt) {
        va_list ap;
        va_start(ap, fmt);
        std::vfprintf(stderr, fmt, ap);
        va_end(ap);
    }
    std::fputc('\n', stderr);
    std::abort();
}

// Sys_Is*Thread: in the initial single-threaded port, we are always main
// and never render/database. When threads.cpp is properly ported, this
// gains real logic based on pthread_self comparison.
bool Sys_IsMainThread()     { return true;  }
bool Sys_IsRenderThread()   { return false; }
bool Sys_IsDatabaseThread() { return false; }

// Sys_GetValue: thread-local slot getter (upstream uses TLS to stash per-
// thread context like the current parse session). Stub returns nullptr —
// callers handle null gracefully in Q3-derived code; full impl lands with
// threads.cpp port.
void *Sys_GetValue(int /*valueIndex*/) { return nullptr; }

// va: Quake3's classic "vsprintf into rotating static buffer" utility.
// Defined in q_shared.cpp upstream, which we can't compile yet (drags in
// gfx_d3d/r_model.h). Local 8-slot rotation is enough for the call sites
// that show up before q_shared.cpp ports.
char *va(const char *format, ...)
{
    static char buffers[8][1024];
    static int  slot = 0;
    char *out = buffers[slot];
    slot = (slot + 1) & 7;
    va_list ap;
    va_start(ap, format);
    std::vsnprintf(out, sizeof(buffers[0]), format ? format : "", ap);
    va_end(ap);
    return out;
}

// === Stubs required by com_shared.cpp ======================================

// _copyDWord: upstream uses x86 inline asm (rep stosd) to fill `count`
// dwords with `value`. Portable equivalent is the obvious loop — gets
// auto-vectorized to NEON on ARM64 by clang at -O2.
void _copyDWord(unsigned int *dst, unsigned int value, unsigned int count)
{
    for (unsigned int i = 0; i < count; ++i) {
        dst[i] = value;
    }
}

// QueryPerformanceCounter / Frequency: portable POSIX implementations
// using std::chrono's steady_clock. Granularity is nanoseconds → matches
// or exceeds Win32 QPC on most hardware. Declarations live in
// src/posix/kisak_compat.h.
#include <chrono>
BOOL QueryPerformanceCounter(LARGE_INTEGER *count)
{
    if (!count) return 0;
    using namespace std::chrono;
    const auto ns = duration_cast<nanoseconds>(
        steady_clock::now().time_since_epoch()).count();
    count->QuadPart = static_cast<long long>(ns);
    return 1;
}
BOOL QueryPerformanceFrequency(LARGE_INTEGER *freq)
{
    if (!freq) return 0;
    freq->QuadPart = 1000000000LL; // ticks per second (we report in ns)
    return 1;
}

// ClearBounds / ExpandBounds: declared in com_math.h, defined in
// com_math.cpp. Trivial math we can implement portably; will collide with
// com_math.cpp's versions when that file ports, at which point these stubs
// get removed.
#include <cfloat>
void ClearBounds(float *mins, float *maxs)
{
    mins[0] = mins[1] = mins[2] = FLT_MAX;
    maxs[0] = maxs[1] = maxs[2] = -FLT_MAX;
}

void ExpandBounds(const float *amins, const float *amaxs,
                  float *omins, float *omaxs)
{
    for (int i = 0; i < 3; ++i) {
        if (amins[i] < omins[i]) omins[i] = amins[i];
        if (amaxs[i] > omaxs[i]) omaxs[i] = amaxs[i];
    }
}

// I_stristr: case-insensitive substring search. Manual implementation
// because strcasestr is a non-standard extension (BSD/GNU) and may not be
// in Switch newlib.
const char *I_stristr(const char *haystack, const char *needle)
{
    if (!haystack || !needle || !*needle) {
        return haystack;
    }
    for (; *haystack; ++haystack) {
        const char *h = haystack;
        const char *n = needle;
        while (*h && *n &&
               std::tolower(static_cast<unsigned char>(*h)) ==
                   std::tolower(static_cast<unsigned char>(*n))) {
            ++h;
            ++n;
        }
        if (!*n) {
            return haystack;
        }
    }
    return nullptr;
}
