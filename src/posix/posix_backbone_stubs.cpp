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
#include <universal/com_files.h>
#include <universal/com_math.h>
#include <bgame/bg_local.h>
#include <client_mp/client_mp.h>
#include <server_mp/server_mp.h>
#include <stringed/stringed_hooks.h>
#include <ui/ui_shared.h>
#include <client/client.h>
#include <DynEntity/DynEntity_client.h>

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

void Com_BuildPlayerProfilePath(char *path, int size, const char * /*name*/, ...)
{
    if (size > 0) path[0] = 0;
}

bool Com_HasPlayerProfile()
{
    return false;
}

void Com_InitPlayerProfiles(int /*controllerIndex*/) {}
void Com_InitHunkMemory() {}
void Com_InitDObj() {}
void Com_ShutdownDObj() {}
void Com_ShutdownWorld() {}
void Com_CleanupBsp() {}
void Com_CheckSetRecommended(int /*newConfig*/) {}
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

unsigned int Win_UpdateThreadLock() { return 0; }

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
void CL_FlushDebugServerData() {}
void CL_ForwardCommandToServer(int /*localClientNum*/, const char * /*cmd*/) {}
void CL_Frame(netsrc_t /*sock*/) {}
int  CL_GetLocalClientConnection(int /*localClientNum*/) { return 0; }
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
void CL_UpdateDebugServerData() {}
void CL_UpdateSound() {}

// =========================================================================
// SV_* — server. Stubs.
// =========================================================================

void SV_AddDedicatedCommands() {}
int  SV_Frame(int /*frameTime*/) { return 0; }
void SV_GameCommand() {}
void SV_Init() {}
void SV_PacketEvent(netadr_t /*from*/, msg_t * /*msg*/) {}
void SV_SetConfigValueForKey(int /*index*/, int /*key*/, char * /*name*/, char * /*value*/) {}
void SV_Shutdown(const char * /*reason*/) {}
void SV_ShutdownGameProgs() {}
void SV_WaitServer() {}

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
void Scr_UpdateDebugSocket() {}
void SCR_UpdateScreen() {}
void GScr_Shutdown() {}

void DObjInit() {}
void DObjShutdown() {}
void FakeLag_Init() {}
void FakeLag_Shutdown() {}
void FX_UnregisterAll() {}
void IN_Frame() {}
void DevGui_Update(int /*localClientNum*/, float /*frameTime*/) {}
// Ragdoll_Update now provided by ragdoll/ragdoll_update.cpp.
void SetAnimCheck(int /*setting*/) {}
void LargeLocalReset() {}
void LiveStorage_Init() {}
void XAnimInit() {}
void XAnimShutdown() {}
void Swap_Init() {}
void SL_Init() {}
void BG_ShutdownWeaponDefFiles() {}
void Con_InitChannels() {}
bool Con_IsChannelVisible(print_msg_dest_t /*dest*/, unsigned int /*channel*/, int /*msgFilters*/) { return false; }
void Con_WriteFilterConfigString(int /*localClientNum*/) {}
void Key_WriteBindings(int /*localClientNum*/, int /*f*/) {}
char *SEH_LocalizeTextMessage(const char *src, const char * /*context*/, msgLocErrType_t /*err*/) { return const_cast<char *>(src); }
void SEH_UpdateLanguageInfo() {}
const char *StringTable_GetColumnValueForRow(const StringTable * /*table*/, int /*row*/, int /*col*/) { return ""; }
void ProfLoad_Init() {}
bool ProfLoad_IsActive() { return false; }
void ProfLoad_Deactivate() {}

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
void MSG_Init(msg_t * /*buf*/, unsigned char * /*data*/, int /*length*/) {}

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
server_t sv;
BOOL updateScreenCalled;

// fx_randomTable: 507-entry deterministic random table used by the
// EffectsCore particle system. Real upstream fills this once at startup
// from a fixed seed so spawn positions/velocities are reproducible
// across the network. We define the storage as writable and let the
// `extern const float fx_randomTable[507]` declaration in fx_system.h
// pick it up by symbol name; the static init below fills it.
float fx_randomTable[507];
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
cpose_t *CG_GetPose(int /*localClientNum*/, unsigned int /*handle*/) { return nullptr; }
void CG_DObjCalcBone(const cpose_t * /*pose*/, DObj_s * /*obj*/, int /*boneIndex*/) {}
void CG_DrawStringExt(const ScreenPlacement * /*place*/, float /*x*/, float /*y*/,
                      char * /*text*/, const float * /*color*/, int /*fontIndex*/,
                      int /*maxChars*/, float /*scale*/) {}

// Com / DObj
DObj_s *Com_GetClientDObj(unsigned int /*handle*/, int /*localClientNum*/) { return nullptr; }
void DObjDisplayAnim(const DObj_s * /*obj*/, const char * /*header*/) {}
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

// XModel
void XModelGetBounds(const XModel * /*model*/, float *mins, float *maxs)
{
    if (mins) mins[0] = mins[1] = mins[2] = 0;
    if (maxs) maxs[0] = maxs[1] = maxs[2] = 0;
}

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
void Phys_AddCollisionContact(PhysWorld /*w*/, const PhysContact * /*c*/, dxBody * /*a*/, dxBody * /*b*/) {}
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
// `cgArray` is `cg_s cgArray[1]` in upstream; ScreenPlacement is fully
// defined in ui/ui_shared.h (included above) so we can zero-construct.
struct cg_s;
alignas(16) static unsigned char cgArray_storage[65536];
cg_s &cgArray = *reinterpret_cast<cg_s *>(cgArray_storage);
const dvar_t *cg_paused = nullptr;
ScreenPlacement scrPlaceFull{};

namespace {
struct FxRandomTableInit {
    FxRandomTableInit() {
        unsigned int s = 0xCAFEF00Du;
        for (int i = 0; i < 507; ++i) {
            s = s * 1103515245u + 12345u;
            fx_randomTable[i] = ((s >> 8) & 0xFFFFFF) / float(0x800000) - 1.0f;
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
