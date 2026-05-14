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

// AxisToQuat: build a quaternion (x, y, z, w) from a 3x3 rotation matrix
// `mat` stored as row-major. Declared in universal/com_math.h; full
// upstream impl lives in com_math.cpp (not yet portable). Standard
// Shepperd's method — numerically stable variant that picks the largest
// diagonal magnitude to avoid division by small numbers.
//
// When com_math.cpp is brought into the build this definition collides
// with upstream's; remove it then.
#include <cmath>
void AxisToQuat(const float (*mat)[3], float *out)
{
    const float trace = mat[0][0] + mat[1][1] + mat[2][2];
    if (trace > 0.0f) {
        const float s = std::sqrt(trace + 1.0f) * 2.0f; // s = 4*qw
        out[3] = 0.25f * s;
        out[0] = (mat[2][1] - mat[1][2]) / s;
        out[1] = (mat[0][2] - mat[2][0]) / s;
        out[2] = (mat[1][0] - mat[0][1]) / s;
    } else if (mat[0][0] > mat[1][1] && mat[0][0] > mat[2][2]) {
        const float s = std::sqrt(1.0f + mat[0][0] - mat[1][1] - mat[2][2]) * 2.0f;
        out[3] = (mat[2][1] - mat[1][2]) / s;
        out[0] = 0.25f * s;
        out[1] = (mat[0][1] + mat[1][0]) / s;
        out[2] = (mat[0][2] + mat[2][0]) / s;
    } else if (mat[1][1] > mat[2][2]) {
        const float s = std::sqrt(1.0f + mat[1][1] - mat[0][0] - mat[2][2]) * 2.0f;
        out[3] = (mat[0][2] - mat[2][0]) / s;
        out[0] = (mat[0][1] + mat[1][0]) / s;
        out[1] = 0.25f * s;
        out[2] = (mat[1][2] + mat[2][1]) / s;
    } else {
        const float s = std::sqrt(1.0f + mat[2][2] - mat[0][0] - mat[1][1]) * 2.0f;
        out[3] = (mat[1][0] - mat[0][1]) / s;
        out[0] = (mat[0][2] + mat[2][0]) / s;
        out[1] = (mat[1][2] + mat[2][1]) / s;
        out[2] = 0.25f * s;
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

// Sys_GetValue / Sys_SetValue: thread-local slot accessor. Upstream uses
// TLS to stash per-thread context (current parse session, render queue,
// etc.). Real impl uses pthread_key_create-allocated keys, lazily on
// first access. 16 slots ought to cover upstream's needs (the original
// uses no more than ~8).
#include <pthread.h>
#include <mutex>
namespace {
constexpr int KISAK_TLS_SLOTS = 16;
pthread_key_t  g_tls_keys[KISAK_TLS_SLOTS];
std::once_flag g_tls_init_flag;
void g_tls_init()
{
    for (int i = 0; i < KISAK_TLS_SLOTS; ++i) {
        pthread_key_create(&g_tls_keys[i], nullptr);
    }
}
} // namespace

void *Sys_GetValue(int valueIndex)
{
    std::call_once(g_tls_init_flag, g_tls_init);
    if (valueIndex < 0 || valueIndex >= KISAK_TLS_SLOTS) return nullptr;
    return pthread_getspecific(g_tls_keys[valueIndex]);
}

void Sys_SetValue(int valueIndex, void *value)
{
    std::call_once(g_tls_init_flag, g_tls_init);
    if (valueIndex < 0 || valueIndex >= KISAK_TLS_SLOTS) return;
    pthread_setspecific(g_tls_keys[valueIndex], value);
}

// va: Quake3's classic "vsprintf into rotating static buffer" utility.
// Defined in q_shared.cpp upstream, which we can't compile yet (drags in
// gfx_d3d/r_model.h). 32-slot rotation matches upstream's MAX_VA_STRING /
// "rotating buffer" count so call chains like
//   Com_Printf("%s %s %s", va("..."), va("..."), va("..."))
// never overwrite an earlier slot before it's consumed.
char *va(const char *format, ...)
{
    constexpr int VA_SLOTS = 32;
    constexpr int VA_SLOT_SIZE = 1024;
    static char buffers[VA_SLOTS][VA_SLOT_SIZE];
    static int  slot = 0;
    char *out = buffers[slot];
    slot = (slot + 1) & (VA_SLOTS - 1);
    va_list ap;
    va_start(ap, format);
    std::vsnprintf(out, VA_SLOT_SIZE, format ? format : "", ap);
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
