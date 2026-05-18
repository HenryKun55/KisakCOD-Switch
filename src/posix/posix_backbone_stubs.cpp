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

#include <qcommon/qcommon.h>
#include <universal/com_files.h>
#include <universal/com_math.h>
#include <bgame/bg_local.h>
#include <client_mp/client_mp.h>
#include <server_mp/server_mp.h>
#include <stringed/stringed_hooks.h>
#include <ui/ui_shared.h>
#include <client/client.h>

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
void Ragdoll_Update(int /*frameTime*/) {}
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
