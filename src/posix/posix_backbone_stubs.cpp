// posix_backbone_stubs.cpp — Engine subsystem stubs for the Phase 1 POSIX build.
//
// When we wired qcommon/{cmd,common,files,threads-skipped}.cpp +
// universal/{dvar,dvar_cmds}.cpp into the build, the linker surfaced ~180
// references into subsystems we have not yet ported (client_mp, server_mp,
// sound, renderer, UI, asset database, scripting, networking, ...). This
// file provides minimal implementations so the executable links.
//
// Three kinds of bodies live here:
//   1. Real impls for libc-flavored helpers (CopyString, Z_Malloc, Com_sprintf,
//      Vec4Compare, I_strncmp, ...). They behave correctly.
//   2. Light stubs that return a safe default (FS_Initialized=false,
//      Sys_GetCpuCount=1, NET_*=0, ...). They do nothing useful but let
//      callers fall through into their early-out path.
//   3. Hard stubs (CL_*, SV_*, DB_*, R_*, SND_*, UI_*, Scr_*) that just
//      return. These will need real implementations as the matching
//      subsystems get ported. Any call into one of these is silently
//      dropped on the floor for now.
//
// Globals at the bottom are sized zero-init storage so any reads land in
// "default constructed" land rather than crashing on null deref.
//
// As subsystems land for real, delete the matching block here. Compile
// errors at that point are the desired signal.

#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cctype>
#include <cmath>

#include <qcommon/qcommon.h>
#include <qcommon/threads.h>
#include <universal/com_files.h>
#include <universal/com_math.h>
#include <bgame/bg_local.h>
#include <client_mp/client_mp.h>
#include <server_mp/server_mp.h>
#include <stringed/stringed_hooks.h>
#include <ui/ui_shared.h>
#include <client/client.h>
#include <DynEntity/DynEntity_client.h>
#include <script/scr_variable.h>
#include <script/scr_parser.h>
#include <script/scr_main.h>
#include <script/scr_compiler.h>
#include <script/scr_vm.h>
#include <qcommon/msg_mp.h>
#include <qcommon/sv_msg_write_mp.h>
#include <game/game_public.h>
#include <universal/com_sndalias.h>
#include <physics/phys_local.h>
#include <cgame/cg_local.h>
#include <cgame_mp/cg_local_mp.h>
#include <sound/snd_public.h>
#include <gfx_d3d/r_init.h>
#include <gfx_d3d/r_rendercmds.h>
#include <EffectsCore/fx_system.h>
#include <game_mp/g_public_mp.h>
#include <aim_assist/aim_assist.h>

// Forward decls for opaque types we just need to pass through.
struct sysEvent_t;
struct FastCriticalSection;
struct netadr_t;
struct msg_t;
struct XZoneInfo;
struct snd_alias_t;
struct StringTable;

// =========================================================================
// String / memory helpers — real implementations.
// =========================================================================

const char *CopyString(const char *in)
{
    if (!in) return nullptr;
    size_t n = std::strlen(in) + 1;
    char *out = static_cast<char *>(std::malloc(n));
    std::memcpy(out, in, n);
    return out;
}

void FreeString(const char *str)
{
    std::free(const_cast<char *>(str));
}

bool CanKeepStringPointer(const char * /*string*/)
{
    // Upstream: tells dvar whether the caller-provided string pointer is
    // backed by a stable storage region (constant pool, hunk, etc.). When
    // false, the dvar has to copy. Returning false is always safe.
    return false;
}

void *Z_Malloc(int size, const char * /*name*/, int /*type*/)
{
    return std::malloc(static_cast<size_t>(size));
}

void Z_Free(void *ptr, int /*type*/)
{
    std::free(ptr);
}

bool Vec4Compare(const float *a, const float *b)
{
    return a[0] == b[0] && a[1] == b[1] && a[2] == b[2] && a[3] == b[3];
}

bool Vec4IsNormalized(const float *v)
{
    float ls = v[0] * v[0] + v[1] * v[1] + v[2] * v[2] + v[3] * v[3];
    return ls > 0.99f && ls < 1.01f;
}

// Right-handed orthonormal basis from a forward vector. CoD convention:
// `forward` is the input direction, `left` and `up` are the two basis
// vectors completing the frame (forward × up = left).
void Vec3Basis_RightHanded(const float *forward, float *left, float *up)
{
    float ax = std::fabs(forward[0]);
    float ay = std::fabs(forward[1]);
    float az = std::fabs(forward[2]);
    float seed[3];
    if (ax <= ay && ax <= az)      { seed[0] = 1; seed[1] = 0; seed[2] = 0; }
    else if (ay <= ax && ay <= az) { seed[0] = 0; seed[1] = 1; seed[2] = 0; }
    else                           { seed[0] = 0; seed[1] = 0; seed[2] = 1; }
    // up = normalize(seed - dot(seed,forward) * forward)
    float d = seed[0] * forward[0] + seed[1] * forward[1] + seed[2] * forward[2];
    up[0] = seed[0] - d * forward[0];
    up[1] = seed[1] - d * forward[1];
    up[2] = seed[2] - d * forward[2];
    float ul = std::sqrt(up[0] * up[0] + up[1] * up[1] + up[2] * up[2]);
    if (ul > 0) { up[0] /= ul; up[1] /= ul; up[2] /= ul; }
    // left = forward × up
    left[0] = forward[1] * up[2] - forward[2] * up[1];
    left[1] = forward[2] * up[0] - forward[0] * up[2];
    left[2] = forward[0] * up[1] - forward[1] * up[0];
}

// Quaternion (x, y, z, w) → 3x3 rotation. CoD stores axes row-major.
void UnitQuatToAxis(const float *quat, float (&axis)[3][3])
{
    const float x = quat[0], y = quat[1], z = quat[2], w = quat[3];
    const float xx = x * x, yy = y * y, zz = z * z;
    const float xy = x * y, xz = x * z, yz = y * z;
    const float wx = w * x, wy = w * y, wz = w * z;
    axis[0][0] = 1 - 2 * (yy + zz);
    axis[0][1] = 2 * (xy + wz);
    axis[0][2] = 2 * (xz - wy);
    axis[1][0] = 2 * (xy - wz);
    axis[1][1] = 1 - 2 * (xx + zz);
    axis[1][2] = 2 * (yz + wx);
    axis[2][0] = 2 * (xz + wy);
    axis[2][1] = 2 * (yz - wx);
    axis[2][2] = 1 - 2 * (xx + yy);
}

// Seeded random unit-sphere direction. Marsaglia's method:
// pick two uniforms in [-1, 1] with s = x²+y² < 1, then map to a
// point on the unit sphere. Uses `fx_randomTable` for determinism
// (the engine wants the same particle to spawn the same direction
// across runs at a given seed).
void FX_RandomDir(int seed, float *dir);  // forward decl, defined after fx_randomTable.

// Com_sprintf: real impl over vsnprintf.
int Com_sprintf(char *dest, unsigned int size, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int n = std::vsnprintf(dest, size, fmt, ap);
    va_end(ap);
    if (n < 0) {
        if (size) dest[0] = 0;
        return 0;
    }
    if (static_cast<unsigned int>(n) >= size && size) dest[size - 1] = 0;
    return n;
}

void Com_DefaultExtension(char *path, unsigned int maxSize, const char *extension)
{
    if (!path || !extension) return;
    const char *dot = std::strrchr(path, '.');
    const char *slash = std::strrchr(path, '/');
    if (dot && (!slash || dot > slash)) return;
    size_t len = std::strlen(path);
    size_t extlen = std::strlen(extension);
    if (len + extlen + 1 > maxSize) return;
    std::memcpy(path + len, extension, extlen + 1);
}

const char *Com_GetFilenameSubString(const char *pathname)
{
    if (!pathname) return "";
    const char *slash = std::strrchr(pathname, '/');
    return slash ? slash + 1 : pathname;
}

// Com_BuildPlayerProfilePath now provided by qcommon/com_playerprofile.cpp.
// Com_HasPlayerProfile now provided by qcommon/com_playerprofile.cpp.
// Com_InitPlayerProfiles now in qcommon/com_playerprofile.cpp.
void Com_InitHunkMemory() {}
void Com_InitDObj() {}
void Com_ShutdownDObj() {}
// Com_ShutdownWorld now in qcommon/com_bsp.cpp.
// Com_CleanupBsp now in qcommon/com_bsp.cpp.
// Com_CheckSetRecommended now in qcommon/com_playerprofile.cpp.
void Com_GetSoundFileName(const snd_alias_t * /*alias*/, char *out, int outSize)
{
    if (outSize > 0) out[0] = 0;
}

// =========================================================================
// I_str* — case-insensitive libc-ish helpers.
// =========================================================================

char *I_strlwr(char *s)
{
    for (char *p = s; *p; ++p) *p = static_cast<char>(std::tolower(static_cast<unsigned char>(*p)));
    return s;
}

void I_strncat(char *dest, int size, const char *src)
{
    if (size <= 0 || !dest || !src) return;
    size_t dl = std::strlen(dest);
    if (static_cast<int>(dl) >= size) return;
    size_t avail = static_cast<size_t>(size) - dl - 1;
    std::strncat(dest, src, avail);
}

int I_strncmp(const char *s0, const char *s1, int n)
{
    return std::strncmp(s0, s1, static_cast<size_t>(n));
}

// =========================================================================
// Info_* — userinfo/serverinfo key/value strings. Upstream impl is in a
// file we haven't pulled yet; the stub here is a no-op so callers that
// build up a config string just get an empty result.
// =========================================================================

void Info_SetValueForKey(char * /*s*/, const char * /*key*/, const char * /*value*/) {}
void Info_SetValueForKey_Big(char * /*s*/, const char * /*key*/, const char * /*value*/) {}

// =========================================================================
// Sys_* — threading + system entry points not already in posix_stubs.cpp.
// =========================================================================

unsigned int Sys_GetCpuCount() { return 1; }
void Sys_Init() {}
int  Sys_IsRemoteDebugClient() { return 0; }
void Sys_DestroySplashWindow() {}
void Sys_Quit() { std::exit(0); }
void Sys_Print(const char *msg) { if (msg) std::fputs(msg, stdout); }

sysEvent_t *Sys_GetEvent(sysEvent_t *result)
{
    // Returns a zero-init event (SE_NONE) so Com_EventLoop falls out
    // immediately. Real input pumping happens in posix_gl_main.cpp via SDL.
    std::memset(result, 0, sizeof(int) * 6);
    return result;
}

void Sys_LockWrite(FastCriticalSection * /*cs*/) {}
void Sys_UnlockWrite(FastCriticalSection * /*cs*/) {}

void Win_UpdateThreadLock() {}

// =========================================================================
// FS_* — file system.
// FS_Initialized=false keeps Com_LogPrintMessage's early-out path. The
// rest will be replaced by a real POSIX-side iwd-aware FS once the engine
// is far enough along to need actual asset loads.
// =========================================================================

bool FS_Initialized() { return false; }
void FS_InitFilesystem() {}
void FS_Shutdown() {}
void FS_ResetFiles() {}
void FS_FCloseFile(int /*h*/) {}
void FS_FCloseLogFile(int /*h*/) {}
void FS_Flush(int /*f*/) {}
void FS_FreeFile(char * /*buffer*/) {}
int  FS_FilenameCompare(const char *s1, const char *s2)
{
    while (*s1 && *s2) {
        int c1 = std::tolower(static_cast<unsigned char>(*s1++));
        int c2 = std::tolower(static_cast<unsigned char>(*s2++));
        if (c1 == '\\') c1 = '/';
        if (c2 == '\\') c2 = '/';
        if (c1 != c2) return c1 - c2;
    }
    return *s1 - *s2;
}
unsigned int FS_FOpenFileRead(const char * /*filename*/, int *file)
{
    if (file) *file = 0;
    return 0;
}
int FS_FOpenFileWrite(const char * /*filename*/) { return 0; }
int FS_FOpenFileWriteToDir(const char * /*filename*/, const char * /*dir*/) { return 0; }
int FS_FOpenTextFileWrite(const char * /*filename*/) { return 0; }
int FS_ReadFile(const char * /*qpath*/, void **buffer)
{
    if (buffer) *buffer = nullptr;
    return -1;
}
int FS_SV_FileExists(char * /*file*/) { return 0; }
const char **FS_ListFiles(const char * /*path*/, const char * /*ext*/,
                          FsListBehavior_e /*behavior*/, int *numfiles)
{
    if (numfiles) *numfiles = 0;
    return nullptr;
}
unsigned int FS_WriteLog(const char * /*buffer*/, unsigned int /*len*/, int /*h*/) { return 0; }
void FS_Printf(int /*h*/, const char * /*fmt*/, ...) {}

// =========================================================================
// DB_* — asset database.
// =========================================================================

void DB_Cleanup() {}
int  DB_FileSize(const char * /*zoneName*/, int /*isMod*/) { return 0; }
int  DB_GetAllXAssetOfType_FastFile(XAssetType /*type*/, XAssetHeader * /*assets*/, int /*max*/) { return 0; }
void DB_InitThread() {}
bool DB_IsMinimumFastFileLoaded() { return false; }
void DB_LoadXAssets(XZoneInfo * /*zoneInfo*/, unsigned int /*zoneCount*/, int /*sync*/) {}
void DB_ReleaseXAssets() {}
void DB_ResetZoneSize(int /*trackLoadProgress*/) {}
void DB_SetInitializing(bool /*inUse*/) {}
void DB_ShutdownXAssets() {}
void DB_SyncXAssets() {}
void DB_Update() {}

// =========================================================================
// CL_* — client subsystem. All stubs for now (we don't have a real client).
// =========================================================================

void CL_CharEvent(int /*localClientNum*/, int /*ch*/) {}
void CL_ConsoleFixPosition() {}
void CL_ConsolePrint(int /*localClientNum*/, int /*channel*/, const char *msg, int /*r*/, int /*g*/, int /*b*/)
{
    if (msg) std::fputs(msg, stdout);
}
int  CL_ControllerIndexFromClientNum(int /*localClientNum*/) { return 0; }
void CL_Disconnect(int /*localClientNum*/) {}
// CL_FlushDebugServerData now in client/cl_debugdata.cpp.
void CL_ForwardCommandToServer(int /*localClientNum*/, const char * /*cmd*/) {}
void CL_Frame(netsrc_t /*sock*/) {}
struct clientConnection_t;
// CL_GetLocalClientConnection provided by src/cgame_mp/cg_main_mp.cpp now.
const char *CL_GetUsernameForLocalClient() { return ""; }
void CL_Init(int /*localClientNum*/) {}
void CL_InitDedicated() {}
void CL_InitKeyCommands() {}
void CL_InitOnceForAllClients() {}
void CL_InitRenderer() {}
void CL_KeyEvent(int /*localClientNum*/, int /*key*/, int /*down*/, unsigned int /*time*/) {}
char CL_PacketEvent(netsrc_t /*sock*/, netadr_t /*from*/, msg_t * /*msg*/, int /*time*/) { return 0; }
void CL_RunOncePerClientFrame(int /*localClientNum*/, int /*frameTime*/) {}
void CL_Shutdown(int /*localClientNum*/) {}
void CL_ShutdownAll(bool /*shutdownRef*/) {}
void CL_ShutdownHunkUsers() {}
void CL_ShutdownRef() {}
void CL_StartHunkUsers() {}
// CL_UpdateDebugServerData now in client/cl_debugdata.cpp.
void CL_UpdateSound() {}

// =========================================================================
// SV_* — server. Stubs.
// =========================================================================

// SV_AddDedicatedCommands provided by src/server_mp/sv_ccmds_mp.cpp now.
// SV_Frame provided by src/server_mp/sv_main_mp.cpp now.
// SV_GameCommand now in server/sv_game.cpp.
// SV_Init provided by src/server_mp/sv_init_mp.cpp now.
// SV_PacketEvent provided by src/server_mp/sv_main_mp.cpp now.
// SV_SetConfigValueForKey provided by src/server_mp/sv_init_mp.cpp now.
// SV_Shutdown provided by src/server_mp/sv_init_mp.cpp now.
// SV_ShutdownGameProgs now in server/sv_game.cpp.
// SV_WaitServer provided by src/server_mp/sv_main_mp.cpp now.

// =========================================================================
// SND_* — sound. Stubs.
// =========================================================================

void SND_ErrorCleanup() {}
void SND_Init() {}
char SND_InitDriver() { return 0; }
void SND_ShutdownChannels() {}
void SND_StopSounds(snd_stopsounds_arg_t /*arg*/) {}

// =========================================================================
// R_* — renderer pipeline (the high-level engine interface, distinct from
// the GL/EGL renderer in src/gfx_gl/). Stubs.
// =========================================================================

void R_BeginDebugFrame() {}
void R_BeginRemoteScreenUpdate() {}
void R_ComErrorCleanup() {}
void R_EndDebugFrame() {}
void R_EndRemoteScreenUpdate() {}
void R_InitThreads() {}
int  R_PopRemoteScreenUpdate() { return 0; }
void R_SetEndTime(int /*endTime*/) {}
void R_SyncRenderThread() {}
void R_WaitEndTime() {}
void R_WaitWorkerCmds() {}

// =========================================================================
// UI_* — UI shell. Stubs.
// =========================================================================

int  UI_GetMenuScreen() { return 0; }
int  UI_GetMenuScreenForError() { return 0; }
int  UI_IsFullscreen(int /*localClientNum*/) { return 0; }
int  UI_SetActiveMenu(int /*localClientNum*/, uiMenuCommand_t /*menu*/) { return 0; }
void UI_SetMap(char * /*name*/, char * /*gametype*/) {}

// =========================================================================
// Scripting + misc. Stubs.
// =========================================================================

bool Scr_CanDrawScript() { return false; }
void Scr_Cleanup() {}
void Scr_DrawScript() {}
void Scr_Init() {}
void Scr_InitVariables() {}
void Scr_MonitorCommand(const char * /*cmd*/) {}
void Scr_Settings(int /*developer*/, int /*developer_script*/, int /*abort_on_error*/) {}
void Scr_Shutdown() {}
int  Scr_UpdateDebugSocket() { return 0; }
// SCR_UpdateScreen provided by src/client_mp/cl_scrn_mp.cpp now.
void GScr_Shutdown() {}

void DObjInit() {}
void DObjShutdown() {}
void FakeLag_Init() {}
void FakeLag_Shutdown() {}
void FX_UnregisterAll() {}
void IN_Frame() {}
void DevGui_Update(int /*localClientNum*/, float /*frameTime*/) {}
// Ragdoll_Update now provided by ragdoll/ragdoll_update.cpp.
// SetAnimCheck now provided by script/scr_animtree.cpp.
void LargeLocalReset() {}
void LiveStorage_Init() {}
// XAnimInit/Shutdown now provided by xanim/xanim.cpp.
void Swap_Init() {}
void SL_Init() {}
void BG_ShutdownWeaponDefFiles() {}
int  BG_AnimScriptEvent(playerState_s * /*ps*/, scriptAnimEventTypes_t /*event*/, int /*isContinue*/, int /*force*/) { return 0; }
void BG_AddPredictableEventToPlayerstate(unsigned int /*event*/, unsigned int /*eventParm*/, playerState_s * /*ps*/) {}
int  PM_GetEffectiveStance(const playerState_s * /*ps*/) { return 0; }
unsigned int PM_GroundSurfaceType(pml_t * /*pml*/) { return 0; }

// =========================================================================
// xanim/bgame cascade — landed with the xanim + bgame files. Math
// helpers get real implementations; subsystem hooks stay stubbed.
// =========================================================================

void Vec3Clear(float *v) { v[0] = v[1] = v[2] = 0; }
void Vec3Copy(const float *in, float *out) { out[0] = in[0]; out[1] = in[1]; out[2] = in[2]; }
void Vec3Mul(const float *a, const float *b, float *out) { out[0] = a[0] * b[0]; out[1] = a[1] * b[1]; out[2] = a[2] * b[2]; }
float Vec2Length(const float *v) { return std::sqrt(v[0] * v[0] + v[1] * v[1]); }

float AngleNormalize360(float angle)
{
    float r = std::fmod(angle, 360.0f);
    if (r < 0) r += 360.0f;
    return r;
}

float AngleDelta(float a, float b)
{
    float d = AngleNormalize360(a - b);
    if (d > 180.0f) d -= 360.0f;
    return d;
}

float Q_rint(float v) { return std::rintf(v); }

void vectoangles(const float *value1, float *angles)
{
    float forward, yaw, pitch;
    if (value1[1] == 0 && value1[0] == 0) {
        yaw = 0;
        pitch = (value1[2] > 0) ? 90 : 270;
    } else {
        yaw = std::atan2(value1[1], value1[0]) * (180.0f / 3.14159265358979323846f);
        if (yaw < 0) yaw += 360;
        forward = std::sqrt(value1[0] * value1[0] + value1[1] * value1[1]);
        pitch = std::atan2(value1[2], forward) * (180.0f / 3.14159265358979323846f);
        if (pitch < 0) pitch += 360;
    }
    angles[0] = -pitch;
    angles[1] = yaw;
    angles[2] = 0;
}

float vectoyaw(const float *vec)
{
    if (vec[1] == 0 && vec[0] == 0) return 0;
    float yaw = std::atan2(vec[1], vec[0]) * (180.0f / 3.14159265358979323846f);
    if (yaw < 0) yaw += 360;
    return yaw;
}

void VectorAngleMultiply(float *vec, float scale)
{
    vec[0] = AngleNormalize360(vec[0] * scale);
    vec[1] = AngleNormalize360(vec[1] * scale);
    vec[2] = AngleNormalize360(vec[2] * scale);
}

// Quaternion algebra helpers. Quaternions stored as (x, y, z, w).
static inline void quat_mul(const float *a, const float *b, float *r)
{
    r[0] = a[3] * b[0] + a[0] * b[3] + a[1] * b[2] - a[2] * b[1];
    r[1] = a[3] * b[1] - a[0] * b[2] + a[1] * b[3] + a[2] * b[0];
    r[2] = a[3] * b[2] + a[0] * b[1] - a[1] * b[0] + a[2] * b[3];
    r[3] = a[3] * b[3] - a[0] * b[0] - a[1] * b[1] - a[2] * b[2];
}

void QuatMultiplyEquals(const float *q, float *acc)
{
    float r[4];
    quat_mul(acc, q, r);
    acc[0] = r[0]; acc[1] = r[1]; acc[2] = r[2]; acc[3] = r[3];
}

void QuatMultiplyReverseEquals(const float *q, float *acc)
{
    float r[4];
    quat_mul(q, acc, r);
    acc[0] = r[0]; acc[1] = r[1]; acc[2] = r[2]; acc[3] = r[3];
}

void QuatMultiplyReverseInverse(const float *a, const float *b, float *out)
{
    float binv[4] = { -b[0], -b[1], -b[2], b[3] };
    quat_mul(a, binv, out);
}

// DObjAnimMat → rotation matrix (3x3). DObjAnimMat is a quaternion + scale.
// Forward-decl DObjAnimMat as opaque is enough since we only take a
// pointer; layout doesn't matter — these are stubs anyway.
void ConvertQuatToMat(const DObjAnimMat * /*mat*/, float (*out)[3])
{
    for (int i = 0; i < 3; ++i) for (int j = 0; j < 3; ++j) out[i][j] = (i == j) ? 1.0f : 0;
}
void ConvertQuatToInverseMat(const DObjAnimMat * /*mat*/, float (*out)[3])
{
    for (int i = 0; i < 3; ++i) for (int j = 0; j < 3; ++j) out[i][j] = (i == j) ? 1.0f : 0;
}
void MatrixTransformVectorQuatTransEquals(const DObjAnimMat * /*mat*/, float * /*v*/) {}

// DObj
void DObjCalcAnim(const DObj_s * /*obj*/, int * /*partBits*/) {}
void DObjDumpInfo(const DObj_s * /*obj*/) {}
void DObjGetHidePartBits(const DObj_s * /*obj*/, unsigned int * /*partBits*/) {}

// PM_
void PM_AddTouchEnt(pmove_t * /*pm*/, int /*entityNum*/) {}
void PM_ClipVelocity(const float *in, const float *normal, float *out)
{
    float dot = in[0] * normal[0] + in[1] * normal[1] + in[2] * normal[2];
    out[0] = in[0] - dot * normal[0];
    out[1] = in[1] - dot * normal[1];
    out[2] = in[2] - dot * normal[2];
}
void PM_ProjectVelocity(const float *in, const float *normal, float *out)
{
    PM_ClipVelocity(in, normal, out);
}
void PM_FootstepEvent(pmove_t * /*pm*/, pml_t * /*pml*/, char /*surfType*/, char /*step*/, int /*eventParm*/) {}
bool PM_ShouldMakeFootsteps(pmove_t * /*pm*/) { return false; }
void PM_playerTrace(pmove_t * /*pm*/, trace_t *trace, const float * /*start*/, const float * /*mins*/,
                    const float * /*maxs*/, const float * /*end*/, int /*passEnt*/, int /*contentMask*/)
{
    if (trace) {
        std::memset(trace, 0, sizeof(*trace));
        trace->fraction = 1.0f;
    }
}
void PM_trace(pmove_t *pm, trace_t *trace, const float *start, const float *mins,
              const float *maxs, const float *end, int passEnt, int contentMask)
{
    PM_playerTrace(pm, trace, start, mins, maxs, end, passEnt, contentMask);
}

// BG
int  BG_AnimScriptAnimation(playerState_s * /*ps*/, aistateEnum_t /*state*/, scriptAnimMoveTypes_t /*move*/, int /*direction*/) { return 0; }
char BG_CheckProne(int /*clientNum*/, const float * /*origin*/, float /*proneAngle*/, float /*forwardLen*/,
                   float /*backwardLen*/, float * /*mins*/, float * /*maxs*/, bool /*upright*/, bool /*standing*/,
                   bool /*proneStarting*/, unsigned char /*lerpFraction*/, proneCheckType_t /*checkType*/, float /*proneTolerance*/)
{ return 1; }
void BG_CreateXAnim(XAnim_s * /*anims*/, unsigned int /*animIndex*/, const char * /*name*/) {}
void BG_InitWeaponString(int /*weaponIndex*/, const char * /*str*/) {}

// Misc collision / config helpers
struct SimplePlaneIntersection_fwd;
struct adjacencyWinding_t_fwd;
void BuildBrushdAdjacencyWindingForSide(float * /*points*/, int /*numPoints*/,
                                        const void * /*spi*/, int /*numIntersections*/,
                                        void * /*winding*/, int /*sideIndex*/) {}
bool DB_IsXAssetDefault(XAssetType /*type*/, const char * /*name*/) { return true; }
snd_alias_list_t *Com_FindSoundAlias(const char * /*name*/) { return nullptr; }
char *Com_LoadRawTextFile(const char * /*filename*/) { return nullptr; }
void Com_UnloadRawTextFile(char * /*buffer*/) {}
const char *Com_SurfaceTypeToName(int /*surfaceType*/) { return ""; }
int Com_sprintfPos(char *dest, int destSize, int *destPos, const char *fmt, ...)
{
    if (!dest || !destPos) return 0;
    va_list ap;
    va_start(ap, fmt);
    int n = std::vsnprintf(dest + *destPos, destSize - *destPos, fmt, ap);
    va_end(ap);
    if (n < 0) return 0;
    *destPos += n;
    return n;
}
bool Info_Validate(const char * /*s*/) { return true; }
int  I_strcmp(const char *a, const char *b) { return std::strcmp(a, b); }
bool ParseConfigStringToStructCustomSize(unsigned char * /*pStruct*/, const cspField_t * /*pFieldList*/,
                                         int /*iNumFields*/, char * /*pszBuffer*/, int /*iMaxFieldTypes*/,
                                         int  (*)(unsigned char *, const char *, const int) /*parseSpecial*/,
                                         void (*)(unsigned char *, const char *) /*parseStrcpy*/)
{ return false; }

// FS - the read API path
unsigned int FS_FOpenFileByMode(char * /*qpath*/, int *file, fsMode_t /*mode*/) { if (file) *file = 0; return 0; }
unsigned int FS_Read(unsigned char * /*buffer*/, unsigned int /*len*/, int /*file*/) { return 0; }

// Hunk family
void *Hunk_AllocDebugMem(unsigned int size) { return std::malloc(size); }
void  Hunk_FreeDebugMem(void *ptr) { std::free(ptr); }
unsigned char *Hunk_AllocLow(unsigned int size, const char * /*name*/, int /*type*/)
{
    return static_cast<unsigned char *>(std::calloc(1, size));
}
unsigned char *Hunk_AllocLowAlign(unsigned int size, int /*align*/, const char * /*name*/, int /*type*/)
{
    return static_cast<unsigned char *>(std::calloc(1, size));
}
void  Hunk_AddData(int /*type*/, void * /*data*/, void *(*)(int) /*alloc*/) {}
bool  Hunk_DataOnHunk(unsigned char * /*data*/) { return false; }
void *Hunk_FindDataForFile(int /*fileId*/, const char * /*filename*/) { return nullptr; }
char *Hunk_SetDataForFile(int /*type*/, const char * /*name*/, void * /*data*/, void *(*)(int) /*alloc*/) { return nullptr; }

// Stringlist (extra entries pulled by bgame/xanim)
void SL_AddRefToString(unsigned int /*stringValue*/) {}
unsigned int SL_ConvertToLowercase(unsigned int stringValue, unsigned int /*user*/, int /*type*/) { return stringValue; }
unsigned int SL_GetLowercaseString(const char * /*str*/, unsigned int /*user*/) { return 0; }
unsigned int SL_GetStringOfSize(const char * /*str*/, unsigned int /*size*/, unsigned int /*user*/, int /*type*/) { return 0; }
void SL_RemoveRefToStringOfSize(unsigned int /*stringValue*/, unsigned int /*size*/) {}

// Scr_*
void Scr_AddArray() {}
void Scr_AddConstString(unsigned int /*stringValue*/) {}
void Scr_AddFloat(float /*value*/) {}
void Scr_NotifyNum(unsigned int /*entnum*/, unsigned int /*classnum*/, unsigned int /*stringValue*/, unsigned int /*numArgs*/) {}

// XAnim/XModel load
XAnimParts *XAnimLoadFile(char * /*name*/, void *(*)(int) /*Alloc*/) { return nullptr; }
XModel *XModelPrecache_LoadObj(char * /*name*/, void *(*)(int) /*Alloc*/, void *(*)(int) /*AllocColl*/) { return nullptr; }
void XAnim_CalcDeltaForTime(const XAnimParts * /*part*/, float /*time*/, float * /*deltaTrans*/, float4 * /*deltaQuat*/) {}

// Globals
int surfaceTypeSoundListCount = 0;

// =========================================================================
// qcommon batch — com_bsp_load_obj / com_playerprofile / mem_track /
// msg_mp / com_profilemapload / graph cascade.
// =========================================================================

// CL_ helpers
struct ScreenPlacement;
struct Font_s;
struct DevGraph;
struct rectDef_s;
struct MapProfileEntry;
struct clientActive_t;
struct NetField;

float (*CL_GetMapCenter())[3] { static float center[3] = {0,0,0}; return (float (*)[3])&center; }
bool  CL_GetPredictedOriginForServerTime(clientActive_t * /*cl*/, int /*serverTime*/,
                                         float * /*origin*/, float * /*velocity*/, float * /*angles*/,
                                         int * /*bobCycle*/, int * /*movementDir*/) { return false; }

char *BG_GetEntityTypeName(int /*eType*/) { return const_cast<char *>(""); }

// MSG_
const NetFieldList *MSG_GetStateFieldListForEntityType(int /*eType*/) { return nullptr; }

// Com_
char *Com_LoadInfoString(char * /*filename*/, const char * /*fileDesc*/, const char * /*ident*/, char * /*loadBuffer*/) { return nullptr; }

// DevGui
void DevGui_AddGraph(const char * /*name*/, DevGraph * /*graph*/) {}

// FS_
void FS_BuildOSPath(const char * /*base*/, const char * /*game*/, const char * /*qpath*/, char *ospath)
{ if (ospath) ospath[0] = 0; }
int  FS_CreatePath(char * /*OSPath*/) { return 0; }
void FS_FreeFileList(const char ** /*list*/) {}
int  FS_OpenFileOverwrite(char * /*filename*/) { return 0; }
unsigned int FS_Write(const char * /*buffer*/, unsigned int /*len*/, int /*h*/) { return 0; }
int  FS_WriteFileToDir(const char * /*qpath*/, const char * /*dir*/, char * /*buffer*/, unsigned int /*size*/) { return 0; }

// I_str
unsigned char I_CleanChar(unsigned char c) { return c; }

// LiveStorage
void LiveStorage_NewUser() {}
void LiveStorage_ReadStats() {}

// Sys_
const char *Sys_DefaultInstallPath() { return "."; }
void Sys_RemoveDirTree(const char * /*path*/) {}

// UI text-draw helpers (used by ProfLoad overlay)
void UI_DrawText(const ScreenPlacement * /*place*/, const char * /*text*/, int /*maxChars*/,
                 Font_s * /*font*/, float /*x*/, float /*y*/, int /*hAlign*/, int /*vAlign*/,
                 float /*scale*/, const float * /*color*/, int /*style*/) {}
void UI_FillRect(const ScreenPlacement * /*place*/, float /*x*/, float /*y*/, float /*w*/,
                 float /*h*/, int /*hAlign*/, int /*vAlign*/, const float * /*color*/) {}
Font_s *UI_GetFontHandle(const ScreenPlacement * /*place*/, int /*fontIndex*/, float /*scale*/) { return nullptr; }

// Win_LocalizeRef
const char *Win_LocalizeRef(const char *str) { return str; }

// Z_MallocGarbage: GP allocator variant — same as malloc for us.
char *Z_MallocGarbage(int size, const char * /*name*/, int /*type*/)
{
    return static_cast<char *>(std::malloc(static_cast<size_t>(size)));
}

// TRACK_* mem-tracking thunks. Each TRACK_<subsystem>() declares its
// statically-allocated buffers to the mem_track system; with mem-tracking
// off (or stubbed) these are all no-ops.
void TRACK_cl_console() {}
// TRACK_cl_input provided by src/client_mp/cl_input.cpp now.
void TRACK_cl_keys() {}
void TRACK_cl_main() {}
void TRACK_cl_parse() {}
void TRACK_cm_world() {}
void TRACK_com_math() {}
void TRACK_db_registry() {}
void TRACK_devgui() {}
void TRACK_dobj_management() {}
void TRACK_fx_marks() {}
void TRACK_fx_random() {}
void TRACK_fx_system() {}
void TRACK_missile_attractors() {}
void TRACK_msg() {}
void TRACK_phys() {}
void TRACK_q_shared() {}
void TRACK_r_buffers() {}
void TRACK_r_debug() {}
void TRACK_r_dpvs() {}
void TRACK_r_font() {}
void TRACK_r_image_wavelet() {}
void TRACK_r_image() {}
void TRACK_r_init() {}
void TRACK_r_material() {}
void TRACK_r_model() {}
void TRACK_r_rendercmds() {}
void TRACK_r_scene() {}
void TRACK_r_screenshot() {}
void TRACK_r_staticmodelcache() {}
void TRACK_r_water() {}
void TRACK_r_workercmds() {}
void TRACK_rb_backend() {}
void TRACK_rb_drawprofile() {}
void TRACK_rb_showcollision() {}
void TRACK_rb_sky() {}
void TRACK_rb_state() {}
void TRACK_rb_stats() {}
void TRACK_rb_sunshadow() {}
void TRACK_scr_debugger() {}
void TRACK_scr_evaluate() {}
void TRACK_scr_parser() {}
void TRACK_scr_vm() {}
void TRACK_snd_driver() {}
void TRACK_snd() {}
void TRACK_stringed_hooks() {}
// TRACK_sv_game now in server/sv_game.cpp.
// TRACK_sv_main provided by src/server_mp/sv_main_mp.cpp now.
void TRACK_ui_main() {}
void TRACK_ui_shared() {}
// TRACK_ui_utils now in ui/ui_utils.cpp.
void TRACK_win_net() {}
void TRACK_xmodel() {}

// Globals for the new batch.
const dvar_t *cl_shownet = nullptr;
const dvar_t *msg_dumpEnts = nullptr;
const dvar_t *msg_printEntityNums = nullptr;
clientActive_t clients[STATIC_MAX_LOCAL_CLIENTS]{};
huffman_t msgHuff{};
unsigned int msecPerRawTimerTick = 1;
netFieldOrderInfo_t orderInfo{};
alignas(16) static unsigned char sys_info_storage[8192];
void *sys_info = sys_info_storage;

// =========================================================================
// database/devgui/aim_assist cascade.
// =========================================================================

struct Material;
struct centity_s;
struct AimTarget;
struct trajectory_t;
struct XZoneMemory;

// CL/CG/Key
// CL_ClearKeys provided by src/client_mp/cl_input.cpp now.
int  Key_IsDown(int /*localClientNum*/, int /*key*/) { return 0; }
// CG_TraceCapsule provided by src/cgame/cg_world.cpp now.
// CG_DObjGetWorldTagPos provided by src/cgame_mp/cg_ents_mp.cpp now.

// FX visibility
double FX_GetClientVisibility(int /*localClientNum*/, const float * /*origin*/, const float * /*viewOrigin*/) { return 1.0; }

// BG
void BG_EvaluateTrajectory(const trajectory_t * /*tr*/, int /*atTime*/, float *result)
{
    if (result) { result[0] = result[1] = result[2] = 0; }
}

// DevGui
void DevGui_Toggle() {}

// R_ (renderer cmds — these just queue commands; no-op in stub mode)
void R_AddCmdDrawText(const char * /*text*/, int /*max*/, Font_s * /*font*/,
                      float /*x*/, float /*y*/, float /*xScale*/, float /*yScale*/,
                      float /*rotation*/, const float * /*color*/, int /*style*/) {}
void R_AddCmdDrawStretchPic(float /*x*/, float /*y*/, float /*w*/, float /*h*/,
                            float /*s0*/, float /*t0*/, float /*s1*/, float /*t1*/,
                            const float * /*color*/, Material * /*material*/) {}
void R_AddCmdDrawStretchPicRotateXY(float /*x*/, float /*y*/, float /*w*/, float /*h*/,
                                    float /*s0*/, float /*t0*/, float /*s1*/, float /*t1*/,
                                    float /*rotation*/, const float * /*color*/, Material * /*material*/) {}
void R_AddCmdDrawQuadPic(const float (* /*quad*/)[2], const float * /*color*/, Material * /*material*/) {}
int   R_TextHeight(Font_s * /*font*/) { return 0; }
int   R_TextWidth(const char * /*text*/, int /*max*/, Font_s * /*font*/) { return 0; }
void *R_AllocStaticIndexBuffer(IDirect3DIndexBuffer9 ** /*ib*/, int /*size*/) { return nullptr; }
void *R_AllocStaticVertexBuffer(IDirect3DVertexBuffer9 ** /*vb*/, int /*size*/) { return nullptr; }
void  R_FinishStaticIndexBuffer(IDirect3DIndexBuffer9 * /*ib*/) {}
void  R_FinishStaticVertexBuffer(IDirect3DVertexBuffer9 * /*vb*/) {}
void  R_FreeStaticIndexBuffer(IDirect3DIndexBuffer9 * /*ib*/) {}
void  R_FreeStaticVertexBuffer(IDirect3DVertexBuffer9 * /*vb*/) {}
void  R_UnlockIndexBuffer(IDirect3DIndexBuffer9 * /*ib*/) {}
void  R_UnlockVertexBuffer(IDirect3DVertexBuffer9 * /*vb*/) {}

// DB
void DB_LoadXFileData(unsigned char * /*buffer*/, unsigned int /*size*/) {}

// PMem
unsigned char *PMem_Alloc(unsigned int size, unsigned int /*alignment*/, unsigned int /*allocType*/, unsigned int /*name*/)
{
    return static_cast<unsigned char *>(std::calloc(1, size));
}
unsigned int PMem_GetOverAllocatedSize() { return 0; }

// SL
unsigned int SL_GetString(const char * /*str*/, unsigned int /*user*/) { return 0; }
void SL_AddUser(unsigned int /*stringValue*/, unsigned int /*user*/) {}

// Math
float RadiusFromBounds(const float *mins, const float *maxs)
{
    float dx = std::fmax(std::fabs(mins[0]), std::fabs(maxs[0]));
    float dy = std::fmax(std::fabs(mins[1]), std::fabs(maxs[1]));
    float dz = std::fmax(std::fabs(mins[2]), std::fabs(maxs[2]));
    return std::sqrt(dx * dx + dy * dy + dz * dz);
}

// Globals
alignas(16) static unsigned char g_assetNames_storage[8192];
void *g_assetNames = g_assetNames_storage;
alignas(16) static unsigned char varXAssetList_storage[16384];
void *varXAssetList = varXAssetList_storage;

// =========================================================================
// Misc batch — sound/server/cgame_mp/client/game subdirs landing.
// =========================================================================

struct centity_t;
struct level_locals_t;
struct gentity_s;
struct serverStatic_t;
struct client_t;
struct netchan_t;
struct netProfileStream_t;
struct netProfileInfo_t;
struct trDebugLine_t;
struct trDebugString_t;

void CalculateRanks() {}
char CL_IsClientLocal(int /*localClientNum*/) { return 1; }
bool CL_IsPlayerMuted(int /*localClientNum*/, unsigned int /*muteClient*/) { return false; }
void ClientUserinfoChanged(unsigned int /*clientNum*/) {}
void Com_SafeClientDObjFree(unsigned int /*handle*/, int /*localClientNum*/) {}
char *FS_LoadedIwdPureChecksums() { static char empty[1] = {0}; return empty; }
int32_t GScr_GetHeadIconIndex(const char * /*name*/) { return 0; }
int32_t GScr_GetStatusIconIndex(const char * /*name*/) { return 0; }
int  I_stricmpwild(const char *s0, const char *s1) { return strcasecmp(s0 ? s0 : "", s1 ? s1 : ""); }
// IN_IsTalkKeyHeld provided by src/client_mp/cl_input.cpp now.
bool Material_IsDefault(const Material * /*material*/) { return true; }
bool NET_OutOfBandVoiceData(netsrc_t /*sock*/, netadr_t /*adr*/, unsigned char * /*data*/, unsigned int /*len*/) { return false; }
bool Netchan_Transmit(netchan_t * /*chan*/, int /*length*/, char * /*data*/) { return false; }
bool Netchan_TransmitNextFragment(netchan_t * /*chan*/) { return false; }

void NetProf_AddPacket(netProfileStream_t * /*stream*/, int /*size*/, int /*type*/) {}
void NetProf_PrepProfiling(netProfileInfo_t * /*info*/) {}
void NetProf_UpdateStatistics(netProfileStream_t * /*stream*/) {}

void PerpendicularVector(const float *src, float *dst)
{
    int pos = 0;
    float minelem = std::fabs(src[0]);
    if (std::fabs(src[1]) < minelem) { pos = 1; minelem = std::fabs(src[1]); }
    if (std::fabs(src[2]) < minelem) { pos = 2; }
    float tempvec[3] = {0,0,0};
    tempvec[pos] = 1.0f;
    float d = src[0] * tempvec[0] + src[1] * tempvec[1] + src[2] * tempvec[2];
    dst[0] = tempvec[0] - d * src[0];
    dst[1] = tempvec[1] - d * src[1];
    dst[2] = tempvec[2] - d * src[2];
    float l = std::sqrt(dst[0] * dst[0] + dst[1] * dst[1] + dst[2] * dst[2]);
    if (l > 0) { dst[0] /= l; dst[1] /= l; dst[2] /= l; }
}

void R_CopyDebugLines(trDebugLine_t * /*dst*/, int /*dstCap*/, trDebugLine_t * /*src*/, int /*count*/, int /*offset*/) {}
void R_CopyDebugStrings(trDebugString_t * /*dst*/, int /*dstCap*/, trDebugString_t * /*src*/, int /*count*/, int /*offset*/) {}
void R_DebugAlloc(void **out, int size, const char * /*name*/) { if (out) *out = std::calloc(1, size); }
void R_DebugFree(void **p) { if (p && *p) { std::free(*p); *p = nullptr; } }
void R_ShutdownDebug() {}

// Scr_*
void Scr_AddClassField(unsigned int /*classnum*/, char * /*name*/, unsigned int /*offset*/) {}
void Scr_AddInt(int /*value*/) {}
void Scr_AddString(const char * /*value*/) {}
void Scr_Error(const char * /*msg*/) {}
unsigned int Scr_GetConstString(unsigned int /*paramIndex*/) { return 0; }
float Scr_GetFloat(unsigned int /*paramIndex*/) { return 0; }
int   Scr_GetInt(unsigned int /*paramIndex*/) { return 0; }
const char *Scr_GetString(unsigned int /*paramIndex*/) { return ""; }
void Scr_GetGenericField(unsigned char * /*structOut*/, fieldtype_t /*type*/, int /*offset*/) {}
void Scr_SetGenericField(unsigned char * /*structIn*/, fieldtype_t /*type*/, int /*offset*/) {}

// StringTable
void StringTable_GetAsset(const char * /*filename*/, StringTable ** /*outTable*/) {}
const char *StringTable_Lookup(const StringTable * /*table*/, int /*column*/, const char * /*key*/, int /*colCount*/) { return nullptr; }

// SV
// SV_CloseDownload provided by src/server_mp/sv_client_mp.cpp now.
void SV_Download_Clear(client_t * /*cl*/) {}
// SV_DropClient provided by src/server_mp/sv_client_mp.cpp now.
// SV_GetConfigstring provided by src/server_mp/sv_init_mp.cpp now.
// SV_SendClientGameState provided by src/server_mp/sv_client_mp.cpp now.

// Voice
void Voice_IncomingVoiceData(unsigned char /*from*/, unsigned char * /*data*/, int /*len*/) {}
bool Voice_IsClientTalking(unsigned int /*clientNum*/) { return false; }

// Globals — typed where we have the type (since game_public.h pulls
// most of the cgame/game/server headers in).
// cg_entitiesArray provided by src/cgame_mp/cg_main_mp.cpp now.
const dvar_t *cl_showSend = nullptr;
const dvar_t *cl_voice = nullptr;
gentity_s g_entities[1024]{};
level_locals_t level{};
const dvar_t *net_profile = nullptr;
// sv_maxclients provided by src/server_mp/sv_main_mp.cpp now.
// sv_voice provided by src/server_mp/sv_init_mp.cpp now.
// svs provided by src/server_mp/sv_main_mp.cpp now.

// =========================================================================
// Misc-2 batch — sv_game / g_svcmds / cg_consolecmds_mp cascade.
// =========================================================================

struct XBoneInfo;

shellshock_parms_t *BG_GetShellshockParms(unsigned int /*index*/) { return nullptr; }
int  BG_LoadShellShockDvars(const char * /*name*/) { return 0; }
int  BG_SaveShellShockDvars(const char * /*name*/) { return 0; }
void BG_SetShellShockParmsFromDvars(shellshock_parms_t * /*parms*/) {}

bool BoxDistSqrdExceeds(const float * /*center*/, const float * /*mins*/, const float * /*maxs*/, float /*distSqrd*/) { return true; }

void CG_ActionSlotDown_f() {}
void CG_ActionSlotUp_f() {}
// CG_FxSetTestPosition / CG_FxTest provided by src/cgame_mp/cg_view_mp.cpp now.
// CG_IsScoreboardDisplayed provided by src/cgame_mp/cg_scoreboard_mp.cpp now.
void CG_NextWeapon_f() {}
void CG_PrevWeapon_f() {}
// CG_RestartSmokeGrenades provided by src/cgame_mp/cg_main_mp.cpp now.

void CL_AddReliableCommand(int /*localClientNum*/, const char * /*cmd*/) {}

void Com_GetBspFilename(char *filename, unsigned int /*max*/, const char * /*mapname*/)
{ if (filename) filename[0] = 0; }
DObj_s *Com_GetServerDObj(unsigned int /*handle*/) { return nullptr; }
void Com_UnloadSoundAliases(snd_alias_system_t /*sys*/) {}

char *ConcatArgs(int /*start*/) { static char buf[1] = {0}; return buf; }

void DObjCreateSkel(DObj_s * /*obj*/, char * /*partBits*/, int /*controlPartBits*/) {}
unsigned int DObjGetAllocSkelSize(const DObj_s * /*obj*/) { return 0; }
void DObjGetBoneInfo(const DObj_s * /*obj*/, XBoneInfo ** /*info*/) {}
void DObjGetBounds(const DObj_s * /*obj*/, float *mins, float *maxs)
{
    if (mins) mins[0] = mins[1] = mins[2] = 0;
    if (maxs) maxs[0] = maxs[1] = maxs[2] = 0;
}
const XModel *DObjGetModel(const DObj_s * /*obj*/, int /*modelIndex*/) { return nullptr; }
unsigned int DObjGetNumModels(const DObj_s * /*obj*/) { return 0; }
XAnimTree_s *DObjGetTree(const DObj_s * /*obj*/) { return nullptr; }
bool DObjIgnoreCollision(const DObj_s * /*obj*/, char /*modelIndex*/) { return false; }
unsigned int DObjNumBones(const DObj_s * /*obj*/) { return 0; }
void DObjSkelAreBonesUpToDate(const DObj_s * /*obj*/, int * /*partBits*/) {}
bool DObjSkelExists(const DObj_s * /*obj*/, int /*boneIndex*/) { return false; }
bool DObjSkelIsBoneUpToDate(DObj_s * /*obj*/, int /*boneIndex*/) { return false; }

// G_GetEntityTypeName provided by src/game_mp/g_utils_mp.cpp now.
double G_GetFogOpaqueDistSqrd() { return 0; }
int   G_GetSavePersist() { return 0; }
void G_InitGame(int /*serverTime*/, int /*randomSeed*/, int /*restart*/, int /*savegame*/) {}
void G_ResetEntityParsePoint() {}
void G_ShutdownGame(int /*restart*/) {}

void MatrixTransformVector43(const float *in, const float (&m)[4][3], float *out)
{
    out[0] = in[0] * m[0][0] + in[1] * m[1][0] + in[2] * m[2][0] + m[3][0];
    out[1] = in[0] * m[0][1] + in[1] * m[1][1] + in[2] * m[2][1] + m[3][1];
    out[2] = in[0] * m[0][2] + in[1] * m[1][2] + in[2] * m[2][2] + m[3][2];
}

bool NET_IsLocalAddress(netadr_t /*adr*/) { return false; }
bool Scr_IsValidGameType(const char * /*name*/) { return false; }

unsigned int SV_ClipHandleForEntity(const gentity_s * /*ent*/) { return 0; }
// SV_GetMapBaseName provided by src/server_mp/sv_ccmds_mp.cpp now.
void SV_LinkEntity(gentity_s * /*ent*/) {}
// SV_SendServerCommand provided by src/server_mp/sv_main_mp.cpp now.
// SV_SetConfigstring provided by src/server_mp/sv_init_mp.cpp now.

unsigned int Sys_MillisecondsRaw() { return Sys_Milliseconds(); }

uiMenuCommand_t UI_GetActiveMenu(int /*localClientNum*/) { return uiMenuCommand_t{}; }
int  UI_Popup(int /*localClientNum*/, const char * /*ref*/) { return 0; }
char *UI_SafeTranslateString(const char *str) { return const_cast<char *>(str ? str : ""); }

float Vec2DistanceSq(const float *a, const float *b)
{
    float dx = a[0] - b[0], dy = a[1] - b[1];
    return dx * dx + dy * dy;
}

const dvar_t *g_banIPs = nullptr;
const dvar_t *g_dedicated = nullptr;
// sv_gametype provided by src/server_mp/sv_main_mp.cpp now.
// =========================================================================
// Small batch — cl_net_chan_mp / cl_pose_mp / sv_main_pc_mp / g_scr_mover.
// =========================================================================

void AxisToAngles(const float (& /*axis*/)[3][3], float *angles)
{
    angles[0] = angles[1] = angles[2] = 0;
}


// G_DObjUpdate provided by src/game_mp/g_utils_mp.cpp now.
// G_FreeEntity provided by src/game_mp/g_utils_mp.cpp now.

void *I_dmaGetDObjSkel(const DObj_s * /*obj*/) { return nullptr; }

const char *NET_AdrToString(netadr_t /*adr*/) { return ""; }
bool NET_CompareBaseAdr(netadr_t /*a*/, netadr_t /*b*/) { return false; }
bool NET_OutOfBandPrint(netsrc_t /*sock*/, netadr_t /*adr*/, const char * /*data*/) { return false; }
int  NET_StringToAdr(char * /*str*/, netadr_t * /*adr*/) { return 0; }

unsigned int Scr_GetNumParam() { return 0; }
void Scr_GetVector(unsigned int /*paramIndex*/, float *v)
{
    if (v) v[0] = v[1] = v[2] = 0;
}
void Scr_Notify(gentity_s * /*ent*/, unsigned short /*name*/, unsigned int /*paramCount*/) {}
void Scr_ObjectError(const char * /*msg*/) {}
void Scr_ParamError(unsigned int /*paramIndex*/, const char * /*msg*/) {}

// SV_FindClientByAddress provided by src/server_mp/sv_main_mp.cpp now.
// SV_PreGameUserVoice / SV_UserVoice provided by src/server_mp/sv_voice_mp.cpp now.
// SVC_GameCompleteStatus provided by src/server_mp/sv_main_mp.cpp now.

const dvar_t *cl_profileTextHeight = nullptr;
// rcon_password provided by src/server_mp/sv_init_mp.cpp now.

// =========================================================================
// physics + stringed batch.
// =========================================================================

struct Results;
struct Poly;
struct cbrush_t;

// ODE bridge — we don't link ODE, so all of these are no-ops with the
// real ODE signatures (deps/ode/objects.h).
#include <ode/objects.h>
extern "C" {
void *dBodyGetData(dBodyID /*body*/) { return nullptr; }
static const dReal kODEZero[4] = {0, 0, 0, 0};
const dReal *dBodyGetPosition(dBodyID /*body*/) { return kODEZero; }
void dBodyGetPointVel(dBodyID /*body*/, dReal /*px*/, dReal /*py*/, dReal /*pz*/, dVector3 result)
{ if (result) result[0] = result[1] = result[2] = 0; }
void dJointAttach(dJointID /*j*/, dBodyID /*b1*/, dBodyID /*b2*/) {}
dxJoint *dJointCreateContact(dWorldID /*w*/, dJointGroupID /*group*/, const dSurfaceParameters * /*s*/, const dContactGeom * /*c*/) { return nullptr; }
void dNormalize3(dVector3 /*v*/) {}
} // extern "C"

// CG_DebugBox now in cgame/cg_drawtools.cpp.
// CG_DebugLine now in cgame/cg_drawtools.cpp.

void ClosestApproachOfTwoLines(const float * /*p1*/, const float * /*d1*/, const float * /*p2*/, const float * /*d2*/, float *t1, float *t2)
{ if (t1) *t1 = 0; if (t2) *t2 = 0; }

char *I_strupr(char *s)
{
    for (char *p = s; *p; ++p) *p = static_cast<char>(std::toupper(static_cast<unsigned char>(*p)));
    return s;
}

float kisak_random() { return std::rand() / float(RAND_MAX); }

bool ParseConfigStringToStruct(unsigned char * /*pStruct*/, const cspField_t * /*pFieldList*/,
                               int /*iNumFields*/, char * /*pszBuffer*/, int /*iMaxFieldTypes*/,
                               int  (*)(unsigned char *, const char *, const int) /*parseSpecial*/,
                               void (*)(unsigned char *, const char *) /*parseStrcpy*/) { return false; }

// Phys collision helpers — stubs.
bool Phys_AddContactData(Results * /*results*/, float /*depth*/, float * /*normal*/, float * /*pos*/, int /*type*/) { return false; }
unsigned int Phys_ClipLineSegmentAgainstPlane(float * /*p1*/, float * /*p2*/, const float * /*plane*/) { return 0; }
unsigned int Phys_ClipLineSegmentAgainstPoly(const float * /*plane*/, const float (* /*verts*/)[3], unsigned int /*numVerts*/,
                                             float * /*p1*/, float * /*p2*/) { return 0; }
void Phys_DrawPoly(const Poly * /*poly*/, const float * /*color*/) {}
int  Phys_GetPlaneForTriangle2(const float (* /*tri*/)[3], const float * /*normal*/, float /*dist*/, float * /*planeOut*/) { return 0; }
int  Phys_GetSurfaceFlagsFromBrush(const cbrush_t * /*brush*/, unsigned int /*faceIndex*/) { return 0; }
void Phys_GetWindingForBrushFace2(const cbrush_t * /*brush*/, unsigned int /*faceIndex*/, Poly * /*polyOut*/, int /*flag*/, const float (* /*axialPlanes*/)[4]) {}
void Phys_ProjectFaceOntoFaceAndClip(const float * /*planeA*/, const Poly * /*polyA*/, const Poly * /*polyB*/, int /*type*/, Results * /*results*/, float * /*extra*/) {}

void Vec3Negate(const float *in, float *out) { out[0] = -in[0]; out[1] = -in[1]; out[2] = -in[2]; }
void Vec4Copy(const float *in, float *out) { out[0] = in[0]; out[1] = in[1]; out[2] = in[2]; out[3] = in[3]; }

// Globals
const dvar_t *phys_contact_cfm = nullptr;
const dvar_t *phys_contact_erp = nullptr;
const dvar_t *phys_drawCollisionWorld = nullptr;
const dvar_t *phys_drawcontacts = nullptr;
const dvar_t *phys_jitterMaxMass = nullptr;
const dvar_t *phys_noIslands = nullptr;
PhysGlob physGlob{};

// =========================================================================
// cgame_mp small batch — cg_draw_net_mp cascade.
// =========================================================================

struct usercmd_s;

// CG_DrawBigDevString now in cgame/cg_drawtools.cpp.
void SV_ClearPacketAnalysis() {}
int  SV_GetClientSnapshotPing(int /*clientNum*/, char /*ignoreSnapshotMs*/) { return 0; }
bool SV_NewPacketAnalysisReady() { return false; }

void UI_DrawHandlePic(const ScreenPlacement * /*place*/, float /*x*/, float /*y*/, float /*w*/, float /*h*/,
                      int /*hAlign*/, int /*vAlign*/, const float * /*color*/, Material * /*material*/) {}
int   UI_TextHeight(Font_s * /*font*/, float /*scale*/) { return 0; }
int   UI_TextWidth(const char * /*text*/, int /*max*/, Font_s * /*font*/, float /*scale*/) { return 0; }

// cg_drawLagometer provided by src/cgame_mp/cg_main_mp.cpp now.
// cg_nopredict provided by src/cgame_mp/cg_main_mp.cpp now.
// cg_packetAnalysisClient provided by src/cgame_mp/cg_main_mp.cpp now.
// cg_packetAnalysisEntTextScale provided by src/cgame_mp/cg_main_mp.cpp now.
// cg_packetAnalysisEntTextY provided by src/cgame_mp/cg_main_mp.cpp now.
// cg_packetAnalysisTextScale provided by src/cgame_mp/cg_main_mp.cpp now.
// cg_packetAnalysisTextY provided by src/cgame_mp/cg_main_mp.cpp now.
// cg_synchronousClients provided by src/cgame_mp/cg_main_mp.cpp now.
const dvar_t *net_showprofile = nullptr;

// g_bitsSent / g_currentSnapshot* — typed via server_mp.h that's already
// pulled in by stub deps.
int g_bitsSent[64][13]{};
int g_currentSnapshotPerEntity[64][1024]{};
unsigned char g_currentSnapshotFieldsPerEntity[64][1024]{};
unsigned char g_currentSnapshotPlayerStateFields[64]{};

// =========================================================================
// cgame batch (cg_camerashake/compass/info/drawtools/playerstate/pose_utils
// + ui_localvars/ui_utils).
// =========================================================================

struct statement_s;
struct itemDef_s;
struct windowDef_t;
struct rectDef_s_fwd;
// StanceState already defined elsewhere — no forward decl needed.

// CG_CloseScriptMenu provided by src/cgame_mp/cg_servercmds_mp.cpp now.
// CG_EntityEvent now in cgame/cg_event.cpp.
// CG_FadeHudMenu provided by src/cgame_mp/cg_newDraw_mp.cpp now.
void CG_HoldBreathInit(cg_s * /*cg*/) {}
// CG_MenuShowNotify provided by src/cgame_mp/cg_servercmds_mp.cpp now.
// CG_ObjectiveIcon / CG_ResetLowHealthOverlay provided by src/cgame_mp/cg_newDraw_mp.cpp now.
// CG_SetEquippedOffHand provided by src/cgame/offhandweapons.cpp now.

void CL_DrawText(const ScreenPlacement * /*place*/, const char * /*text*/, int /*maxChars*/,
                 Font_s * /*font*/, float /*x*/, float /*y*/, int /*hAlign*/, int /*vAlign*/,
                 float /*xScale*/, float /*yScale*/, const float * /*color*/, int /*style*/) {}
void CL_DrawTextRotate(const ScreenPlacement * /*place*/, const char * /*text*/, int /*maxChars*/,
                       Font_s * /*font*/, float /*x*/, float /*y*/, float /*rotation*/,
                       int /*hAlign*/, int /*vAlign*/, float /*xScale*/, float /*yScale*/,
                       const float * /*color*/, int /*style*/) {}
bool CL_IsServerLoadingMap() { return false; }
bool CL_IsWaitingOnServerToLoadMap(int /*localClientNum*/) { return false; }
// CL_SetStance provided by src/client_mp/cl_input.cpp now.
void CL_SetWaitingOnServerToLoadMap(int /*localClientNum*/, bool /*waiting*/) {}

float DB_GetLoadedFraction() { return 1.0f; }

bool IsExpressionTrue(int /*localClientNum*/, const statement_s * /*expr*/) { return false; }
const rectDef_s *Item_GetTextRect(int /*localClientNum*/, const itemDef_s * /*item*/) { return nullptr; }

float kisak_crandom() { return (std::rand() / float(RAND_MAX)) * 2.0f - 1.0f; }

// SCR_UpdateLoadScreen provided by src/client_mp/cl_scrn_mp.cpp now.
int  String_Parse(const char ** /*p*/, char * /*out*/, int /*outSize*/) { return 0; }
void UI_CloseAllMenus(int /*localClientNum*/) {}
void UI_DrawMapLevelshot(int /*localClientNum*/) {}
void UI_DrawTextNoSnap(const ScreenPlacement * /*place*/, const char * /*text*/, int /*maxChars*/,
                       Font_s * /*font*/, float /*x*/, float /*y*/, int /*hAlign*/, int /*vAlign*/,
                       float /*scale*/, const float * /*color*/, int /*style*/) {}

float Vec2NormalizeTo(const float *in, float *out)
{
    float l = std::sqrt(in[0] * in[0] + in[1] * in[1]);
    if (l > 0) { out[0] = in[0] / l; out[1] = in[1] / l; }
    else       { out[0] = 0; out[1] = 0; }
    return l;
}

bool Window_IsVisible(int /*localClientNum*/, const windowDef_t * /*window*/) { return false; }

void YawVectors2D(float yaw, float *forward, float *right)
{
    float rad = yaw * (3.14159265358979323846f / 180.0f);
    float c = std::cos(rad), s = std::sin(rad);
    if (forward) { forward[0] = c; forward[1] = s; }
    if (right)   { right[0]   = s; right[1]   = -c; }
}

const dvar_t *bg_viewKickMax = nullptr;
const dvar_t *bg_viewKickMin = nullptr;
const dvar_t *bg_viewKickScale = nullptr;
// cg_hudDamageIconTime provided by src/cgame_mp/cg_main_mp.cpp now.
// hud_fade_compass provided by src/cgame_mp/cg_newDraw_mp.cpp now.
const dvar_t *uiscript_debug = nullptr;
BOOL g_waitingForServer = 0;

// =========================================================================
// cl_cgame_mp + cg_compassfriendlies_mp + cg_visionsets cascade.
// =========================================================================

struct MemoryFile;

unsigned int BG_GetViewmodelWeaponIndex(const playerState_s * /*ps*/) { return 0; }
WeaponDef *BG_GetWeaponDef(unsigned int /*weaponIndex*/) { return nullptr; }

// CG_ArchiveState provided by src/cgame_mp/cg_newDraw_mp.cpp now.
// CG_Init provided by src/cgame_mp/cg_main_mp.cpp now.
// CG_RegisterSounds provided by src/cgame_mp/cg_main_mp.cpp now.
// CG_Shutdown provided by src/cgame_mp/cg_main_mp.cpp now.

char CL_AnyLocalClientsRunning() { return 0; }
void CL_DisconnectError(char * /*msg*/) {}
void CL_ParseServerMessage(netsrc_t /*sock*/, msg_t * /*msg*/) {}
void CL_SystemInfoChanged(int /*localClientNum*/) {}
bool CL_WasMapAlreadyLoaded() { return false; }

void CM_LinkWorld() {}
unsigned char ColorIndex(unsigned char /*c*/) { return 7; }
snd_alias_t *Com_PickSoundAlias(const char * /*name*/) { return nullptr; }
void Com_TouchMemory() {}

void Con_ClearNotify(int /*localClientNum*/) {}
void Con_Close(int /*localClientNum*/) {}
void Con_InitMessageBuffer() {}
void Con_TimeJumped(int /*localClientNum*/, int /*time*/) {}
void Con_TimeNudged(int /*localClientNum*/, int /*delta*/) {}

void DB_EnumXAssets(XAssetType /*type*/, void (*)(XAssetHeader, void*) /*cb*/, void * /*ctx*/, bool /*loaded*/) {}
void DevGui_AddCommand(const char * /*name*/, char * /*menu*/) {}
void FX_Archive(int /*localClientNum*/, MemoryFile * /*memFile*/) {}

const char *Info_ValueForKey(const char * /*s*/, const char * /*key*/) { return ""; }

// LargeLocal: real definition in universal/com_memory.h; provide impls.
// Upstream is a per-frame scratch area; use plain malloc/free.
LargeLocal::LargeLocal(int sizeParam)
{
    size = sizeParam;
    // KISAKHACK: startPos holds a 64-bit malloc'd pointer in a 32-bit
    // slot. Caller-side OK because we just hand it back via GetBuf();
    // documented under docs/RISKS.md.
    startPos = (int)(uintptr_t)std::malloc(sizeParam);
}
LargeLocal::~LargeLocal() { std::free((void*)(uintptr_t)startPos); }
unsigned char *LargeLocal::GetBuf() { return (unsigned char *)(uintptr_t)startPos; }

void R_AddCmdDrawStretchPicFlipST(float /*x*/, float /*y*/, float /*w*/, float /*h*/,
                                  float /*s0*/, float /*t0*/, float /*s1*/, float /*t1*/,
                                  const float * /*color*/, Material * /*material*/) {}
void R_AddCmdDrawStretchPicRotateST(float /*x*/, float /*y*/, float /*w*/, float /*h*/,
                                    float /*s0*/, float /*t0*/, float /*s1*/, float /*t1*/,
                                    float /*rotationS*/, float /*rotationT*/,
                                    const float * /*color*/, Material * /*material*/) {}
void R_ArchiveFogState(MemoryFile * /*memFile*/) {}
void R_EndRegistration() {}
void R_LoadWorld(char * /*name*/, int * /*checksum*/, int /*flag*/) {}
void R_RenderScene(const refdef_s * /*refdef*/) {}
void R_UpdateTeamColors(int /*localClientNum*/, const float * /*color1*/, const float * /*color2*/) {}

char *SEH_SafeTranslateString(char *str) { return str ? str : (char*)""; }
const char *SEH_StringEd_GetString(const char *str) { return str ? str : ""; }

void UI_CloseAll(int /*localClientNum*/) {}
char *UI_ReplaceConversionString(char *src, const char * /*replace*/) { return src; }

const dvar_t *cl_activeAction = nullptr;
const dvar_t *cl_freezeDemo = nullptr;
BOOL cl_serverLoadingMap = 0;
const dvar_t *cl_showServerCommands = nullptr;
const dvar_t *cl_showTimeDelta = nullptr;
const dvar_t *loc_warnings = nullptr;
const dvar_t *loc_warningsAsErrors = nullptr;
const dvar_t *nextdemo = nullptr;

// sv_archive_mp cascade.
struct SnapshotInfo_s;
struct clientState_s;
struct archivedEntity_s;

void MSG_WriteDeltaArchivedEntity(SnapshotInfo_s * /*info*/, msg_t * /*msg*/, int /*time*/, archivedEntity_s * /*from*/, archivedEntity_s * /*to*/, int /*force*/) {}
void MSG_WriteDeltaClient(SnapshotInfo_s * /*info*/, msg_t * /*msg*/, int /*time*/, clientState_s * /*from*/, clientState_s * /*to*/, int /*force*/) {}
void MSG_WriteDeltaPlayerstate(SnapshotInfo_s * /*info*/, msg_t * /*msg*/, int /*time*/, const playerState_s * /*from*/, const playerState_s * /*to*/) {}
void MSG_WriteEntityIndex(SnapshotInfo_s * /*info*/, msg_t * /*msg*/, int /*newnum*/, int /*indexBits*/) {}
void SV_PacketDataIsNotNetworkData(int /*type*/, const msg_t * /*msg*/) {}
void SV_PacketDataIsUnknown(int /*type*/, const msg_t * /*msg*/) {}
void SV_ResetPacketData(int /*type*/, const msg_t * /*msg*/) {}
serverStaticHeader_t svsHeader{};
int svsHeaderValid = 0;

// bullet + ui_gameinfo_mp cascade.
struct BulletFireParams;
struct BulletTraceResults;
struct AntilagClientStore;

char BG_AdvanceTrace(BulletFireParams * /*p*/, BulletTraceResults * /*r*/, float /*dist*/) { return 0; }
double BG_GetSurfacePenetrationDepth(const WeaponDef * /*w*/, unsigned int /*surfType*/) { return 0; }
unsigned int BG_GetWeaponIndex(const WeaponDef * /*w*/) { return 0; }
unsigned char DirToByte(const float * /*dir*/) { return 0; }
int  FS_GetFileList(const char * /*path*/, const char * /*ext*/, FsListBehavior_e /*behavior*/, char *listbuf, int /*size*/)
{
    if (listbuf) listbuf[0] = 0;
    return 0;
}
void G_AntiLag_RestoreClientPos(AntilagClientStore * /*store*/) {}
void G_AntiLagRewindClientPos(int /*clientNum*/, AntilagClientStore * /*store*/) {}
// G_CheckHitTriggerDamage provided by src/game_mp/g_trigger_mp.cpp now.
void G_Damage(gentity_s * /*targ*/, gentity_s * /*inflictor*/, gentity_s * /*attacker*/, float * /*dir*/,
              float * /*point*/, int /*damage*/, int /*dflags*/, int /*mod*/, unsigned int /*weapon*/,
              hitLocation_t /*hitLoc*/, unsigned int /*timeOffset*/, unsigned int /*modelIndex*/, int /*partGroup*/) {}
void G_LocationalTraceAllowChildren(trace_t *trace, float * /*start*/, float * /*end*/, int /*passEnt*/, int /*contentMask*/, unsigned char * /*priority*/)
{ if (trace) { std::memset(trace, 0, sizeof(*trace)); trace->fraction = 1.0f; } }
// G_TempEntity provided by src/game_mp/g_utils_mp.cpp now.
bool OnSameTeam(gentity_s * /*ent1*/, gentity_s * /*ent2*/) { return false; }

const dvar_t *bullet_penetrationEnabled = nullptr;
const dvar_t *bullet_penetrationMinFxDist = nullptr;
const dvar_t *g_debugLocDamage = nullptr;
// sv_clientSideBullets provided by src/server_mp/sv_main_mp.cpp now.
sharedUiInfo_t sharedUiInfo{};

// cg_event cascade.
struct FxEffectDef;
struct snd_alias_list_t;

int  BG_WeaponIsClipOnly(unsigned int /*weaponIndex*/) { return 0; }
void ByteToDir(unsigned int /*b*/, float *dir)
{ if (dir) { dir[0] = 1; dir[1] = 0; dir[2] = 0; } }
void CG_BulletHitClientEvent(int /*localClientNum*/, int /*ent*/, float * /*start*/, float * /*end*/,
                             unsigned int /*type*/, int /*hitLoc*/, int /*partGroup*/) {}
void CG_BulletHitEvent(int /*localClientNum*/, int /*sourceEnt*/, unsigned int /*surfType*/,
                       unsigned int /*hitEnt*/, float * /*start*/, float * /*end*/,
                       const float * /*normal*/, unsigned int /*flags*/, int /*hitLoc*/,
                       unsigned char /*priority*/, int /*partGroup*/, short /*recoilIndex*/) {}
// CG_CalcEntityLerpPositions provided by src/cgame_mp/cg_ents_mp.cpp now.
// CG_DrawScoreboard_GetTeamColorIndex provided by src/cgame_mp/cg_scoreboard_mp.cpp now.
void CG_EjectWeaponBrass(int /*localClientNum*/, const entityState_s * /*es*/, int /*time*/) {}
void CG_FireWeapon(int /*localClientNum*/, centity_s * /*cent*/, int /*mode*/, unsigned short /*weapon*/,
                   unsigned int /*surfType*/, const playerState_s * /*ps*/) {}
void CG_ImpactEffectForWeapon(unsigned int /*weapon*/, unsigned int /*surfType*/, char /*type*/,
                              const FxEffectDef ** /*effect*/, snd_alias_list_t ** /*sound*/) {}
void CG_MeleeBloodEvent(int /*localClientNum*/, const centity_s * /*cent*/) {}
void CG_OutOfAmmoChange(int /*localClientNum*/) {}
// CG_PlayClientSoundAlias provided by src/cgame_mp/cg_main_mp.cpp now.
// CG_PlayEntitySoundAlias provided by src/cgame_mp/cg_main_mp.cpp now.
// CG_PlaySoundAlias provided by src/cgame_mp/cg_main_mp.cpp now.
// CG_PlaySoundAliasAsMasterByName provided by src/cgame_mp/cg_main_mp.cpp now.
// CG_PlaySoundAliasByName provided by src/cgame_mp/cg_main_mp.cpp now.
// CG_PrepOffHand provided by src/cgame/offhandweapons.cpp now.
// CG_PriorityCenterPrint provided by src/cgame_mp/cg_draw_mp.cpp now.
void CG_SelectWeaponIndex(int /*localClientNum*/, unsigned int /*weaponIndex*/) {}
// CG_StopSoundAlias provided by src/cgame_mp/cg_main_mp.cpp now.
// CG_StopSoundsOnEnt provided by src/cgame_mp/cg_main_mp.cpp now.
// CG_SwitchOffHandCmd / CG_UseOffHand provided by src/cgame/offhandweapons.cpp now.

void CL_DeathMessagePrint(int /*localClientNum*/, char * /*killerName*/, char /*killerColor*/,
                          char * /*victimName*/, char /*victimColor*/, Material * /*icon*/,
                          float /*duration*/, float /*scale*/, bool /*friendlyFire*/) {}
// CL_GetClientName provided by src/client_mp/cl_ui_mp.cpp now.

void DynEntCl_ExplosionEvent(int /*localClientNum*/, bool /*ent*/, float * /*org*/, float /*r*/, float /*rs*/,
                             float * /*norm*/, float /*duration*/, int /*type*/, int /*flags*/) {}
void DynEntCl_JitterEvent(int /*localClientNum*/, float * /*pos*/, float /*radius*/, float /*amp*/, float /*duration*/, float /*frequency*/) {}
void DynEntCl_MeleeEvent(int /*localClientNum*/, int /*entityNum*/) {}

void FX_PlayBoltedEffect(int /*localClientNum*/, const FxEffectDef * /*effect*/, int /*time*/, unsigned int /*entityNum*/, unsigned int /*boneIndex*/) {}
void FX_PlayOrientedEffect(int /*localClientNum*/, const FxEffectDef * /*effect*/, int /*time*/, const float * /*origin*/, const float (* /*axis*/)[3]) {}

void Scr_SetString(unsigned short * /*ptr*/, unsigned int /*stringValue*/) {}

const dvar_t *bg_fallDamageMaxHeight = nullptr;
const dvar_t *bg_fallDamageMinHeight = nullptr;
// cg_debugEvents provided by src/cgame_mp/cg_main_mp.cpp now.
// cg_footsteps provided by src/cgame_mp/cg_main_mp.cpp now.
// cgMedia + cgsArray: typed via cg_local_mp.h (already in include chain).
// cgMedia / cgsArray provided by src/cgame_mp/cg_main_mp.cpp now.

// Con_InitChannels now in client/con_channels.cpp.
// Con_IsChannelVisible now in client/con_channels.cpp.
// Con_WriteFilterConfigString now in client/con_channels.cpp.
void Key_WriteBindings(int /*localClientNum*/, int /*f*/) {}
char *SEH_LocalizeTextMessage(const char *src, const char * /*context*/, msgLocErrType_t /*err*/) { return const_cast<char *>(src); }
void SEH_UpdateLanguageInfo() {}
const char *StringTable_GetColumnValueForRow(const StringTable * /*table*/, int /*row*/, int /*col*/) { return ""; }
// ProfLoad_Init now in qcommon/com_profilemapload.cpp.
// ProfLoad_IsActive now in qcommon/com_profilemapload.cpp.
// ProfLoad_Deactivate now in qcommon/com_profilemapload.cpp.

// =========================================================================
// Net + msg. Stubs.
// =========================================================================

void NET_Init() {}
int  NET_GetClientPacket(netadr_t * /*from*/, msg_t * /*msg*/) { return 0; }
int  NET_GetLoopPacket(netsrc_t /*sock*/, netadr_t * /*from*/, msg_t * /*msg*/) { return 0; }
int  NET_GetServerPacket(netadr_t * /*from*/, msg_t * /*msg*/) { return 0; }
void NET_RestartDebug() {}
void NET_ShutdownDebug() {}
void NET_Sleep(int /*msec*/) {}
void Netchan_Init(short /*port*/) {}
// MSG_Init now in qcommon/msg_mp.cpp.

// =========================================================================
// Hunk + PMem.
// =========================================================================

void Hunk_Clear() {}
void Hunk_ClearTempMemory() {}
void Hunk_ClearTempMemoryHigh() {}
void Hunk_InitDebugMemory() {}
void Hunk_ResetDebugMem() {}
void Hunk_ShutdownDebugMemory() {}
void PMem_Init() {}
void PMem_BeginAlloc(const char * /*name*/, unsigned int /*allocType*/) {}
void PMem_EndAlloc(const char * /*name*/, unsigned int /*allocType*/) {}

// =========================================================================
// Build number — upstream auto-generates this on Windows. We ship a static
// 0 build (BUILD_NUMBER macro is in src/buildnumber.h).
// =========================================================================

char *getBuildNumber()
{
    static char buf[8] = "0";
    return buf;
}

int getBuildNumberAsInt() { return 0; }

// =========================================================================
// Globals expected by other backbone files.
// =========================================================================

bgs_t *bgs = nullptr;
clientUIActive_t clientUIActives[STATIC_MAX_LOCAL_CLIENTS]{};
int com_fileAccessed;
const dvar_s *fs_basepath = nullptr;
const dvar_s *fs_debug = nullptr;
int fs_fakeChkSum;
const dvar_s *fs_gameDirVar = nullptr;
int fs_numServerIwds;
searchpath_s *fs_searchpaths = nullptr;
const char *fs_serverIwdNames[1024]{};
int fs_serverIwds[1024]{};
const dvar_t *loc_language = nullptr;
// sv provided by src/server_mp/sv_main_mp.cpp now.
// updateScreenCalled provided by src/client_mp/cl_scrn_mp.cpp now.

// fx_randomTable: 507-entry deterministic random table used by the
// EffectsCore particle system. Real upstream fills this once at startup
// from a fixed seed so spawn positions/velocities are reproducible
// across the network. Storage is const to match the upstream declaration
// `extern const float fx_randomTable[507]` in fx_system.h; the static
// initializer below mutates through a non-const alias.
const float fx_randomTable[507] = {};
int fx_serverVisClient = -1;

// =========================================================================
// Ragdoll / DynEntity / DObj / Phys / CG cascade — landed with ragdoll +
// DynEntity_coll. These will be replaced as the real subsystems come in.
// =========================================================================

// --- Math helpers — real impls -------------------------------------------
void AxisTranspose(const float (&in)[3][3], float (&out)[3][3])
{
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j)
            out[i][j] = in[j][i];
}

void MatrixMultiply43(const float (&a)[4][3], const float (&b)[4][3], float (&out)[4][3])
{
    // 4x3 affine matrix multiply (row 3 is translation, rows 0-2 the rotation).
    for (int i = 0; i < 3; ++i) {
        out[0][i] = a[0][0] * b[0][i] + a[0][1] * b[1][i] + a[0][2] * b[2][i];
        out[1][i] = a[1][0] * b[0][i] + a[1][1] * b[1][i] + a[1][2] * b[2][i];
        out[2][i] = a[2][0] * b[0][i] + a[2][1] * b[1][i] + a[2][2] * b[2][i];
        out[3][i] = a[3][0] * b[0][i] + a[3][1] * b[1][i] + a[3][2] * b[2][i] + b[3][i];
    }
}

void MatrixTransposeTransformVector43(const float *in, const float (&m)[4][3], float *out)
{
    float t[3] = { in[0] - m[3][0], in[1] - m[3][1], in[2] - m[3][2] };
    out[0] = t[0] * m[0][0] + t[1] * m[0][1] + t[2] * m[0][2];
    out[1] = t[0] * m[1][0] + t[1] * m[1][1] + t[2] * m[1][2];
    out[2] = t[0] * m[2][0] + t[1] * m[2][1] + t[2] * m[2][2];
}

void QuatToAxis(const float *quat, float (&axis)[3][3])
{
    UnitQuatToAxis(quat, axis);
}

float Q_acos(float c)
{
    if (c > 1.0f) c = 1.0f;
    if (c < -1.0f) c = -1.0f;
    return std::acos(c);
}

void Vec3AddScalar(const float *a, float s, float *sum)
{
    sum[0] = a[0] + s;
    sum[1] = a[1] + s;
    sum[2] = a[2] + s;
}

void Vec3Rotate(const float *in, const float (&m)[3][3], float *out)
{
    out[0] = in[0] * m[0][0] + in[1] * m[1][0] + in[2] * m[2][0];
    out[1] = in[0] * m[0][1] + in[1] * m[1][1] + in[2] * m[2][1];
    out[2] = in[0] * m[0][2] + in[1] * m[1][2] + in[2] * m[2][2];
}

float Vec4Dot(const float *a, const float *b)
{
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2] + a[3] * b[3];
}

float Vec4LengthSq(const float *v)
{
    return v[0] * v[0] + v[1] * v[1] + v[2] * v[2] + v[3] * v[3];
}

void Vec4Lerp(const float *from, const float *to, float frac, float *result)
{
    result[0] = from[0] + frac * (to[0] - from[0]);
    result[1] = from[1] + frac * (to[1] - from[1]);
    result[2] = from[2] + frac * (to[2] - from[2]);
    result[3] = from[3] + frac * (to[3] - from[3]);
}

float flrand(float fmin, float fmax)
{
    // Float in [min, max). Same deterministic seed pattern as fx_randomTable.
    static unsigned int s = 0xDEADBEEFu;
    s = s * 1103515245u + 12345u;
    float r = ((s >> 8) & 0xFFFFFF) / float(0x1000000);
    return fmin + r * (fmax - fmin);
}

// --- Opaque subsystem stubs ---------------------------------------------
// All of these return zero/null. As the matching subsystems land, each
// block here gets deleted and the real impl takes over.

// Forward-decl every opaque type the stubs touch. The enums
// DynEntityCollType / DynEntityDrawType / DynEntityType and PhysWorld are
// already defined in headers we include above.
struct cpose_t;
struct DObj_s;
struct DObjAnimMat;
struct DynEntityDef;
struct DynEntityClient;
struct DynEntityColl;
struct DynEntityPose;
struct DynEntityProps;
struct XModel;
struct PhysMass;
struct PhysContact;
struct PhysPreset;
struct dxBody;
struct dxJointHinge;
struct dxJointAMotor;
struct dxJointBall;

// CG_
// CG_GetPose / CG_DObjCalcBone provided by src/cgame_mp/cg_ents_mp.cpp now.
// CG_DrawStringExt now in cgame/cg_drawtools.cpp.

// Com / DObj
DObj_s *Com_GetClientDObj(unsigned int /*handle*/, int /*localClientNum*/) { return nullptr; }
// DObjDisplayAnim now provided by xanim/xanim.cpp.
void DObjGetBasePoseMatrix(const DObj_s * /*obj*/, unsigned char /*boneIndex*/, DObjAnimMat * /*outMat*/) {}
int  DObjGetBoneIndex(const DObj_s * /*obj*/, unsigned int /*name*/, unsigned char *index)
{
    if (index) *index = 255;
    return 0;
}
DObjAnimMat *DObjGetRotTransArray(const DObj_s * /*obj*/) { return nullptr; }
char DObjSetSkelRotTransIndex(DObj_s * /*obj*/, const int * /*partBits*/, int /*boneIndex*/) { return 0; }

// DynEnt
DynEntityClient *DynEnt_GetClientEntity(unsigned short /*id*/, DynEntityDrawType /*draw*/) { return nullptr; }
DynEntityColl *DynEnt_GetEntityColl(DynEntityCollType /*coll*/, unsigned short /*id*/) { return nullptr; }
unsigned short DynEnt_GetEntityCount(DynEntityCollType /*coll*/) { return 0; }
const DynEntityDef *DynEnt_GetEntityDef(unsigned short /*id*/, DynEntityDrawType /*draw*/) { return nullptr; }
const DynEntityProps *DynEnt_GetEntityProps(DynEntityType /*t*/) { return nullptr; }
unsigned short DynEnt_GetId(const DynEntityDef * /*def*/, DynEntityDrawType /*draw*/) { return 0; }

// XModelGetBounds now provided by xanim/xmodel.cpp.

// R
unsigned int R_GetLocalClientNum() { return 0; }

// SL (stringlist)
unsigned int SL_FindString(const char * /*str*/) { return 0; }

// Phys_ family — all stubs. PhysWorld is passed by-value as an opaque
// `struct PhysWorld` so we just take it; nothing happens.
void Phys_ObjAddForce(PhysWorld /*w*/, dxBody * /*b*/, float * /*force*/, const float * /*pos*/) {}
void Phys_ObjAddGeomBox(PhysWorld /*w*/, dxBody * /*b*/, const float * /*min*/, const float * /*max*/) {}
void Phys_ObjAddGeomBrushModel(PhysWorld /*w*/, dxBody * /*b*/, unsigned short /*brushIndex*/, const PhysMass * /*mass*/) {}
void Phys_ObjAddGeomCapsule(PhysWorld /*w*/, dxBody * /*b*/, int /*axis*/, float /*radius*/, float /*halfHeight*/, const float * /*center*/) {}
void Phys_ObjAddGeomCylinder(PhysWorld /*w*/, dxBody * /*b*/, const float * /*min*/, const float * /*max*/) {}
void Phys_ObjAddGeomCylinderDirection(PhysWorld /*w*/, dxBody * /*b*/, int /*axis*/, float /*r*/, float /*h*/, const float * /*c*/) {}
dxBody *Phys_ObjCreate(PhysWorld /*w*/, float * /*pos*/, float * /*rot*/, float * /*inertia*/, const PhysPreset * /*preset*/) { return nullptr; }
void Phys_ObjDestroy(PhysWorld /*w*/, dxBody * /*b*/) {}
void Phys_ObjGetCenterOfMass(dxBody * /*b*/, float *out) { if (out) out[0] = out[1] = out[2] = 0; }
void Phys_ObjGetInterpolatedState(PhysWorld /*w*/, dxBody * /*b*/, float * /*pos*/, float * /*rot*/) {}
void Phys_ObjGetPosition(dxBody * /*b*/, float * /*pos*/, float (* /*rot*/)[3]) {}
bool Phys_ObjIsAsleep(dxBody * /*b*/) { return true; }
void Phys_ObjSetAngularVelocityRaw(dxBody * /*b*/, float * /*omega*/) {}
void Phys_ObjSetCollisionFromXModel(const XModel * /*model*/, PhysWorld /*w*/, dxBody * /*b*/) {}
void Phys_ObjSetContactCentroid(dxBody * /*b*/, const float * /*centroid*/) {}
void Phys_ObjSetOrientation(PhysWorld /*w*/, dxBody * /*b*/, const float * /*pos*/, const float * /*rot*/) {}
void Phys_ObjSetVelocity(dxBody * /*b*/, float * /*vel*/) {}
void Phys_RunToTime(int /*physClock*/, PhysWorld /*w*/, int /*time*/) {}
void Phys_SetCollisionCallback(PhysWorld /*w*/, void (* /*cb*/)()) {}
// Phys_AddCollisionContact now in physics/phys_contacts.cpp.
dxJointAMotor *Phys_CreateAngularMotor(PhysWorld /*w*/, dxBody * /*a*/, dxBody * /*b*/, unsigned int /*flags*/,
                                       const float (* /*axes*/)[3], const float * /*p1*/, const float * /*p2*/,
                                       const float * /*p3*/, const float * /*p4*/) { return nullptr; }
dxJointBall *Phys_CreateBallAndSocket(PhysWorld /*w*/, dxBody * /*a*/, dxBody * /*b*/, float * /*anchor*/) { return nullptr; }
dxJointHinge *Phys_CreateHinge(PhysWorld /*w*/, dxBody * /*a*/, dxBody * /*b*/, float * /*anchor*/, float * /*axis*/,
                               float /*lo*/, float /*hi*/, float /*friction*/, float /*motor*/) { return nullptr; }
void Phys_JointDestroy(PhysWorld /*w*/, dxJointHinge * /*j*/) {}
void Phys_SetAngularMotorParams(PhysWorld /*w*/, dxJointAMotor * /*j*/, const float * /*p1*/, const float * /*p2*/,
                                const float * /*p3*/, const float * /*p4*/) {}
void Phys_SetHingeParams(PhysWorld /*w*/, dxJointHinge * /*j*/, float /*lo*/, float /*hi*/, float /*friction*/, float /*motor*/) {}

// Globals from the cg / script-place layer.
// game_public.h transitively pulls cgame_mp.h which exposes the real
// cg_s type, so we can zero-construct directly.
// cgArray provided by src/cgame_mp/cg_main_mp.cpp now.
// cg_paused provided by src/cgame_mp/cg_main_mp.cpp now.
// scrPlaceFull now in client/screen_placement.cpp.

// =========================================================================
// Script VM cascade — landed with scr_animtree / scr_main / scr_memorytree.
// The real upstream impls live in scr_variable / scr_vm / scr_parser /
// scr_stringlist / scr_compiler / scr_evaluate, which we haven't ported
// yet. Stubs return zero/empty; engine boots into a state where scripts
// silently no-op.
// =========================================================================

// --- Variable system stubs (scr_variable.cpp) -----------------------------
// scr_variable.h / scr_compiler.h pull in VariableValueInternal_u, Vartype_t,
// VariableValue, sval_u, PrecacheEntry — see includes above. XAnim_s /
// XAnimParts / HunkUser are forward-only.
struct XAnim_s;
struct XAnimParts;
struct HunkUser;

unsigned int FindObject(unsigned int /*id*/) { return 0; }
unsigned int GetObject(unsigned int /*id*/) { return 0; }
unsigned int FindVariable(unsigned int /*parentId*/, unsigned int /*name*/) { return 0; }
unsigned int GetVariable(unsigned int /*parentId*/, unsigned int /*name*/) { return 0; }
unsigned int FindArrayVariable(unsigned int /*parentId*/, int /*intValue*/) { return 0; }
unsigned int GetArrayVariable(unsigned int /*parentId*/, unsigned int /*value*/) { return 0; }
unsigned int GetNewVariable(unsigned int /*parentId*/, unsigned int /*value*/) { return 0; }
unsigned int GetArray(unsigned int /*id*/) { return 0; }
unsigned int GetArraySize(unsigned int /*id*/) { return 0; }
unsigned int FindFirstSibling(unsigned int /*id*/) { return 0; }
unsigned int FindNextSibling(unsigned int /*id*/) { return 0; }
unsigned int GetVariableName(unsigned int /*id*/) { return 0; }
Vartype_t    GetValueType(unsigned int /*id*/) { return VAR_UNDEFINED; }
VariableValueInternal_u *GetVariableValueAddress(unsigned int /*id*/) { return nullptr; }
void         RemoveRefToObject(unsigned int /*id*/) {}
void         RemoveVariable(unsigned int /*parentId*/, unsigned int /*name*/) {}
void         ClearObject(unsigned int /*parentId*/) {}
void         SetVariableValue(unsigned int /*id*/, VariableValue * /*value*/) {}

// --- Stringlist (scr_stringlist.cpp) --------------------------------------
const char *SL_ConvertToString(unsigned int /*stringValue*/) { return ""; }
const char *SL_DebugConvertToString(unsigned int /*stringValue*/) { return ""; }
unsigned int SL_GetLowercaseString_(const char * /*str*/, unsigned int /*user*/, int /*type*/) { return 0; }
unsigned int SL_GetString_(const char * /*str*/, unsigned int /*user*/, int /*type*/) { return 0; }
void SL_RemoveRefToString(unsigned int /*stringValue*/) {}
void SL_ShutdownSystem(unsigned int /*user*/) {}
void SL_TransferRefToUser(unsigned int /*stringValue*/, unsigned int /*user*/) {}

// --- Scr_* lifecycle + helpers --------------------------------------------
char *Scr_AddSourceBuffer(const char * /*filename*/, char * /*extFilename*/,
                          const char * /*codePos*/, bool /*archive*/) { return nullptr; }
unsigned int Scr_AllocArray() { return 0; }
void Scr_ClearErrorMessage() {}
unsigned int Scr_CreateCanonicalFilename(const char * /*filename*/) { return 0; }
void Scr_EndLoadEvaluate() {}
VariableValue Scr_EvalVariable(unsigned int /*id*/) { VariableValue v{}; return v; }
void Scr_InitAllocNode() {}
void Scr_InitDebugger() {}
void Scr_InitDebuggerMain() {}
void Scr_InitEvaluate() {}
void Scr_InitOpcodeLookup() {}
void Scr_ShutdownDebugger() {}
void Scr_ShutdownDebuggerMain() {}
void Scr_ShutdownEvaluate() {}
void Scr_ShutdownOpcodeLookup() {}

// --- Compiler/parser ------------------------------------------------------
void CompileError(unsigned int /*sourcePos*/, const char * /*msg*/, ...) {}
void CompileError2(char * /*codePos*/, const char * /*msg*/, ...) {}
void ScriptCompile(sval_u /*val*/, unsigned int /*fileId*/, unsigned int /*scriptId*/,
                   PrecacheEntry * /*entries*/, int /*entriesCount*/) {}
void ScriptParse(sval_u * /*parseData*/, unsigned char /*user*/) {}

// --- TempMalloc / Hunk debug ----------------------------------------------
char *TempMalloc(unsigned int len) { return static_cast<char *>(std::malloc(len)); }
void TempMemoryReset(HunkUser * /*user*/) {}
unsigned char *Hunk_AllocXAnimPrecache(unsigned int size)
{
    return static_cast<unsigned char *>(std::calloc(1, size));
}
void Hunk_CheckTempMemoryClear() {}
void Hunk_CheckTempMemoryHighClear() {}
HunkUser *Hunk_UserCreate(int /*maxSize*/, const char * /*name*/, bool /*fixed*/,
                          bool /*tempMem*/, int /*type*/) { return nullptr; }
void Hunk_UserDestroy(HunkUser * /*user*/) {}

// --- XAnim now provided by xanim/xanim.cpp -------------------------------
// (XAnimBlend, XAnimCreate, XAnimCreateAnims, XAnimPrecache,
//  XAnimSetupSyncNodes, XAnimInit, XAnimShutdown)

// --- Misc -----------------------------------------------------------------
bool I_iscsym(int c) { return std::isalnum(c) || c == '_'; }
// ProfLoad_Begin now in qcommon/com_profilemapload.cpp.
// ProfLoad_End now in qcommon/com_profilemapload.cpp.

// --- Global storage for scr*Pub structures -------------------------------
// All these pub structs have their definitions reached via the script
// headers included at the top, so we can zero-construct them properly.
scrCompilePub_t  scrCompilePub{};
scrParserPub_t   scrParserPub{};
scrVarPub_t      scrVarPub{};
static scrVarDebugPub_t scrVarDebugPub_storage{};
scrVarDebugPub_t *scrVarDebugPub = &scrVarDebugPub_storage;
scrVmPub_t       scrVmPub{};
bool g_loadedImpureScript = false;

namespace {
struct FxRandomTableInit {
    FxRandomTableInit() {
        float *table = const_cast<float *>(fx_randomTable);
        unsigned int s = 0xCAFEF00Du;
        for (int i = 0; i < 507; ++i) {
            s = s * 1103515245u + 12345u;
            table[i] = ((s >> 8) & 0xFFFFFF) / float(0x800000) - 1.0f;
        }
    }
};
[[maybe_unused]] FxRandomTableInit g_fxRandomTableInit;
}

void FX_RandomDir(int seed, float *dir)
{
    // Use fx_randomTable as the seed source. Pull three entries from the
    // table at staggered offsets, treat as a 3D vector, normalize.
    const int n = 507;
    int i0 = ((seed * 1664525) >> 0) % n; if (i0 < 0) i0 += n;
    int i1 = ((seed * 1013904223) >> 4) % n; if (i1 < 0) i1 += n;
    int i2 = ((seed * 22695477) >> 8) % n; if (i2 < 0) i2 += n;
    dir[0] = fx_randomTable[i0];
    dir[1] = fx_randomTable[i1];
    dir[2] = fx_randomTable[i2];
    float l = std::sqrt(dir[0] * dir[0] + dir[1] * dir[1] + dir[2] * dir[2]);
    if (l > 0) { dir[0] /= l; dir[1] /= l; dir[2] /= l; }
    else       { dir[0] = 1; dir[1] = 0; dir[2] = 0; }
}

// === CGAME draw/debug/reticles/offhandweapons satellites ===========================

// CG_ShouldDrawHud provided by src/cgame_mp/cg_newDraw_mp.cpp now.
void R_TrackStatistics(trStatistics_t *) {}
void FX_DrawMarkProfile(int, void (*)(const char *, float *), float *) {}
int  PMem_GetFreeAmount() { return 0; }
void Phys_DrawDebugText(const ScreenPlacement *) {}
int  Scr_GetStringUsage() { return 0; }
void Phys_GetPerformance(float *t, int *a, int *b) { if (t) *t = 0.f; if (a) *a = 0; if (b) *b = 0; }
int  SND_GetSoundOverlay(snd_overlay_type_t, snd_overlay_info_t *, int, int *) { return 0; }
unsigned int Scr_GetNumScriptVars() { return 0u; }
void BG_GetSpreadForWeapon(const playerState_s *, const WeaponDef *, float *lo, float *hi)
{ if (lo) *lo = 0.f; if (hi) *hi = 0.f; }
snd_entchannel_info_t *SND_GetEntChannelName(int) { return nullptr; }
void CG_UpdateViewModelPose(const DObj_s *, int) {}
bool UI_ShouldDrawCrosshair() { return false; }
unsigned int Scr_GetNumScriptThreads() { return 0u; }
int  CG_PlayerTurretWeaponIdx(int) { return 0; }
void Phys_PerformanceEndFrame() {}
void AimAssist_DrawDebugOverlay(unsigned int) {}
int  BG_GetFirstEquippedOffhand(const playerState_s *, int) { return 0; }
int  BG_GetFirstAvailableOffhand(const playerState_s *, int) { return 0; }
// CG_Flashbanged provided by src/cgame/cg_shellshock.cpp now.
void FX_DrawProfile(int, void (*)(char *), float *) {}
int  R_PickMaterial(int, const float *, const float *, char *, char *, char *, unsigned int) { return 0; }
uint32_t BG_GetNumWeapons() { return 0u; }
int32_t  BG_ClipForWeapon(uint32_t) { return 0; }
void     FX_Beam_Add(FxBeam *) {}
void     FX_PostLight_Add(FxPostLight *) {}
// CG_DObjGetWorldBoneMatrix provided by src/cgame_mp/cg_ents_mp.cpp now.

// cg_laserEndOffset provided by src/cgame_mp/cg_main_mp.cpp now.
// cg_laserFlarePct provided by src/cgame_mp/cg_main_mp.cpp now.
// cg_laserLight provided by src/cgame_mp/cg_main_mp.cpp now.
// cg_laserLightBeginOffset provided by src/cgame_mp/cg_main_mp.cpp now.
// cg_laserLightBodyTweak provided by src/cgame_mp/cg_main_mp.cpp now.
// cg_laserLightEndOffset provided by src/cgame_mp/cg_main_mp.cpp now.
// cg_laserLightRadius provided by src/cgame_mp/cg_main_mp.cpp now.
// cg_laserRadius provided by src/cgame_mp/cg_main_mp.cpp now.
// cg_laserRange provided by src/cgame_mp/cg_main_mp.cpp now.
// cg_laserRangePlayer provided by src/cgame_mp/cg_main_mp.cpp now.
// === cg_shellshock satellites ====================================================

void MatrixMultiply(const mat3x3 & /*a*/, const mat3x3 & /*b*/, mat3x3 &out)
{
    out[0][0] = 1.f; out[0][1] = 0.f; out[0][2] = 0.f;
    out[1][0] = 0.f; out[1][1] = 1.f; out[1][2] = 0.f;
    out[2][0] = 0.f; out[2][1] = 0.f; out[2][2] = 1.f;
}
void AxisCopy(const mat3x3 &in, mat3x3 &out)
{
    out[0][0] = in[0][0]; out[0][1] = in[0][1]; out[0][2] = in[0][2];
    out[1][0] = in[1][0]; out[1][1] = in[1][1]; out[1][2] = in[1][2];
    out[2][0] = in[2][0]; out[2][1] = in[2][1]; out[2][2] = in[2][2];
}
void R_AddCmdSaveScreen(unsigned int) {}
void R_AddCmdSaveScreenSection(float, float, float, float, unsigned int) {}
void R_AddCmdBlendSavedScreenShockBlurred(int, float, float, float, float, unsigned int) {}
void R_AddCmdBlendSavedScreenShockFlashed(float, float, float, float, float, float) {}
int  SND_PlaySoundAlias(const snd_alias_t *, SndEntHandle, const float *, int, snd_alias_system_t) { return 0; }
int  SND_PlayBlendedSoundAliases(const snd_alias_t *, const snd_alias_t *, float, float, SndEntHandle, const float *, int, snd_alias_system_t) { return 0; }
void SND_SetChannelVolumes(int, const float *, int) {}
void SND_DeactivateChannelVolumes(int, int) {}
void SND_SetEnvironmentEffects(int, const char *, float, float, int) {}
void SND_DeactivateEnvironmentEffects(int, int) {}
int32_t CL_GetLocalClientActiveCount() { return 0; }
// CG_GetLocalClientViewParams provided by src/cgame_mp/cg_view_mp.cpp now.

// === cg_effects_load_obj satellites ==============================================

const FxEffectDef *FX_Register(const char *) { return nullptr; }
int  compare_impact_files(const char **, const char **) { return 0; }
int  Com_SurfaceTypeFromName(const char *) { return 0; }
unsigned int *Hunk_AllocateTempMemory(int size, const char * /*name*/)
{
    return static_cast<unsigned int *>(std::calloc((size + sizeof(unsigned int) - 1) / sizeof(unsigned int), sizeof(unsigned int)));
}
unsigned int Hunk_AllocateTempMemoryHigh(int /*size*/, const char * /*name*/) { return 0u; }

// === cg_draw_indicators satellites ===============================================

void UI_FillRectPhysical(float, float, float, float, const float *) {}

const dvar_t *bg_maxGrenadeIndicatorSpeed   = nullptr;
// cg_hudDamageIconHeight provided by src/cgame_mp/cg_main_mp.cpp now.
// cg_hudDamageIconInScope provided by src/cgame_mp/cg_main_mp.cpp now.
// cg_hudDamageIconOffset provided by src/cgame_mp/cg_main_mp.cpp now.
// cg_hudDamageIconWidth provided by src/cgame_mp/cg_main_mp.cpp now.
// cg_hudGrenadeIconEnabledFlash provided by src/cgame_mp/cg_main_mp.cpp now.
// cg_hudGrenadeIconHeight provided by src/cgame_mp/cg_main_mp.cpp now.
// cg_hudGrenadeIconInScope provided by src/cgame_mp/cg_main_mp.cpp now.
// cg_hudGrenadeIconMaxHeight provided by src/cgame_mp/cg_main_mp.cpp now.
// cg_hudGrenadeIconMaxRangeFlash provided by src/cgame_mp/cg_main_mp.cpp now.
// cg_hudGrenadeIconMaxRangeFrag provided by src/cgame_mp/cg_main_mp.cpp now.
// cg_hudGrenadeIconOffset provided by src/cgame_mp/cg_main_mp.cpp now.
// cg_hudGrenadeIconWidth provided by src/cgame_mp/cg_main_mp.cpp now.
// cg_hudGrenadePointerHeight provided by src/cgame_mp/cg_main_mp.cpp now.
// cg_hudGrenadePointerPivot provided by src/cgame_mp/cg_main_mp.cpp now.
// cg_hudGrenadePointerPulseFreq provided by src/cgame_mp/cg_main_mp.cpp now.
// cg_hudGrenadePointerPulseMax provided by src/cgame_mp/cg_main_mp.cpp now.
// cg_hudGrenadePointerPulseMin provided by src/cgame_mp/cg_main_mp.cpp now.
// cg_hudGrenadePointerWidth provided by src/cgame_mp/cg_main_mp.cpp now.
// === cg_hudelem satellites =======================================================

void  FX_SpriteAdd(FxSprite *) {}
float Vec2Distance(const float *a, const float *b)
{
    float dx = a[0] - b[0], dy = a[1] - b[1];
    return std::sqrt(dx * dx + dy * dy);
}
int   SEH_PrintStrlen(const char *s) { return s ? static_cast<int>(std::strlen(s)) : 0; }
void  BG_LerpHudColors(const hudelem_s *, int, hudelem_color_t *) {}
int   compare_hudelems(const void *, const void *) { return 0; }
bool  UI_AnyMenuVisible(int) { return false; }
// CG_ServerMaterialName provided by src/cgame_mp/cg_newDraw_mp.cpp now.
double R_NormalizedTextScale(Font_s *, float scale) { return scale; }
void  CL_PlayTextFXPulseSounds(uint32_t, int, int, int, int, int, int *) {}
// CG_GetViewAxisProjections provided by src/cgame_mp/cg_draw_mp.cpp now.
void  CL_DrawTextPhysicalWithEffects(const char *, int, Font_s *, float, float, float, float,
                                     const float *, int, const float *, Material *, Material *,
                                     int, int, int, int) {}
int   UI_GetKeyBindingLocalizedString(int, const char *, char *out)
{
    if (out) out[0] = '\0';
    return 0;
}

// === cg_localents satellites =====================================================

void CG_DrawTracer(const float *, const float *, const refdef_s *) {}
// cg_tracerLength provided by src/cgame_mp/cg_main_mp.cpp now.
// === cg_world satellites =========================================================

void   DObjLock(DObj_s *) {}
void   DObjUnlock(DObj_s *) {}
double DObjGetRadius(const DObj_s *) { return 0.0; }
int    DObjGetContents(const DObj_s *) { return 0; }
int    DObjHasContents(DObj_s *, int) { return 0; }
void   DObjGeomTraceline(DObj_s *, float *, float *const, int, DObjTrace_s *) {}
void   DObjGeomTracelinePartBits(DObj_s *, int, int *) {}
DObjAnimMat *CG_DObjCalcPose(const cpose_t *, const DObj_s *, int32_t *) { return nullptr; }
void   DynEntCl_ClipMoveTrace(const moveclip_t *, trace_t *) {}
void   CM_PointTraceStaticModels(trace_t *, const float *, const float *, int) {}

// === cg_predict_mp satellites ====================================================

// CL_SendCmd provided by src/client_mp/cl_input.cpp now.
bool BG_CanItemBeGrabbed(const entityState_s *, const playerState_s *, int) { return false; }
bool BG_PlayerTouchesItem(const playerState_s *, const entityState_s *, int) { return false; }
bool BG_PlayerHasRoomForEntAllAmmoTypes(const entityState_s *, const playerState_s *) { return false; }
void BG_PlayerStateToEntityState(playerState_s *, entityState_s *, int, uint8_t) {}
void PM_UpdateViewAngles(playerState_s *, float, usercmd_s *, uint8_t) {}
void Pmove(pmove_t *) {}
// CG_AdjustPositionForMover provided by src/cgame_mp/cg_ents_mp.cpp now.
// CG_ExtractTransPlayerState provided by src/cgame_mp/cg_snapshot_mp.cpp now.

// cg_errorDecay provided by src/cgame_mp/cg_main_mp.cpp now.
// cg_predictItems provided by src/cgame_mp/cg_main_mp.cpp now.
// cg_showmiss provided by src/cgame_mp/cg_main_mp.cpp now.
// cg_viewZSmoothingMax provided by src/cgame_mp/cg_main_mp.cpp now.
// cg_viewZSmoothingMin provided by src/cgame_mp/cg_main_mp.cpp now.
// cg_viewZSmoothingTime provided by src/cgame_mp/cg_main_mp.cpp now.
// === cg_client_side_effects_mp satellites ========================================

FxEffect *FX_SpawnOrientedEffect(int, const FxEffectDef *, int, const float *, const float (*)[3], uint32_t) { return nullptr; }

// === cg_snapshot_mp satellites ===================================================

// CG_InitView provided by src/cgame_mp/cg_view_mp.cpp now.
// CG_ClearUnion provided by src/cgame_mp/cg_ents_mp.cpp now.
// CG_GameMessage provided by src/cgame_mp/cg_main_mp.cpp now.
void   R_UnlinkEntity(unsigned int, unsigned int) {}
void   AimAssist_Setup(int) {}
void   R_InitSceneData(int) {}
XModel *R_RegisterModel(const char *) { return nullptr; }
void   SND_SetListener(int, int, const float *, const float (*)[3]) {}
void   SND_FadeAllSounds(float, int) {}
// CG_UpdatePlayerDObj provided by src/cgame_mp/cg_players_mp.cpp now.
// CG_UpdateViewOffset provided by src/cgame_mp/cg_view_mp.cpp now.
void   FX_MarkEntDetachAll(int, int) {}
// CG_ResetPlayerEntity provided by src/cgame_mp/cg_players_mp.cpp now.
void   FX_ThroughWithEffect(int, FxEffect *) {}
// CG_mg42_PreControllers provided by src/cgame_mp/cg_ents_mp.cpp now.
void   CG_UpdateHandViewmodels(int, XModel *) {}
// CG_Player_PreControllers provided by src/cgame_mp/cg_ents_mp.cpp now.
// CG_SetFrameInterpolation provided by src/cgame_mp/cg_ents_mp.cpp now.
void   CG_UpdateWeaponViewmodels(int) {}
// CG_UpdateBModelWorldBounds provided by src/cgame_mp/cg_ents_mp.cpp now.
// CG_ExecuteNewServerCommands provided by src/cgame_mp/cg_servercmds_mp.cpp now.
// CG_CheckOpenWaitingScriptMenu provided by src/cgame_mp/cg_servercmds_mp.cpp now.
void   AimAssist_ClearEntityReference(int, int) {}

// cg_entityOriginArray provided by src/cgame_mp/cg_main_mp.cpp now.
// cg_fs_debug provided by src/cgame_mp/cg_main_mp.cpp now.
// === cg_view_mp satellites =======================================================

float BG_GetSpeed(const playerState_s *, int) { return 0.f; }
void  FX_RewindTo(int, int) {}
void  R_ClearScene(unsigned int) {}
// CG_DrawActive provided by src/cgame_mp/cg_draw_mp.cpp now.
float BG_GetBobCycle(const playerState_s *) { return 0.f; }
void  FX_BeginUpdate(int) {}
void  Key_AddCatcher(int, int) {}
void  R_SetLodOrigin(const refdef_s *) {}
void  CG_VehGunnerPOV(int, float *o, float *a)
{
    if (o) { o[0] = o[1] = o[2] = 0; }
    if (a) { a[0] = a[1] = a[2] = 0; }
}
void  CG_AddViewWeapon(int) {}
// CG_ProcessEntity provided by src/cgame_mp/cg_ents_mp.cpp now.
void  FX_FillUpdateCmd(int, FxCmd *) {}
void  AddLeanToPosition(float *, float, float, float, float) {}
// CG_DObjUpdateInfo provided by src/cgame_mp/cg_ents_mp.cpp now.
void  Key_RemoveCatcher(int, int) {}
double R_GetFarPlaneDist() { return 0.0; }
// CG_AddPacketEntity provided by src/cgame_mp/cg_ents_mp.cpp now.
bool  Key_IsCatcherActive(int, int) { return false; }
// CG_AddPacketEntities provided by src/cgame_mp/cg_ents_mp.cpp now.
// CL_GetMenuBlurRadius provided by src/client_mp/cl_scrn_mp.cpp now.
void  FX_SetNextUpdateTime(int, int) {}
void  CL_ResetSkeletonCache(int) {}
void  BG_CalculateViewAngles(viewState_t *, float *) {}
void  FX_SetNextUpdateCamera(int, const refdef_s *, float) {}
float BG_GetVerticalBobFactor(const playerState_s *, float, float, float) { return 0.f; }
int32_t BG_IsAimDownSightWeapon(uint32_t) { return 0; }
void  CG_UpdateViewWeaponAnim(int) {}
void  CG_VehSphereCoordsToPos(float, float, float, float *out) { if (out) { out[0] = out[1] = out[2] = 0; } }
bool  G_ExitAfterConnectPaths() { return false; }
void  R_AddCmdProjectionSet2D() {}
void  R_UpdateSpotLightEffect(FxCmd *) {}
// CG_DObjGetWorldTagMatrix provided by src/cgame_mp/cg_ents_mp.cpp now.
bool  CG_VehLocalClientDriving(int) { return false; }
void  R_UpdateRemainingEffects(FxCmd *) {}
float BG_GetHorizontalBobFactor(const playerState_s *, float, float, float) { return 0.f; }
// CG_ProcessClientNoteTracks provided by src/cgame_mp/cg_ents_mp.cpp now.
void  R_UpdateNonDependentEffects(FxCmd *) {}
int32_t CG_VehLocalClientVehicleSlot(int) { return -1; }
void  AimAssist_UpdateScreenTargets(int, const float *, const float *, float, float) {}
bool  CG_VehLocalClientUsingVehicle(int) { return false; }
int32_t AimAssist_GetScreenTargetCount(int) { return 0; }
void  CG_VehSeatOriginForLocalClient(int, float *out) { if (out) { out[0] = out[1] = out[2] = 0; } }
int32_t AimAssist_GetScreenTargetEntity(int, uint32_t) { return -1; }
int32_t CL_LocalActiveIndexFromClientNum(int) { return 0; }
// CL_Input provided by src/client_mp/cl_input.cpp now.
// CG_Draw2D provided by src/cgame_mp/cg_draw_mp.cpp now.
void  R_SyncGpu(int (*)(unsigned long long)) {}

const dvar_t *bg_bobMax           = nullptr;
// cgDC provided by src/cgame_mp/cg_main_mp.cpp now.
// cg_drawShellshock provided by src/cgame_mp/cg_main_mp.cpp now.
// cg_dumpAnims provided by src/cgame_mp/cg_main_mp.cpp now.
// cg_fov provided by src/cgame_mp/cg_main_mp.cpp now.
// cg_fovMin provided by src/cgame_mp/cg_main_mp.cpp now.
// cg_fovScale provided by src/cgame_mp/cg_main_mp.cpp now.
// cg_thirdPerson provided by src/cgame_mp/cg_main_mp.cpp now.
// cg_thirdPersonAngle provided by src/cgame_mp/cg_main_mp.cpp now.
// cg_thirdPersonRange provided by src/cgame_mp/cg_main_mp.cpp now.
const dvar_t *vehDebugClient        = nullptr;
const dvar_t *vehDriverViewDist     = nullptr;
const dvar_t *vehDriverViewFocusRange = nullptr;
const dvar_t *vehDriverViewHeightMax  = nullptr;

// === cg_players_mp satellites ====================================================

float RotationToYaw(const float *) { return 0.f; }
float vectosignedyaw(const float *) { return 0.f; }
void  YawToAxis(float, mat3x3 &axis)
{
    axis[0][0] = 1.f; axis[0][1] = 0.f; axis[0][2] = 0.f;
    axis[1][0] = 0.f; axis[1][1] = 1.f; axis[1][2] = 0.f;
    axis[2][0] = 0.f; axis[2][1] = 0.f; axis[2][2] = 1.f;
}
void  R_AddDObjToScene(const DObj_s *, const cpose_t *, unsigned int, unsigned int, float *, float) {}
void  BG_PlayerAnimation(int, const entityState_s *, clientInfo_t *) {}
void  CG_AddPlayerWeapon(int, const GfxScaledPlacement *, const playerState_s *, centity_s *, int) {}
bool  BG_IsKnifeMeleeAnim(const clientInfo_t *, int) { return false; }
void  BG_UpdatePlayerDObj(int, DObj_s *, entityState_s *, clientInfo_t *, int) {}
void  FX_MarkEntUpdateBegin(FxMarkDObjUpdateContext *, DObj_s *, bool, uint16_t) {}
void  FX_MarkEntUpdateEnd(FxMarkDObjUpdateContext *, int, int, DObj_s *, bool, uint16_t) {}
// CG_GetWeaponAttachBone provided by src/cgame_mp/cg_main_mp.cpp now.

// cg_connectionIconSize provided by src/cgame_mp/cg_main_mp.cpp now.
// cg_constantSizeHeadIcons provided by src/cgame_mp/cg_main_mp.cpp now.
// cg_debugPosition provided by src/cgame_mp/cg_main_mp.cpp now.
// cg_drawWVisDebug provided by src/cgame_mp/cg_main_mp.cpp now.
// cg_headIconMinScreenRadius provided by src/cgame_mp/cg_main_mp.cpp now.
// cg_scriptIconSize provided by src/cgame_mp/cg_main_mp.cpp now.
// cg_voiceIconSize provided by src/cgame_mp/cg_main_mp.cpp now.
// cg_youInKillCamSize provided by src/cgame_mp/cg_main_mp.cpp now.
// === cg_servercmds_mp satellites =================================================

void R_SwitchFog(unsigned int, int, int) {}
void FX_InitSystem(int) {}
void Phys_Shutdown() {}
void SND_StopMusic(int) {}
// CG_StartAmbient provided by src/cgame_mp/cg_main_mp.cpp now.
int  Load_ScriptMenu(int, const char *, int) { return 0; }
void CG_RegisterItems(int) {}
void Menus_ShowByName(const UiContext *, const char *) {}
void CG_SetupWeaponDef(int) {}
void CL_ParseMapCenter(int) {}
void DynEntCl_Shutdown(int) {}
void FX_KillAllEffects(int) {}
void FX_ShutdownSystem(int) {}
// CG_BoldGameMessage provided by src/cgame_mp/cg_main_mp.cpp now.
void R_SetFogFromServer(float, unsigned char, unsigned char, unsigned char, float) {}
void SND_PlayMusicAlias(int, const snd_alias_t *, bool, snd_alias_system_t) {}
void UI_CloseInGameMenu(int) {}
int  UI_PopupScriptMenu(int, const char *, bool) { return 0; }
// CG_ClearCenterPrint provided by src/cgame_mp/cg_draw_mp.cpp now.
void LiveStorage_SetStat(int, int, unsigned int) {}
void R_InitPrimaryLights(GfxLight *) {}
void CL_ResetPlayerMuting(uint32_t) {}
void DynEntCl_DestroyEvent(int, uint16_t, DynEntityCollType, const float *, const float *) {}
void DynEntCl_InitEntities(int) {}
void UI_ClosePopupScriptMenu(int, bool) {}
// CG_PlayClientSoundAliasByName provided by src/cgame_mp/cg_main_mp.cpp now.
// CG_StopClientSoundAliasByName provided by src/cgame_mp/cg_main_mp.cpp now.
// CG_ShouldPlaySoundOnLocalClient provided by src/cgame_mp/cg_main_mp.cpp now.
void R_ClearShadowedPrimaryLightHistory(int) {}
char *UI_GetMapDisplayNameFromPartialLoadNameMatch(const char *, int *) { return nullptr; }
void Phys_Init() {}

// cg_chatHeight provided by src/cgame_mp/cg_main_mp.cpp now.
// cg_chatTime provided by src/cgame_mp/cg_main_mp.cpp now.
// cg_teamChatsOnly provided by src/cgame_mp/cg_main_mp.cpp now.
// === cg_draw_mp satellites =======================================================

void Con_DrawSay(int, int, int) {}
void Menu_PaintAll(UiContext *) {}
void Con_DrawErrors(int, int, int, float) {}
void Menus_HideByName(const UiContext *, const char *) {}
int32_t PM_GetSprintLeft(const playerState_s *, int32_t) { return 0; }
void Menus_CloseByName(UiContext *, const char *) {}
int32_t BG_GetMaxSprintTime(const playerState_s *) { return 0; }
// CG_CalcPlayerHealth provided by src/cgame_mp/cg_newDraw_mp.cpp now.
void CL_DrawTextPhysical(const char *, int, Font_s *, float, float, float, float, const float *, int) {}
void Con_DrawMiniConsole(int, int, int, float) {}
const char *UI_GetTopActiveMenuName(int) { return ""; }
// CG_CheckPlayerForLowAmmo / CG_CheckPlayerForLowClip provided by src/cgame_mp/cg_newDraw_mp.cpp now.
void BG_AssertOffhandIndexOrNone(uint32_t) {}
void Vec4Mul(const float *a, const float *b, float *p)
{
    if (a && b && p) { p[0] = a[0]*b[0]; p[1] = a[1]*b[1]; p[2] = a[2]*b[2]; p[3] = a[3]*b[3]; }
}

// cg_centertime provided by src/cgame_mp/cg_main_mp.cpp now.
// cg_descriptiveText provided by src/cgame_mp/cg_main_mp.cpp now.
// cg_draw2D provided by src/cgame_mp/cg_main_mp.cpp now.
// cg_drawCrosshairNames provided by src/cgame_mp/cg_main_mp.cpp now.
// cg_drawFriendlyNames provided by src/cgame_mp/cg_main_mp.cpp now.
// cg_drawSpectatorMessages provided by src/cgame_mp/cg_main_mp.cpp now.
// cg_drawThroughWalls provided by src/cgame_mp/cg_main_mp.cpp now.
// cg_enemyNameFadeOut provided by src/cgame_mp/cg_main_mp.cpp now.
// cg_friendlyNameFadeOut provided by src/cgame_mp/cg_main_mp.cpp now.
// cg_hudChatIntermissionPosition provided by src/cgame_mp/cg_main_mp.cpp now.
// cg_hudChatPosition provided by src/cgame_mp/cg_main_mp.cpp now.
// cg_hudSayPosition provided by src/cgame_mp/cg_main_mp.cpp now.
// cg_hudVotePosition provided by src/cgame_mp/cg_main_mp.cpp now.
// cg_minicon provided by src/cgame_mp/cg_main_mp.cpp now.
// cg_overheadIconSize provided by src/cgame_mp/cg_main_mp.cpp now.
// cg_overheadNamesFarDist provided by src/cgame_mp/cg_main_mp.cpp now.
// cg_overheadNamesFarScale provided by src/cgame_mp/cg_main_mp.cpp now.
// cg_overheadNamesFont provided by src/cgame_mp/cg_main_mp.cpp now.
// cg_overheadNamesGlow provided by src/cgame_mp/cg_main_mp.cpp now.
// cg_overheadNamesMaxDist provided by src/cgame_mp/cg_main_mp.cpp now.
// cg_overheadNamesNearDist provided by src/cgame_mp/cg_main_mp.cpp now.
// cg_overheadNamesSize provided by src/cgame_mp/cg_main_mp.cpp now.
// cg_overheadRankSize provided by src/cgame_mp/cg_main_mp.cpp now.
// debugOverlay provided by src/cgame_mp/cg_main_mp.cpp now.
// hud_fade_* + hud_health_startpulse_injured provided by src/cgame_mp/cg_newDraw_mp.cpp now.
const dvar_t *ui_showEndOfGame            = nullptr;

// === cg_ents_mp satellites =======================================================

const char *DObjGetName(const DObj_s *) { return ""; }
void  Vec3ScaleMad(float, const float *, float, const float *, float *out) { if (out) { out[0] = out[1] = out[2] = 0; } }
GfxBrushModel *R_GetBrushModel(unsigned int) { return nullptr; }
void  CG_DoControllers(const cpose_t *, const DObj_s *, int *) {}
void  R_LinkDObjEntity(unsigned int, unsigned int, float *, float) {}
void  UnitQuatToAngles(const float *, float *out) { if (out) { out[0] = out[1] = out[2] = 0; } }
PhysPreset *DObjGetPhysPreset(const DObj_s *) { return nullptr; }
void  FX_RetriggerEffect(int, FxEffect *, int) {}
void  R_LinkBModelEntity(unsigned int, unsigned int, GfxBrushModel *) {}
void  CG_VehProcessEntity(int, centity_s *) {}
void  DObjSetHidePartBits(DObj_s *, const unsigned int *) {}
DObj_s *Com_ClientDObjCreate(DObjModel_s *, unsigned short, XAnimTree_s *, unsigned int, int) { return nullptr; }
void  DObjGetHierarchyBits(const DObj_s *, int, int *) {}
void  Phys_ObjBulletImpact(PhysWorld, dxBody *, const float *, const float *, float, float) {}
// CG_IsRagdollTrajectory provided by src/cgame_mp/cg_main_mp.cpp now.
void  R_SkinGfxEntityDelayed(GfxSceneEntity *) {}
int32_t CG_VehPlayerVehicleSlot(int, uint32_t) { return -1; }
bool  CG_VehEntityUsingVehicle(int, uint32_t) { return false; }
void  FX_AssertAllocatedEffect(int, FxEffect *) {}
bool  CG_PlayerUsingScopedTurret(int) { return false; }
void  R_UpdateXModelBoundsDelayed(GfxSceneEntity *) {}
void  BG_Player_DoControllersSetup(const entityState_s *, clientInfo_t *, int) {}
void  CG_VehSeatTransformForPlayer(int, uint32_t, float *o, float *a)
{
    if (o) { o[0] = o[1] = o[2] = 0; }
    if (a) { a[0] = a[1] = a[2] = 0; }
}
void  FX_MarkEntUpdateHidePartBits(const uint32_t *, const uint32_t *, int, int) {}
void  R_AddBrushModelToSceneFromAngles(const GfxBrushModel *, const float *, const float *, uint16_t) {}
void  DObjPhysicsSetCollisionFromXModel(const DObj_s *, PhysWorld, dxBody *) {}
void  Vec3Avg(const float *a, const float *b, float *out)
{
    if (a && b && out) { out[0] = (a[0]+b[0])*0.5f; out[1] = (a[1]+b[1])*0.5f; out[2] = (a[2]+b[2])*0.5f; }
}

uint16_t *controller_names[6]{};
// Hunk_AllocXAnimClient provided by src/cgame_mp/cg_main_mp.cpp now.

// === cg_newDraw_mp satellites ====================================================

uint32_t GetWeaponIndex(const cg_s *) { return 0u; }
bool     PM_IsSprinting(const playerState_s *) { return false; }
int32_t  BG_AmmoForWeapon(uint32_t) { return 0; }
bool     Key_IsCommandBound(int, const char *) { return false; }
int32_t  BG_GetAmmoPlayerMax(const playerState_s *, uint32_t, uint32_t) { return 0; }
bool     CL_ShouldDisplayHud(int) { return true; }
bool     BG_WeaponBlocksProne(uint32_t) { return false; }
int      UI_GetTalkerClientNum(int, int) { return -1; }
int32_t  BG_GetTotalAmmoReserve(const playerState_s *, uint32_t) { return 0; }
void     CG_DrawPlayerActionSlot(int, const rectDef_s *, uint32_t, float *, Font_s *, float, int) {}
void     CG_DrawPlayerWeaponIcon(int, const rectDef_s *, const float *) {}
int32_t  PM_GetSprintLeftLastTime(const playerState_s *) { return 0; }
// CG_GetPredictedPlayerState provided by src/cgame_mp/cg_main_mp.cpp now.
void     CG_DrawPlayerActionSlotDpad(int, const rectDef_s *, const float *, Material *) {}
void     CG_DrawPlayerWeaponAmmoStock(int, const rectDef_s *, Font_s *, float, float *, Material *, int) {}
void     CG_DrawPlayerWeaponBackground(int, const rectDef_s *, const float *, Material *) {}
int32_t  BG_PlayerWeaponCountPrimaryTypes(const playerState_s *) { return 0; }
void     CG_DrawPlayerWeaponLowAmmoWarning(int, const rectDef_s *, Font_s *, float, int, float, float, char, Material *) {}
void     CG_DrawPlayerWeaponAmmoClipGraphic(int, const rectDef_s *, const float *) {}

// cg_cursorHints provided by src/cgame_mp/cg_main_mp.cpp now.
// cg_drawBreathHint provided by src/cgame_mp/cg_main_mp.cpp now.
// cg_drawHealth provided by src/cgame_mp/cg_main_mp.cpp now.
// cg_drawMantleHint provided by src/cgame_mp/cg_main_mp.cpp now.
// cg_hintFadeTime provided by src/cgame_mp/cg_main_mp.cpp now.
// cg_hudProneY provided by src/cgame_mp/cg_main_mp.cpp now.
// cg_hudStanceFlash provided by src/cgame_mp/cg_main_mp.cpp now.
// cg_hudStanceHintPrints provided by src/cgame_mp/cg_main_mp.cpp now.
// cg_invalidCmdHintBlinkInterval provided by src/cgame_mp/cg_main_mp.cpp now.
// cg_invalidCmdHintDuration provided by src/cgame_mp/cg_main_mp.cpp now.
// cg_weaponHintsCoD1Style provided by src/cgame_mp/cg_main_mp.cpp now.
// === cg_main_mp satellites =======================================================

void Menu_Setup(UiContext *) {}
void BG_LoadAnim() {}
void CG_Veh_Init() {}
MenuList *UI_LoadMenus(char *, int) { return nullptr; }
void AimAssist_Init(int) {}
void UI_AddMenuList(UiContext *, MenuList *) {}
Font_s *CL_RegisterFont(const char *, int) { return nullptr; }
void SND_StopAmbient(int, int) {}
void BG_RegisterDvars() {}
void FX_KillEffectDef(int, const FxEffectDef *) {}
XModel *FX_RegisterModel(const char *) { return nullptr; }
menuDef_t *Menus_FindByName(const UiContext *, const char *) { return nullptr; }
void BG_ClearWeaponDef() {}
void Com_StripExtension(char *in, char *out)
{
    if (!in || !out) return;
    const char *dot = nullptr;
    for (const char *p = in; *p; ++p) { if (*p == '.') dot = p; }
    if (dot) { size_t n = static_cast<size_t>(dot - in); std::memcpy(out, in, n); out[n] = '\0'; }
    else     { std::strcpy(out, in); }
}
void UI_LoadIngameMenus(int) {}
void CG_VehRegisterDvars() {}
int32_t CG_WeaponDObjHandle(int32_t) { return 0; }
void Menus_FreeAllMemory(UiContext *) {}
char SND_AddLengthNotify(int, const snd_alias_t *, SndLengthId) { return 0; }
void SND_StopSoundsOnEnt(SndEntHandle) {}
void Com_LoadSoundAliases(const char *, const char *, snd_alias_system_t) {}
void SND_PlayAmbientAlias(int, const snd_alias_t *, int, snd_alias_system_t) {}
void Snd_AssertAliasValid(snd_alias_t *) {}
int32_t DB_GetAllXAssetOfType(XAssetType, XAssetHeader *, int32_t) { return 0; }
void DynEntCl_RegisterDvars() {}
const char **FS_ListFilesInLocation(const char *, const char *, FsListBehavior_e, int *numfiles, int)
{
    if (numfiles) *numfiles = 0;
    return nullptr;
}
void SND_AddPlayFXSoundAlias(snd_alias_t *, SndEntHandle, const float *) {}
void SND_StopSoundAliasOnEnt(SndEntHandle, const char *) {}
void Scr_ShutdownGameStrings() {}
void FX_RegisterDefaultEffect() {}
snd_alias_list_t *Com_FindSoundAliasNoErrors(const char *) { return nullptr; }
snd_alias_t *Com_PickSoundAliasFromList(snd_alias_list_t *) { return nullptr; }
int SND_PlaySoundAliasAsMaster(const snd_alias_t *, SndEntHandle, const float *, int, snd_alias_system_t) { return 0; }
void CG_AmmoCounterRegisterDvars() {}
void BG_LoadPenetrationDepthTable() {}
uint8_t *Hunk_AllocPhysPresetPrecache(unsigned int size) { return static_cast<uint8_t *>(std::calloc(size > 0 ? size : 1, 1)); }

unsigned int bg_lastParsedWeaponIndex = 0;
clientConnection_t clientConnections[1]{};
const dvar_t *g_compassShowEnemies = nullptr;

// === sv_voice_mp satellites ======================================================

bgs_t level_bgs{};
const dvar_t *voice_deadChat  = nullptr;
const dvar_t *voice_global    = nullptr;
const dvar_t *voice_localEcho = nullptr;

// === g_trigger_mp satellites =====================================================

int32_t G_SpawnInt(const char *, const char *, int32_t *out) { if (out) *out = 0; return 0; }
int32_t G_SpawnFloat(const char *, const char *, float *out) { if (out) *out = 0.f; return 0; }
void    Scr_AddEntity(gentity_s *) {}
void    Scr_AddVector(const float *) {}
int     CM_AreaEntities(const float *, const float *, int *, int, int) { return 0; }
void    AddPointToBounds(const float *v, float *mins, float *maxs)
{
    if (!v || !mins || !maxs) return;
    for (int i = 0; i < 3; ++i) { if (v[i] < mins[i]) mins[i] = v[i]; if (v[i] > maxs[i]) maxs[i] = v[i]; }
}
// G_FreeEntityDelay provided by src/game_mp/g_utils_mp.cpp now.
int32_t G_LevelSpawnString(const char *, const char *, const char **out) { if (out) *out = ""; return 0; }
BOOL    Scr_IsSystemActive() { return 0; }
int     SV_SightTraceToEntity(float *, float *, float *, float *, int, int) { return 0; }

// === cl_scrn_mp satellites =======================================================

void   R_EndFrame() {}
void   UI_Refresh(int) {}
void   CL_DrawLogo(int) {}
void   DevGui_Draw(int) {}
void   R_BeginFrame() {}
void   UI_UpdateTime(int, int) {}
void   Con_DrawConsole(int) {}
void   R_EndCubemapShot(CubemapShot) {}
void   SND_InitFXSounds() {}
double UI_GetBlurRadius(int) { return 0.0; }
void   R_AddCmdEndOfList() {}
void   R_SaveCubemapShot(char *, CubemapShot, float, float) {}
void   SCR_DrawCinematic(int) {}
void   Net_DisplayProfile(int) {}
void   R_BeginCubemapShot(int, int) {}
void   R_AddCmdClearScreen(int, const float *, float, unsigned char) {}
void   R_AddCmdDrawProfile() {}
void   R_BeginSharedCmdList() {}
void   Sys_LoadingKeepAlive() {}
void   UI_DrawConnectScreen(int) {}
void   R_IssueRenderCommands(unsigned int) {}
void   R_BeginClientCmdList2D() {}
void   R_ClearClientCmdList2D() {}
void   R_BspGenerateReflections() {}
void   R_LightingFromCubemapShots(const float *) {}
char   CL_AnyLocalClientChallenging() { return 0; }
char   CL_AllLocalClientsDisconnected() { return 1; }
unsigned int FS_FTell(int) { return 0u; }
const dvar_t *r_reflectionProbeGenerate = nullptr;

// === cl_ui_mp satellites =========================================================

void UI_Shutdown(int) {}
void UI_Component_Init() {}
const char *Key_KeynumToString(int32_t, int32_t) { return ""; }
int32_t CL_UpdateDirtyPings(netsrc_t, uint32_t) { return 0; }
char *UI_GetMapDisplayName(const char *) { return const_cast<char *>(""); }
void R_PushRemoteScreenUpdate(int) {}
char *UI_GetGameTypeDisplayName(const char *) { return const_cast<char *>(""); }
void UI_Init(int) {}

// === g_misc_mp satellites ========================================================

// G_AddEvent provided by src/game_mp/g_utils_mp.cpp now.
// G_SetAngle provided by src/game_mp/g_utils_mp.cpp now.
void YawVectors(float, float *f, float *r) { if (f) { f[0] = 1; f[1] = 0; f[2] = 0; } if (r) { r[0] = 0; r[1] = 1; r[2] = 0; } }
// G_SetOrigin provided by src/game_mp/g_utils_mp.cpp now.
// G_GeneralLink provided by src/game_mp/g_utils_mp.cpp now.
float ColorNormalize(const float *, float *out) { if (out) { out[0] = 1; out[1] = 1; out[2] = 1; out[3] = 1; } return 1.f; }
void G_TraceCapsule(trace_t *trace, const float *, const float *, const float *, const float *, int, int)
{ if (trace) { std::memset(trace, 0, sizeof(*trace)); trace->fraction = 1.f; } }
void SV_UnlinkEntity(gentity_s *) {}
// G_PlaySoundAlias provided by src/game_mp/g_utils_mp.cpp now.
int32_t IsItemRegistered(uint32_t) { return 0; }
void G_LocationalTrace(trace_t *trace, float *, float *, int, int, uint8_t *)
{ if (trace) { std::memset(trace, 0, sizeof(*trace)); trace->fraction = 1.f; } }
// G_SoundAliasIndex provided by src/game_mp/g_utils_mp.cpp now.
void SetClientViewAngle(gentity_s *, const float *) {}
void G_GetPlayerViewOrigin(const playerState_s *, float *out) { if (out) { out[0] = out[1] = out[2] = 0; } }
void DObjSetControlTagAngles(DObj_s *, int *, unsigned int, float *) {}
// G_DObjGetLocalTagMatrix provided by src/game_mp/g_utils_mp.cpp now.
// G_DObjGetWorldTagMatrix provided by src/game_mp/g_utils_mp.cpp now.
uint32_t G_GetWeaponIndexForName(const char *) { return 0u; }
void BG_GetPlayerViewDirection(const playerState_s *, float *f, float *r, float *u)
{
    if (f) { f[0] = 1; f[1] = 0; f[2] = 0; }
    if (r) { r[0] = 0; r[1] = 1; r[2] = 0; }
    if (u) { u[0] = 0; u[1] = 0; u[2] = 1; }
}
gentity_s *Weapon_RocketLauncher_Fire(gentity_s *, uint32_t, float, struct weaponParms *, const float *, gentity_s *, const float *) { return nullptr; }

// === g_utils_mp satellites =======================================================

void player_die(gentity_s *, gentity_s *, gentity_s *, int, int, int, const float *, hitLocation_t, int) {}
void Helicopter_Die(gentity_s *, gentity_s *, gentity_s *, const int, const int, const int, const float *, const hitLocation_t, int) {}
void Scr_FreeEntity(gentity_s *) {}
void Scr_FreeThread(uint16_t) {}
void DB_ReplaceModel(const char *, const char *) {}
void G_VehFreeEntity(gentity_s *) {}
void Helicopter_Pain(gentity_s *, gentity_s *, int, const float *, const int, const float *, const hitLocation_t, const int) {}
void MatrixTranspose(const mat3x3 &in, mat3x3 &out)
{
    out[0][0] = in[0][0]; out[0][1] = in[1][0]; out[0][2] = in[2][0];
    out[1][0] = in[0][1]; out[1][1] = in[1][1]; out[1][2] = in[2][1];
    out[2][0] = in[0][2]; out[2][1] = in[1][2]; out[2][2] = in[2][2];
}
void Touch_Item_Auto(gentity_s *, gentity_s *, int) {}
void G_ExplodeMissile(gentity_s *) {}
void Helicopter_Think(gentity_s *) {}
void G_VehUnlinkPlayer(gentity_s *, gentity_s *) {}
void PlayerCorpse_Free(gentity_s *) {}
uint16_t Scr_ExecEntThread(gentity_s *, int, uint32_t) { return 0; }
void FinishSpawningItem(gentity_s *) {}
void G_PlayerController(const gentity_s *, int *) {}
void G_TimedObjectThink(gentity_s *) {}
void G_VehEntHandler_Die(gentity_s *, gentity_s *, gentity_s *, const int, const int, const int, const float *, const hitLocation_t, int) {}
void G_VehEntHandler_Use(gentity_s *, gentity_s *, gentity_s *) {}
DObj_s *Com_ServerDObjCreate(DObjModel_s *, unsigned short, XAnimTree_s *, unsigned int) { return nullptr; }
void DroppedItemClearOwner(gentity_s *) {}
void G_VehEntHandler_Think(gentity_s *) {}
void G_VehEntHandler_Touch(gentity_s *, gentity_s *, int) {}
void Helicopter_Controller(const gentity_s *, int *) {}
void Com_SafeServerDObjFree(unsigned int) {}
unsigned int SL_FindLowercaseString(const char *) { return 0u; }
// SV_GetConfigstringConst provided by src/server_mp/sv_init_mp.cpp now.
void Hunk_OverrideDataForFile(int, const char *, void *) {}
void MatrixInverseOrthogonal43(const mat4x3 &in, mat4x3 &out)
{
    out[0][0] = in[0][0]; out[0][1] = in[1][0]; out[0][2] = in[2][0];
    out[1][0] = in[0][1]; out[1][1] = in[1][1]; out[1][2] = in[2][1];
    out[2][0] = in[0][2]; out[2][1] = in[1][2]; out[2][2] = in[2][2];
    out[3][0] = -in[3][0]; out[3][1] = -in[3][1]; out[3][2] = -in[3][2];
}
void Missile_FreeAttractorRefs(gentity_s *) {}
void G_VehEntHandler_Controller(const gentity_s *, int *) {}
void BodyEnd(gentity_s *) {}
void AxisClear(mat3x3 &axis)
{
    axis[0][0] = 1; axis[0][1] = 0; axis[0][2] = 0;
    axis[1][0] = 0; axis[1][1] = 1; axis[1][2] = 0;
    axis[2][0] = 0; axis[2][1] = 0; axis[2][2] = 1;
}

scr_data_t g_scr_data{};

// === cl_input satellites =========================================================

void UI_MouseEvent(int, int, int) {}
bool DevGui_IsActive() { return false; }
bool Sys_IsLANAddress(netadr_t) { return false; }
void IN_ShowSystemCursor(BOOL) {}
void AimAssist_UpdateMouseInput(const AimInput *, AimOutput *) {}
void CL_SavePredictedOriginForServerTime(clientActive_t *, int32_t, float *, float *, float *, int32_t, int32_t) {}
char ClampChar(int v) { if (v < -128) return -128; if (v > 127) return 127; return static_cast<char>(v); }
void UI_Component::MouseEvent(int, int) {}

const dvar_t *cl_debugMessageKey = nullptr;
const dvar_t *cl_freelook        = nullptr;
const dvar_t *cl_maxpackets      = nullptr;
const dvar_t *cl_mouseAccel      = nullptr;
const dvar_t *cl_nodelta         = nullptr;
const dvar_t *cl_packetdup       = nullptr;
const dvar_t *cl_sensitivity     = nullptr;
const dvar_t *cl_showMouseRate   = nullptr;
uint32_t      frame_msec         = 0u;
const dvar_t *m_filter           = nullptr;
const dvar_t *m_forward          = nullptr;
const dvar_t *m_pitch            = nullptr;
const dvar_t *m_side             = nullptr;
const dvar_t *m_yaw              = nullptr;
PlayerKeyState playerKeys[1]{};

// === sv_init_mp satellites =======================================================

void FS_Restart(int, int) {}
void CL_InitLoad(const char *, const char *) {}
// SV_RunFrame provided by src/server_mp/sv_main_mp.cpp now.
void CL_MapLoading(const char *) {}
char *ClientConnect(uint32_t, uint16_t) { return nullptr; }
// SV_FreeClients provided by src/server_mp/sv_client_mp.cpp now.
// SV_Heartbeat_f provided by src/server_mp/sv_ccmds_mp.cpp now.
// SV_InitSnapshot provided by src/server_mp/sv_main_mp.cpp now.
char *FS_LoadedIwdNames() { return const_cast<char *>(""); }
// SV_SendDisconnect provided by src/server_mp/sv_client_mp.cpp now.
void DB_UpdateDebugZone() {}
void Hunk_FreeTempMemory(char *) {}
void SV_EndClientSnapshot(client_t *, msg_t *) {}
void FS_ClearIwdReferences() {}
char *FS_LoadedIwdChecksums() { return const_cast<char *>(""); }
char *FS_ReferencedIwdNames() { return const_cast<char *>(""); }
void Scr_ParseGameTypeList() {}
char CL_IsLocalClientActive(int) { return 0; }
// SV_AddOperatorCommands provided by src/server_mp/sv_ccmds_mp.cpp now.
void SV_BeginClientSnapshot(client_t *, msg_t *) {}
// SV_SetSystemInfoConfig provided by src/server_mp/sv_main_mp.cpp now.
char *DB_ReferencedFFNameList() { return const_cast<char *>(""); }
char *DB_ReferencedFFChecksums() { return const_cast<char *>(""); }
void SV_GetServerStaticHeader() {}
void SV_SetServerStaticHeader() {}
void SV_WriteSnapshotToClient(client_t *, msg_t *) {}
char *FS_ReferencedIwdChecksums() { return const_cast<char *>(""); }
void SV_WriteEntityFieldNumbers() {}
void Sys_EndLoadThreadPriorities() {}
void Sys_BeginLoadThreadPriorities() {}

// com_inServerFrame provided by src/server_mp/sv_main_mp.cpp now.
// sv_serverId_value provided by src/server_mp/sv_ccmds_mp.cpp now.
// sv_allowedClan1 provided by src/server_mp/sv_main_mp.cpp now.
// sv_allowedClan2 provided by src/server_mp/sv_main_mp.cpp now.
// sv_botsPressAttackBtn provided by src/server_mp/sv_main_mp.cpp now.
// sv_cheats provided by src/server_mp/sv_main_mp.cpp now.
// sv_connectTimeout provided by src/server_mp/sv_main_mp.cpp now.
// sv_debugMessageKey provided by src/server_mp/sv_main_mp.cpp now.
// sv_debugPacketContents provided by src/server_mp/sv_main_mp.cpp now.
// sv_debugPacketContentsForClientThisFrame provided by src/server_mp/sv_main_mp.cpp now.
// sv_debugPlayerstate provided by src/server_mp/sv_main_mp.cpp now.
// sv_debugRate provided by src/server_mp/sv_main_mp.cpp now.
// sv_debugReliableCmds provided by src/server_mp/sv_main_mp.cpp now.
// sv_disableClientConsole provided by src/server_mp/sv_main_mp.cpp now.
// sv_floodProtect provided by src/server_mp/sv_main_mp.cpp now.
// sv_fps provided by src/server_mp/sv_main_mp.cpp now.
// sv_hostname provided by src/server_mp/sv_main_mp.cpp now.
// sv_kickBanTime provided by src/server_mp/sv_main_mp.cpp now.
// sv_mapRotation provided by src/server_mp/sv_main_mp.cpp now.
// sv_mapRotationCurrent provided by src/server_mp/sv_main_mp.cpp now.
// sv_mapname provided by src/server_mp/sv_main_mp.cpp now.
// sv_maxPing provided by src/server_mp/sv_main_mp.cpp now.
// sv_maxRate provided by src/server_mp/sv_main_mp.cpp now.
// sv_minPing provided by src/server_mp/sv_main_mp.cpp now.
// sv_packet_info provided by src/server_mp/sv_main_mp.cpp now.
// sv_padPackets provided by src/server_mp/sv_main_mp.cpp now.
// sv_privateClients provided by src/server_mp/sv_main_mp.cpp now.
// sv_reconnectlimit provided by src/server_mp/sv_main_mp.cpp now.
// sv_serverid provided by src/server_mp/sv_main_mp.cpp now.
// sv_showAverageBPS provided by src/server_mp/sv_main_mp.cpp now.
// sv_showCommands provided by src/server_mp/sv_main_mp.cpp now.
// sv_timeout provided by src/server_mp/sv_main_mp.cpp now.
// sv_zombietime provided by src/server_mp/sv_main_mp.cpp now.
// === sv_ccmds_mp satellites ======================================================

void BG_SetPerk(int32_t *, uint32_t) {}
char *I_CleanStr(char *s) { return s; }
int I_DrawStrlen(const char *s) { return s ? static_cast<int>(std::strlen(s)) : 0; }
// SV_BanClient provided by src/server_mp/sv_client_mp.cpp now.
void Scr_DoProfile(float) {}
void FS_ConvertPath(char *) {}
// SV_UnbanClient provided by src/server_mp/sv_client_mp.cpp now.
void Scr_RunDebugger() {}
int32_t G_GetClientScore(int32_t) { return 0; }
clientState_s *G_GetClientState(int32_t) { return nullptr; }
void G_SetSavePersist(int32_t) {}
// SV_BanGuidBriefly provided by src/server_mp/sv_client_mp.cpp now.
// SV_AddServerCommand provided by src/server_mp/sv_main_mp.cpp now.
// SV_ClientEnterWorld provided by src/server_mp/sv_client_mp.cpp now.
void Scr_DoProfileBuiltin(float) {}
void Scr_DumpScriptThreads() {}
void Scr_RunDebuggerRemote() {}
void Scr_DumpScriptVariables(bool, bool, bool, bool, bool, const char *, const char *, int) {}
void Steam_SV_AddTestCommands() {}

// === sv_main_mp satellites =======================================================

void G_RunFrame(int32_t) {}
void FakeLag_Frame() {}
void Scr_FreeValue(unsigned int) {}
// SV_ClientThink provided by src/server_mp/sv_client_mp.cpp now.
void Scr_SetLoading(int) {}
int  Netchan_Process(netchan_t *, msg_t *) { return 0; }
// SV_GetChallenge provided by src/server_mp/sv_client_mp.cpp now.
// SV_ReceiveStats provided by src/server_mp/sv_client_mp.cpp now.
// SV_DirectConnect provided by src/server_mp/sv_client_mp.cpp now.
WinThreadLock Win_GetThreadLock() { return {}; }
// SV_DelayDropClient provided by src/server_mp/sv_client_mp.cpp now.
void Scr_UpdateDebugger() {}
void SV_SendClientMessages() {}
int32_t G_GetClientArchiveTime(int32_t) { return 0; }
// SV_ExecuteClientMessage provided by src/server_mp/sv_client_mp.cpp now.
void MSG_WriteReliableCommandToBuffer(const char *, char *, int) {}

uint8_t tempServerMsgBuf[131072]{};

// === sv_client_mp satellites =====================================================

void ClientBegin(int32_t) {}
void ClientThink(int32_t) {}
int  FS_WriteFile(char *, char *, unsigned int) { return 0; }
void ClientCommand(int32_t) {}
void Netchan_Setup(netsrc_t, netchan_t *, netadr_t, int, char *, int, char *, int) {}
bool NET_CompareAdr(netadr_t, netadr_t) { return false; }
void MSG_WriteEntity(SnapshotInfo_s *, msg_t *, int, entityState_s *, const entityState_s *, int) {}
bool BG_IsWeaponValid(const playerState_s *, uint32_t) { return false; }
void ClientDisconnect(int32_t) {}
void G_SetLastServerTime(int32_t, int32_t) {}
void SV_PacketDataIsHeader(int, const msg_t *) {}
bool Steam_CheckClientTicket(unsigned char *, unsigned int, unsigned long long) { return true; }
void Steam_OnClientDropped(unsigned long long) {}
void SV_BuildClientSnapshot(client_t *) {}
void SV_SendMessageToClient(msg_t *, client_t *) {}
bool BG_ValidateWeaponNumber(uint32_t) { return true; }
bool Sys_IsLANAddress_IgnoreSubnet(netadr_t) { return false; }
void SV_UpdateServerCommandsToClient(client_t *, msg_t *) {}

const dvar_t *net_lanauthorize = nullptr;

// === CGAME dvars and storage referenced by the new sources =========================

// cg_crosshairAlpha provided by src/cgame_mp/cg_main_mp.cpp now.
// cg_crosshairAlphaMin provided by src/cgame_mp/cg_main_mp.cpp now.
// cg_crosshairDynamic provided by src/cgame_mp/cg_main_mp.cpp now.
// cg_crosshairEnemyColor provided by src/cgame_mp/cg_main_mp.cpp now.
// cg_debugInfoCornerOffset provided by src/cgame_mp/cg_main_mp.cpp now.
// cg_debug_overlay_viewport provided by src/cgame_mp/cg_main_mp.cpp now.
// cg_drawCrosshair provided by src/cgame_mp/cg_main_mp.cpp now.
// cg_drawFPS provided by src/cgame_mp/cg_main_mp.cpp now.
// cg_drawFPSLabels provided by src/cgame_mp/cg_main_mp.cpp now.
// cg_drawGun provided by src/cgame_mp/cg_main_mp.cpp now.
// cg_drawMaterial provided by src/cgame_mp/cg_main_mp.cpp now.
// cg_drawScriptUsage provided by src/cgame_mp/cg_main_mp.cpp now.
// cg_drawSnapshot provided by src/cgame_mp/cg_main_mp.cpp now.
// cg_drawTurretCrosshair provided by src/cgame_mp/cg_main_mp.cpp now.
// cg_drawVersion provided by src/cgame_mp/cg_main_mp.cpp now.
// cg_drawVersionX provided by src/cgame_mp/cg_main_mp.cpp now.
// cg_drawVersionY provided by src/cgame_mp/cg_main_mp.cpp now.
// cg_drawpaused provided by src/cgame_mp/cg_main_mp.cpp now.
// cg_enemyNameFadeIn provided by src/cgame_mp/cg_main_mp.cpp now.
// cg_friendlyNameFadeIn provided by src/cgame_mp/cg_main_mp.cpp now.
// hud_fade_offhand provided by src/cgame_mp/cg_newDraw_mp.cpp now.
const dvar_t *phys_drawDebugInfo       = nullptr;
const dvar_t *player_debugHealth       = nullptr;
const dvar_t *snd_drawEqChannels       = nullptr;
// snd_drawInfo provided by src/cgame_mp/cg_main_mp.cpp now.
// cg_weaponsArray provided by src/cgame_mp/cg_main_mp.cpp now.