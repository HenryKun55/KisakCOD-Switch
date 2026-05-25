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

// Com_Printf / Com_PrintError / Com_Error now live in qcommon/common.cpp
// (the real implementations from upstream) once it joined the build.

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

// _copyDWord now lives in qcommon/common.cpp.

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

// Win32 UI stubs — see kisak_compat.h comment.
HWND GetActiveWindow() { return nullptr; }
int  MessageBoxA(HWND /*hWnd*/, const char *text, const char *caption,
                 unsigned int /*type*/)
{
    std::fprintf(stderr, "[messagebox] %s: %s\n",
                 caption ? caption : "(no caption)",
                 text ? text : "(no text)");
    return 6; // IDYES — assume user accepts. Upstream call sites use this
              // for config-change confirmation dialogs that block on Windows.
}

// Vec3 helpers: declared in com_math.h, defined in com_math.cpp. Standard
// vector math we can implement portably; collide with com_math.cpp's
// versions when that file ports, at which point these get removed.
float Vec3Dot(const float *a, const float *b)
{
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

float Vec3NormalizeTo(const float *in, float *out)
{
    const float lensq = in[0]*in[0] + in[1]*in[1] + in[2]*in[2];
    if (lensq <= 0.0f) {
        out[0] = out[1] = out[2] = 0.0f;
        return 0.0f;
    }
    const float len = std::sqrt(lensq);
    const float inv = 1.0f / len;
    out[0] = in[0] * inv;
    out[1] = in[1] * inv;
    out[2] = in[2] * inv;
    return len;
}

float Vec3Normalize(float *v)
{
    return Vec3NormalizeTo(v, v);
}

float Vec3LengthSq(const float *v)
{
    return v[0]*v[0] + v[1]*v[1] + v[2]*v[2];
}

void Vec3Sub(const float *a, const float *b, float *out)
{
    out[0] = a[0] - b[0];
    out[1] = a[1] - b[1];
    out[2] = a[2] - b[2];
}

// Vec3Mad: out = a + scale * b (multiply-add). Standard q3 helper.
void Vec3Mad(const float *a, float scale, const float *b, float *out)
{
    out[0] = a[0] + scale * b[0];
    out[1] = a[1] + scale * b[1];
    out[2] = a[2] + scale * b[2];
}

// ProfLoad tracking: map-profile timing instrumentation. Stub no-ops
// until the profile-load subsystem is properly wired up. Forward-decl
// the enum so the mangled signature matches the upstream callers.
enum MapProfileTrackedValue : int;
// ProfLoad_BeginTrackedValue now in qcommon/com_profilemapload.cpp.
// ProfLoad_EndTrackedValue now in qcommon/com_profilemapload.cpp.

// === Vec3 / matrix math required by the cm_*.cpp collision files =========
// All real implementations (not placeholder stubs). When com_math.cpp
// finally ports, these collide with upstream's versions — remove then.

float Q_fabs(float v) { return std::fabs(v); }

float Vec3Length(const float *v)
{
    return std::sqrt(v[0]*v[0] + v[1]*v[1] + v[2]*v[2]);
}

float Vec3DistanceSq(const float *a, const float *b)
{
    const float dx = a[0]-b[0], dy = a[1]-b[1], dz = a[2]-b[2];
    return dx*dx + dy*dy + dz*dz;
}

float Vec3Distance(const float *a, const float *b)
{
    return std::sqrt(Vec3DistanceSq(a, b));
}

bool Vec3IsNormalized(const float *v)
{
    const float lensq = v[0]*v[0] + v[1]*v[1] + v[2]*v[2];
    return std::fabs(lensq - 1.0f) < 0.0001f;
}

void Vec3Add(const float *a, const float *b, float *out)
{
    out[0] = a[0] + b[0];
    out[1] = a[1] + b[1];
    out[2] = a[2] + b[2];
}

void Vec3Cross(const float *a, const float *b, float *out)
{
    out[0] = a[1]*b[2] - a[2]*b[1];
    out[1] = a[2]*b[0] - a[0]*b[2];
    out[2] = a[0]*b[1] - a[1]*b[0];
}

void Vec3Scale(const float *a, float s, float *out)
{
    out[0] = a[0] * s;
    out[1] = a[1] * s;
    out[2] = a[2] * s;
}

void Vec3Lerp(const float *a, const float *b, float t, float *out)
{
    out[0] = a[0] + t * (b[0] - a[0]);
    out[1] = a[1] + t * (b[1] - a[1]);
    out[2] = a[2] + t * (b[2] - a[2]);
}

// out = a + s1*b + s2*c (multiply-add-multiply-add).
void Vec3MadMad(const float *a, float s1, const float *b,
                float s2, const float *c, float *out)
{
    out[0] = a[0] + s1 * b[0] + s2 * c[0];
    out[1] = a[1] + s1 * b[1] + s2 * c[1];
    out[2] = a[2] + s1 * b[2] + s2 * c[2];
}

// Returns 1 if every component of `a` differs from `b` by at most
// `epsilon`, 0 otherwise. `n` is the component count (3 for vec3).
int VecNCompareCustomEpsilon(const float *a, const float *b, float epsilon, int n)
{
    for (int i = 0; i < n; ++i) {
        if (std::fabs(a[i] - b[i]) > epsilon) return 0;
    }
    return 1;
}

// 3x3 matrix * vec3.
void MatrixTransformVector(const float *in, const float (&m)[3][3], float *out)
{
    out[0] = in[0]*m[0][0] + in[1]*m[1][0] + in[2]*m[2][0];
    out[1] = in[0]*m[0][1] + in[1]*m[1][1] + in[2]*m[2][1];
    out[2] = in[0]*m[0][2] + in[1]*m[1][2] + in[2]*m[2][2];
}

// 3x3 transposed matrix * vec3 (used to take a vector from world into a
// local frame whose basis is the rows of m).
void MatrixTransposeTransformVector(const float *in, const float (&m)[3][3], float *out)
{
    out[0] = in[0]*m[0][0] + in[1]*m[0][1] + in[2]*m[0][2];
    out[1] = in[0]*m[1][0] + in[1]*m[1][1] + in[2]*m[1][2];
    out[2] = in[0]*m[2][0] + in[1]*m[2][1] + in[2]*m[2][2];
}

// In-place transpose of a 3x3 matrix (out = in^T).
void G_TransposeMatrix(float (*in)[3], float (*out)[3])
{
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            out[i][j] = in[j][i];
        }
    }
}

// G_RotatePoint: rotates `pt` (vec3) in-place by a 3x3 matrix.
void G_RotatePoint(float *pt, float (*m)[3])
{
    float tmp[3] = { pt[0], pt[1], pt[2] };
    pt[0] = tmp[0]*m[0][0] + tmp[1]*m[1][0] + tmp[2]*m[2][0];
    pt[1] = tmp[0]*m[0][1] + tmp[1]*m[1][1] + tmp[2]*m[2][1];
    pt[2] = tmp[0]*m[0][2] + tmp[1]*m[1][2] + tmp[2]*m[2][2];
}

// Plane equation from 3 points: plane[0..2] = normal, plane[3] = distance.
void PlaneFromPoints(float *plane, const float *a, const float *b, const float *c)
{
    const float ab[3] = { b[0]-a[0], b[1]-a[1], b[2]-a[2] };
    const float ac[3] = { c[0]-a[0], c[1]-a[1], c[2]-a[2] };
    plane[0] = ab[1]*ac[2] - ab[2]*ac[1];
    plane[1] = ab[2]*ac[0] - ab[0]*ac[2];
    plane[2] = ab[0]*ac[1] - ab[1]*ac[0];
    const float lensq = plane[0]*plane[0] + plane[1]*plane[1] + plane[2]*plane[2];
    if (lensq > 0.0f) {
        const float inv = 1.0f / std::sqrt(lensq);
        plane[0] *= inv;
        plane[1] *= inv;
        plane[2] *= inv;
    }
    plane[3] = plane[0]*a[0] + plane[1]*a[1] + plane[2]*a[2];
}

// Intersect 3 planes (each plane is 4 floats: nx ny nz d). `planes` is an
// array of 3 const float* (one per plane). Result in `out` (vec3).
// Solves planes[i] · p = planes[i][3] via Cramer's rule. Returns the
// intersection unchanged on near-singular configurations (callers handle
// the no-intersection case via separate validity checks upstream).
void IntersectPlanes(const float **planes, float *out)
{
    const float a = planes[0][0], b = planes[0][1], c = planes[0][2];
    const float d = planes[1][0], e = planes[1][1], f = planes[1][2];
    const float g = planes[2][0], h = planes[2][1], i = planes[2][2];
    const float det = a*(e*i - f*h) - b*(d*i - f*g) + c*(d*h - e*g);
    if (std::fabs(det) < 1e-9f) {
        out[0] = out[1] = out[2] = 0.0f;
        return;
    }
    const float pa = planes[0][3], pb = planes[1][3], pc = planes[2][3];
    const float invDet = 1.0f / det;
    out[0] = invDet * (pa*(e*i - f*h) - b*(pb*i - f*pc) + c*(pb*h - e*pc));
    out[1] = invDet * (a*(pb*i - f*pc) - pa*(d*i - f*g) + c*(d*pc - pb*g));
    out[2] = invDet * (a*(e*pc - pb*h) - b*(d*pc - pb*g) + pa*(d*h - e*g));
}

// SnapPointToIntersectingPlanes: snap `pt` so it lies as close as possible
// to all 3 planes' intersection within the given tolerances. Stub uses the
// raw 3-plane intersection — upstream's algorithm refines along a tolerance
// disc but the snap is rarely on the hot path for our build.
void SnapPointToIntersectingPlanes(const float **planes, float *pt,
                                   float /*tolerance*/, float /*step*/)
{
    IntersectPlanes(planes, pt);
}

// === Engine stubs required by cm_* collision files ========================
// These are subsystems we have not yet ported. Stubs accept the calls and
// either no-op or return sentinel values so the collision module links.

#include <new>

// Hunk_Alloc: upstream's permanent memory hunk allocator. Backed by a fresh
// new[] for now — bytes leak intentionally; the hunk lives for the engine's
// lifetime in upstream too. Will be replaced when qcommon's hunk subsystem
// is properly ported.
void *Hunk_Alloc(unsigned int size, const char * /*name*/, int /*type*/)
{
    return new (std::nothrow) unsigned char[size]();
}

// Hunk_AllocAlign: aligned hunk allocation. new[] on uchar gives at least
// alignof(std::max_align_t), enough for EffectsCore's float-array uses.
void *Hunk_AllocAlign(unsigned int size, int /*align*/,
                      const char * /*name*/, int /*type*/)
{
    return new (std::nothrow) unsigned char[size]();
}

// Sys_Error: fatal engine error. Same behaviour as Com_Error for now.
void Sys_Error(const char *fmt, ...)
{
    std::fputs("[sys-fatal] ", stderr);
    if (fmt) {
        va_list ap;
        va_start(ap, fmt);
        std::vfprintf(stderr, fmt, ap);
        va_end(ap);
    }
    std::fputc('\n', stderr);
    std::abort();
}

// track_static_alloc_internal now provided by qcommon/mem_track.cpp.

// DB_FindXAssetHeader: asset lookup. Returns a default-constructed
// XAssetHeader (data=nullptr) so callers can fail gracefully. Real impl
// lands with db_load.cpp.
// Defined out-of-line via forward declaration of the union so we don't
// pull xanim/xanim.h into the stubs TU.
enum XAssetType : int;
union XAssetHeader { void *data; XAssetHeader() : data(nullptr) {} };
XAssetHeader DB_FindXAssetHeader(XAssetType /*type*/, const char * /*name*/)
{
    return XAssetHeader{};
}

// XModelTraceLine now provided by xanim/xmodel.cpp.

// CM_LoadMapData_LoadObj: collision-model loader entry. Real impl lands
// with cm_load_obj.cpp (which depends on more renderer state). Until
// then, map data simply isn't loaded.
void CM_LoadMapData_LoadObj(const char * /*name*/) {}

// === Globals required by cm_load and friends ==============================
// Definitions of upstream globals so the linker resolves the externs.
// Sizes match upstream layouts on 32-bit; on 64-bit they may be slightly
// larger due to pointer growth in member structs but the storage is
// allocated dynamically and field accesses go through the upstream types
// (no runtime impact for what we currently exercise).

// THREAD_CONTEXT_COUNT comes from qcommon/thread_context.h on POSIX.
// TraceThreadInfo is a substantial struct; allocate a generous buffer that
// covers its size (~16 KB per slot). The collision code only writes to
// thread-local copies, never reads the array directly in the cm_* set we
// link today, so the storage is effectively dead.
#include <qcommon/thread_context.h>
struct TraceThreadInfo;
alignas(16) static unsigned char g_traceThreadInfo_storage[THREAD_CONTEXT_COUNT * 16384];
// `g_traceThreadInfo` is declared `extern TraceThreadInfo array[N]` in
// upstream — we define the storage as an array via reinterpret_cast so
// the linker resolves both decl forms. Use `extern` linkage explicitly
// to avoid the const-pointer-treated-as-internal warning.
extern TraceThreadInfo * const g_traceThreadInfo;
TraceThreadInfo * const g_traceThreadInfo =
    reinterpret_cast<TraceThreadInfo *>(g_traceThreadInfo_storage);

// useFastFile is now defined in qcommon/common.cpp (real upstream).

// === statmonitor.cpp deps ================================================

// Sys_Milliseconds: monotonic ms since process start. Real impl via
// std::chrono::steady_clock — collides with q_shared.cpp when it ports.
unsigned int Sys_Milliseconds()
{
    using namespace std::chrono;
    static const auto start = steady_clock::now();
    return static_cast<unsigned int>(
        duration_cast<milliseconds>(steady_clock::now() - start).count());
}

// Sys_EnterCriticalSection / LeaveCriticalSection: upstream uses Win32
// CRITICAL_SECTION indexed by thread-domain enum. Real pthread_mutex
// impl; 16 named slots ought to cover all upstream call sites.
namespace {
constexpr int KISAK_CRIT_SLOTS = 16;
pthread_mutex_t g_crit_mutexes[KISAK_CRIT_SLOTS] = {
    PTHREAD_MUTEX_INITIALIZER, PTHREAD_MUTEX_INITIALIZER,
    PTHREAD_MUTEX_INITIALIZER, PTHREAD_MUTEX_INITIALIZER,
    PTHREAD_MUTEX_INITIALIZER, PTHREAD_MUTEX_INITIALIZER,
    PTHREAD_MUTEX_INITIALIZER, PTHREAD_MUTEX_INITIALIZER,
    PTHREAD_MUTEX_INITIALIZER, PTHREAD_MUTEX_INITIALIZER,
    PTHREAD_MUTEX_INITIALIZER, PTHREAD_MUTEX_INITIALIZER,
    PTHREAD_MUTEX_INITIALIZER, PTHREAD_MUTEX_INITIALIZER,
    PTHREAD_MUTEX_INITIALIZER, PTHREAD_MUTEX_INITIALIZER,
};
} // namespace

void Sys_EnterCriticalSection(int slot)
{
    if (slot < 0 || slot >= KISAK_CRIT_SLOTS) return;
    pthread_mutex_lock(&g_crit_mutexes[slot]);
}

void Sys_LeaveCriticalSection(int slot)
{
    if (slot < 0 || slot >= KISAK_CRIT_SLOTS) return;
    pthread_mutex_unlock(&g_crit_mutexes[slot]);
}

// Material_RegisterHandle: looks up a Material by name. Stub returns
// nullptr — material system isn't ported. Forward-decl the Material
// struct so the mangled signature matches.
struct Material;
Material *Material_RegisterHandle(const char * /*name*/, int /*imageTrack*/)
{
    return nullptr;
}

// cls global is now provided by src/client_mp/cl_main_mp.cpp.

// com_statmon now defined in qcommon/common.cpp (real upstream).

// === scr_const.cpp dep ====================================================
// GScr_AllocString: register a string in the script string-table and
// return its 16-bit handle. Stub returns 0 (an empty/invalid handle).
// Real impl lands with scr_stringlist.cpp (which has its own deps).
unsigned short GScr_AllocString(const char * /*str*/) { return 0; }

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
