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
