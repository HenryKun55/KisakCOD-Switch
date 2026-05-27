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

// const char *CopyString(const char *in)
// {
//     if (!in) return nullptr;
//     size_t n = std::strlen(in) + 1;
//     char *out = static_cast<char *>(std::malloc(n));
//     std::memcpy(out, in, n);
//     return out;
// }

// void FreeString(const char *str)
// {
//     std::free(const_cast<char *>(str));
// }

// CanKeepStringPointer provided by src/universal/q_shared.cpp now.

// void *Z_Malloc(int size, const char * /*name*/, int /*type*/)
// {
//     return std::malloc(static_cast<size_t>(size));
// }

// void Z_Free(void *ptr, int /*type*/)
// {
//     std::free(ptr);
// }

// Vec4Compare, Vec4IsNormalized, Vec3Basis_RightHanded, UnitQuatToAxis
// provided by src/universal/com_math.cpp now.

// Seeded random unit-sphere direction. Marsaglia's method:
// pick two uniforms in [-1, 1] with s = x²+y² < 1, then map to a
// point on the unit sphere. Uses `fx_randomTable` for determinism
// (the engine wants the same particle to spawn the same direction
// across runs at a given seed).
void FX_RandomDir(int seed, float *dir);  // forward decl, defined after fx_randomTable.

// Com_sprintf provided by src/universal/q_shared.cpp now.

// Com_DefaultExtension, Com_GetFilenameSubString provided by
// src/universal/q_shared.cpp now.

// Com_BuildPlayerProfilePath now provided by qcommon/com_playerprofile.cpp.
// Com_HasPlayerProfile now provided by qcommon/com_playerprofile.cpp.
// Com_InitPlayerProfiles now in qcommon/com_playerprofile.cpp.
// void Com_InitHunkMemory() {}
// void Com_InitDObj() {}
// void Com_ShutdownDObj() {}
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

// I_strlwr, I_strncat, I_strncmp provided by src/universal/q_shared.cpp now.

// =========================================================================
// Info_* — userinfo/serverinfo key/value strings. Upstream impl is in a
// file we haven't pulled yet; the stub here is a no-op so callers that
// build up a config string just get an empty result.
// =========================================================================

// void Info_SetValueForKey(char * /*s*/, const char * /*key*/, const char * /*value*/) {}  // provided by q_shared.cpp now
// void Info_SetValueForKey_Big(char * /*s*/, const char * /*key*/, const char * /*value*/) {}  // provided by q_shared.cpp now

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

// bool FS_Initialized() { return false; }  // provided by com_files.cpp now
// void FS_InitFilesystem() {}  // provided by com_files.cpp now
// void FS_Shutdown() {}  // provided by com_files.cpp now
// void FS_ResetFiles() {}  // provided by com_files.cpp now
// void FS_FCloseFile(int /*h*/) {}  // provided by com_files.cpp now
// void FS_FCloseLogFile(int /*h*/) {}  // provided by com_files.cpp now
// void FS_Flush(int /*f*/) {}  // provided by com_files.cpp now
// void FS_FreeFile(char * /*buffer*/) {}  // provided by com_files.cpp now
// FS_FilenameCompare provided by src/universal/com_files.cpp now.
// FS_FOpenFileRead provided by src/universal/com_files.cpp now.
// int FS_FOpenFileWrite(const char * /*filename*/) { return 0; }  // provided by com_files.cpp now
// int FS_FOpenFileWriteToDir(const char * /*filename*/, const char * /*dir*/) { return 0; }  // provided by com_files.cpp now
// int FS_FOpenTextFileWrite(const char * /*filename*/) { return 0; }  // provided by com_files.cpp now
// FS_ReadFile provided by src/universal/com_files.cpp now.
// int FS_SV_FileExists(char * /*file*/) { return 0; }  // provided by com_files.cpp now
// FS_ListFiles provided by src/universal/com_files.cpp now.
// unsigned int FS_WriteLog(const char * /*buffer*/, unsigned int /*len*/, int /*h*/) { return 0; }  // provided by com_files.cpp now
// void FS_Printf(int /*h*/, const char * /*fmt*/, ...) {}  // provided by com_files.cpp now

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

// void CL_CharEvent(int /*localClientNum*/, int /*ch*/) {}  // provided by cl_keys.cpp now
// void CL_ConsoleFixPosition() {}  // provided by cl_console.cpp now
// CL_ConsolePrint provided by src/client/cl_console.cpp now.
// CL_ControllerIndexFromClientNum provided by src/client_mp/cl_main_mp.cpp now.
// CL_Disconnect provided by src/client_mp/cl_main_mp.cpp now.
// CL_FlushDebugServerData now in client/cl_debugdata.cpp.
// CL_ForwardCommandToServer provided by src/client_mp/cl_main_mp.cpp now.
// CL_Frame provided by src/client_mp/cl_main_mp.cpp now.
struct clientConnection_t;
// CL_GetLocalClientConnection provided by src/cgame_mp/cg_main_mp.cpp now.
// CL_GetUsernameForLocalClient provided by src/client_mp/cl_main_mp.cpp now.
// CL_Init provided by src/client_mp/cl_main_mp.cpp now.
// CL_InitDedicated provided by src/client_mp/cl_main_mp.cpp now.
// void CL_InitKeyCommands() {}  // provided by cl_keys.cpp now
// CL_InitOnceForAllClients provided by src/client_mp/cl_main_mp.cpp now.
// CL_InitRenderer provided by src/client_mp/cl_main_mp.cpp now.
// void CL_KeyEvent(int /*localClientNum*/, int /*key*/, int /*down*/, unsigned int /*time*/) {}  // provided by cl_keys.cpp now
// CL_PacketEvent provided by src/client_mp/cl_main_mp.cpp now.
// CL_RunOncePerClientFrame provided by src/client_mp/cl_main_mp.cpp now.
// CL_Shutdown provided by src/client_mp/cl_main_mp.cpp now.
// CL_ShutdownAll provided by src/client_mp/cl_main_mp.cpp now.
// CL_ShutdownHunkUsers provided by src/client_mp/cl_main_mp.cpp now.
// CL_ShutdownRef provided by src/client_mp/cl_main_mp.cpp now.
// CL_StartHunkUsers provided by src/client_mp/cl_main_mp.cpp now.
// CL_UpdateDebugServerData now in client/cl_debugdata.cpp.
// CL_UpdateSound provided by src/client_mp/cl_main_mp.cpp now.

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

// int  UI_GetMenuScreen() { return 0; }  // provided by ui_shared.cpp now
// int  UI_GetMenuScreenForError() { return 0; }  // provided by ui_shared.cpp now
// int  UI_IsFullscreen(int /*localClientNum*/) { return 0; }  // provided by ui_main_mp.cpp now
// int  UI_SetActiveMenu(int /*localClientNum*/, uiMenuCommand_t /*menu*/) { return 0; }  // provided by ui_main_mp.cpp now
// void UI_SetMap(char * /*name*/, char * /*gametype*/) {}  // provided by ui_main_mp.cpp now

// =========================================================================
// Scripting + misc. Stubs.
// =========================================================================

bool Scr_CanDrawScript() { return false; }
// void Scr_Cleanup() {}  // provided by scr_variable/scr_stringlist now
void Scr_DrawScript() {}
void Scr_Init() {}
// void Scr_InitVariables() {}  // provided by scr_variable/scr_stringlist now
void Scr_MonitorCommand(const char * /*cmd*/) {}
void Scr_Settings(int /*developer*/, int /*developer_script*/, int /*abort_on_error*/) {}
void Scr_Shutdown() {}
int  Scr_UpdateDebugSocket() { return 0; }
// SCR_UpdateScreen provided by src/client_mp/cl_scrn_mp.cpp now.
// GScr_Shutdown provided by src/game_mp/g_scr_main_mp.cpp now.

// void DObjInit() {}
// void DObjShutdown() {}
// void FakeLag_Init() {}  // provided by net_chan_mp.cpp now
// void FakeLag_Shutdown() {}  // provided by net_chan_mp.cpp now
void FX_UnregisterAll() {}
void IN_Frame() {}
// void DevGui_Update(int /*localClientNum*/, float /*frameTime*/) {}
// Ragdoll_Update now provided by ragdoll/ragdoll_update.cpp.
// SetAnimCheck now provided by script/scr_animtree.cpp.
// void LargeLocalReset() {}
void LiveStorage_Init() {}
// XAnimInit/Shutdown now provided by xanim/xanim.cpp.
// void Swap_Init() {}  // provided by q_shared.cpp now
// void SL_Init() {}  // provided by scr_variable/scr_stringlist now
// void BG_ShutdownWeaponDefFiles() {}  // provided by bg_weapons.cpp now
// int  BG_AnimScriptEvent(playerState_s * /*ps*/, scriptAnimEventTypes_t /*event*/, int /*isContinue*/, int /*force*/) { return 0; }  // provided by bg_animation_mp.cpp now
// void BG_AddPredictableEventToPlayerstate(unsigned int /*event*/, unsigned int /*eventParm*/, playerState_s * /*ps*/) {}  // provided by bg_misc.cpp now
// int  PM_GetEffectiveStance(const playerState_s * /*ps*/) { return 0; }  // provided by bg_pmove.cpp now
// unsigned int PM_GroundSurfaceType(pml_t * /*pml*/) { return 0; }  // provided by bg_pmove.cpp now

// =========================================================================
// xanim/bgame cascade — landed with the xanim + bgame files. Math
// helpers get real implementations; subsystem hooks stay stubbed.
// =========================================================================

// void Vec3Clear(float *v) { v[0] = v[1] = v[2] = 0; }  // provided by com_math.cpp now
// void Vec3Copy(const float *in, float *out) { out[0] = in[0]; out[1] = in[1]; out[2] = in[2]; }  // provided by com_math.cpp now
// void Vec3Mul(const float *a, const float *b, float *out) { out[0] = a[0] * b[0]; out[1] = a[1] * b[1]; out[2] = a[2] * b[2]; }  // provided by com_math.cpp now
// float Vec2Length(const float *v) { return std::sqrt(v[0] * v[0] + v[1] * v[1]); }  // provided by com_math.cpp now

// AngleNormalize360, AngleDelta, Q_rint, vectoangles, vectoyaw,
// VectorAngleMultiply, QuatMultiplyEquals/ReverseEquals/ReverseInverse,
// ConvertQuatToMat / ConvertQuatToInverseMat, MatrixTransformVectorQuatTransEquals
// all provided by src/universal/com_math.cpp now.

// DObj
// void DObjCalcAnim(const DObj_s * /*obj*/, int * /*partBits*/) {}
// void DObjDumpInfo(const DObj_s * /*obj*/) {}
// void DObjGetHidePartBits(const DObj_s * /*obj*/, unsigned int * /*partBits*/) {}

// PM_
// void PM_AddTouchEnt(pmove_t * /*pm*/, int /*entityNum*/) {}  // provided by bg_pmove.cpp now
// PM_ClipVelocity provided by src/bgame/bg_pmove.cpp now.
// PM_ProjectVelocity provided by src/bgame/bg_pmove.cpp now.
// void PM_FootstepEvent(pmove_t * /*pm*/, pml_t * /*pml*/, char /*surfType*/, char /*step*/, int /*eventParm*/) {}  // provided by bg_pmove.cpp now
// bool PM_ShouldMakeFootsteps(pmove_t * /*pm*/) { return false; }  // provided by bg_pmove.cpp now
// PM_playerTrace provided by src/bgame/bg_pmove.cpp now.
// PM_trace provided by src/bgame/bg_pmove.cpp now.

// BG
// int  BG_AnimScriptAnimation(playerState_s * /*ps*/, aistateEnum_t /*state*/, scriptAnimMoveTypes_t /*move*/, int /*direction*/) { return 0; }  // provided by bg_animation_mp.cpp now
// BG_CheckProne provided by src/bgame/bg_misc.cpp now.
// void BG_CreateXAnim(XAnim_s * /*anims*/, unsigned int /*animIndex*/, const char * /*name*/) {}  // provided by bg_misc.cpp now
// void BG_InitWeaponString(int /*weaponIndex*/, const char * /*str*/) {}  // provided by bg_animation_mp.cpp now

// Misc collision / config helpers
struct SimplePlaneIntersection_fwd;
struct adjacencyWinding_t_fwd;
void BuildBrushdAdjacencyWindingForSide(float * /*points*/, int /*numPoints*/,
                                        const void * /*spi*/, int /*numIntersections*/,
                                        void * /*winding*/, int /*sideIndex*/) {}
bool DB_IsXAssetDefault(XAssetType /*type*/, const char * /*name*/) { return true; }
snd_alias_list_t *Com_FindSoundAlias(const char * /*name*/) { return nullptr; }
// char *Com_LoadRawTextFile(const char * /*filename*/) { return nullptr; }
// void Com_UnloadRawTextFile(char * /*buffer*/) {}
// Com_SurfaceTypeToName provided by src/universal/surfaceflags.cpp now.
// Com_sprintfPos provided by src/universal/q_shared.cpp now.
// bool Info_Validate(const char * /*s*/) { return true; }  // provided by q_shared.cpp now
// int  I_strcmp(const char *a, const char *b) { return std::strcmp(a, b); }  // provided by q_shared.cpp now
// bool ParseConfigStringToStructCustomSize(unsigned char * /*pStruct*/, const cspField_t * /*pFieldList*/,  // provided by q_shared.cpp now
                                         // int /*iNumFields*/, char * /*pszBuffer*/, int /*iMaxFieldTypes*/,
                                         // int  (*)(unsigned char *, const char *, const int) /*parseSpecial*/,
                                         // void (*)(unsigned char *, const char *) /*parseStrcpy*/)
// { return false; }

// FS - the read API path
// unsigned int FS_FOpenFileByMode(char * /*qpath*/, int *file, fsMode_t /*mode*/) { if (file) *file = 0; return 0; }  // provided by com_files.cpp now
// unsigned int FS_Read(unsigned char * /*buffer*/, unsigned int /*len*/, int /*file*/) { return 0; }  // provided by com_files.cpp now

// Hunk family
// void *Hunk_AllocDebugMem(unsigned int size) { return std::malloc(size); }
// void  Hunk_FreeDebugMem(void *ptr) { std::free(ptr); }
// unsigned char *Hunk_AllocLow(unsigned int size, const char * /*name*/, int /*type*/)
// {
//     return static_cast<unsigned char *>(std::calloc(1, size));
// }
// unsigned char *Hunk_AllocLowAlign(unsigned int size, int /*align*/, const char * /*name*/, int /*type*/)
// {
//     return static_cast<unsigned char *>(std::calloc(1, size));
// }
// void  Hunk_AddData(int /*type*/, void * /*data*/, void *(*)(int) /*alloc*/) {}
// bool  Hunk_DataOnHunk(unsigned char * /*data*/) { return false; }
// void *Hunk_FindDataForFile(int /*fileId*/, const char * /*filename*/) { return nullptr; }
// char *Hunk_SetDataForFile(int /*type*/, const char * /*name*/, void * /*data*/, void *(*)(int) /*alloc*/) { return nullptr; }

// Stringlist (extra entries pulled by bgame/xanim)
// void SL_AddRefToString(unsigned int /*stringValue*/) {}  // provided by scr_variable/scr_stringlist now
// unsigned int SL_ConvertToLowercase(unsigned int stringValue, unsigned int /*user*/, int /*type*/) { return stringValue; }  // provided by scr_variable/scr_stringlist now
// unsigned int SL_GetLowercaseString(const char * /*str*/, unsigned int /*user*/) { return 0; }  // provided by scr_variable/scr_stringlist now
// unsigned int SL_GetStringOfSize(const char * /*str*/, unsigned int /*size*/, unsigned int /*user*/, int /*type*/) { return 0; }  // provided by scr_variable/scr_stringlist now
// void SL_RemoveRefToStringOfSize(unsigned int /*stringValue*/, unsigned int /*size*/) {}  // provided by scr_variable/scr_stringlist now

// Scr_*
void Scr_AddArray() {}
void Scr_AddConstString(unsigned int /*stringValue*/) {}
void Scr_AddFloat(float /*value*/) {}
// Scr_Notify provided by src/game_mp/g_spawn_mp.cpp now.

// XAnim/XModel load
XAnimParts *XAnimLoadFile(char * /*name*/, void *(*)(int) /*Alloc*/) { return nullptr; }
XModel *XModelPrecache_LoadObj(char * /*name*/, void *(*)(int) /*Alloc*/, void *(*)(int) /*AllocColl*/) { return nullptr; }
// void XAnim_CalcDeltaForTime(const XAnimParts * /*part*/, float /*time*/, float * /*deltaTrans*/, float4 * /*deltaQuat*/) {}

// Globals
// int surfaceTypeSoundListCount = 0;  // provided by bg_weapons.cpp now

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
// CL_GetMapCenter provided by src/client_mp/cl_main_mp.cpp now.
// CL_GetPredictedOriginForServerTime provided by src/client_mp/cl_parse_mp.cpp now.

// char *BG_GetEntityTypeName(int /*eType*/) { return const_cast<char *>(""); }  // provided by bg_misc.cpp now

// MSG_
// const NetFieldList *MSG_GetStateFieldListForEntityType(int /*eType*/) { return nullptr; }  // provided by sv_msg_write_mp.cpp now

// Com_
// char *Com_LoadInfoString(char * /*filename*/, const char * /*fileDesc*/, const char * /*ident*/, char * /*loadBuffer*/) { return nullptr; }

// DevGui
// void DevGui_AddGraph(const char * /*name*/, DevGraph * /*graph*/) {}

// FS_
// void FS_BuildOSPath(const char * /*base*/, const char * /*game*/, const char * /*qpath*/, char *ospath)  // provided by com_files.cpp now
// { if (ospath) ospath[0] = 0; }
// int  FS_CreatePath(char * /*OSPath*/) { return 0; }  // provided by com_files.cpp now
// void FS_FreeFileList(const char ** /*list*/) {}  // provided by com_files.cpp now
// int  FS_OpenFileOverwrite(char * /*filename*/) { return 0; }  // provided by com_files.cpp now
// unsigned int FS_Write(const char * /*buffer*/, unsigned int /*len*/, int /*h*/) { return 0; }  // provided by com_files.cpp now
// int  FS_WriteFileToDir(const char * /*qpath*/, const char * /*dir*/, char * /*buffer*/, unsigned int /*size*/) { return 0; }  // provided by com_files.cpp now

// I_str
// unsigned char I_CleanChar(unsigned char c) { return c; }  // provided by q_shared.cpp now

// LiveStorage
void LiveStorage_NewUser() {}
void LiveStorage_ReadStats() {}

// Sys_
const char *Sys_DefaultInstallPath() { return "."; }
void Sys_RemoveDirTree(const char * /*path*/) {}

// UI text-draw helpers (used by ProfLoad overlay)
// UI_DrawText provided by src/ui_mp/ui_main_mp.cpp now.
// UI_FillRect provided by src/ui/ui_atoms.cpp now.
// Font_s *UI_GetFontHandle(const ScreenPlacement * /*place*/, int /*fontIndex*/, float /*scale*/) { return nullptr; }  // provided by ui_main_mp.cpp now

// Win_LocalizeRef
const char *Win_LocalizeRef(const char *str) { return str; }

// Z_MallocGarbage: GP allocator variant — same as malloc for us.
// char *Z_MallocGarbage(int size, const char * /*name*/, int /*type*/)
// {
//     return static_cast<char *>(std::malloc(static_cast<size_t>(size)));
// }

// TRACK_* mem-tracking thunks. Each TRACK_<subsystem>() declares its
// statically-allocated buffers to the mem_track system; with mem-tracking
// off (or stubbed) these are all no-ops.
// void TRACK_cl_console() {}  // provided by cl_console.cpp now
// TRACK_cl_input provided by src/client_mp/cl_input.cpp now.
// void TRACK_cl_keys() {}  // provided by cl_keys.cpp now
// TRACK_cl_main provided by src/client_mp/cl_main_mp.cpp now.
// TRACK_cl_parse provided by src/client_mp/cl_parse_mp.cpp now.
// void TRACK_cm_world() {}
// void TRACK_com_math() {}  // provided by com_math.cpp now
void TRACK_db_registry() {}
// void TRACK_devgui() {}
// void TRACK_dobj_management() {}
// void TRACK_fx_marks() {}
// void TRACK_fx_random() {}  // provided by fx_random.cpp now
void TRACK_fx_system() {}
// void TRACK_missile_attractors() {}  // provided by g_missile.cpp now
// void TRACK_msg() {}  // provided by sv_msg_write_mp.cpp now
void TRACK_phys() {}
// void TRACK_q_shared() {}  // provided by q_shared.cpp now
void TRACK_r_buffers() {}
// void TRACK_r_debug() {}
void TRACK_r_dpvs() {}
// void TRACK_r_font() {}
// void TRACK_r_image_wavelet() {}  // provided by r_image_wavelet.cpp now
void TRACK_r_image() {}
void TRACK_r_init() {}
void TRACK_r_material() {}
// void TRACK_r_model() {}
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
// void TRACK_stringed_hooks() {}  // provided by stringed_hooks.cpp now
// TRACK_sv_game now in server/sv_game.cpp.
// TRACK_sv_main provided by src/server_mp/sv_main_mp.cpp now.
// void TRACK_ui_main() {}  // provided by ui_main_mp.cpp now
// void TRACK_ui_shared() {}  // provided by ui_shared.cpp now
// TRACK_ui_utils now in ui/ui_utils.cpp.
void TRACK_win_net() {}
void TRACK_xmodel() {}

// Globals for the new batch.
// cl_shownet provided by src/client_mp/cl_main_mp.cpp now.
// msg_dumpEnts provided by src/qcommon/net_chan_mp.cpp now.
// msg_printEntityNums provided by src/qcommon/net_chan_mp.cpp now.
// clients provided by src/client_mp/cl_main_mp.cpp now.
// huffman_t msgHuff{};  // provided by sv_msg_write_mp.cpp now
unsigned int msecPerRawTimerTick = 1;
// netFieldOrderInfo_t orderInfo{};  // provided by sv_msg_write_mp.cpp now
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
// int  Key_IsDown(int /*localClientNum*/, int /*key*/) { return 0; }  // provided by cl_keys.cpp now
// CG_TraceCapsule provided by src/cgame/cg_world.cpp now.
// CG_DObjGetWorldTagPos provided by src/cgame_mp/cg_ents_mp.cpp now.

// FX visibility
double FX_GetClientVisibility(int /*localClientNum*/, const float * /*origin*/, const float * /*viewOrigin*/) { return 1.0; }

// BG
// BG_EvaluateTrajectory provided by src/bgame/bg_misc.cpp now.

// DevGui
// void DevGui_Toggle() {}

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
// int   R_TextHeight(Font_s * /*font*/) { return 0; }
// int   R_TextWidth(const char * /*text*/, int /*max*/, Font_s * /*font*/) { return 0; }
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
// unsigned int SL_GetString(const char * /*str*/, unsigned int /*user*/) { return 0; }  // provided by scr_variable/scr_stringlist now
// void SL_AddUser(unsigned int /*stringValue*/, unsigned int /*user*/) {}  // provided by scr_variable/scr_stringlist now

// Math
// RadiusFromBounds provided by src/universal/com_math.cpp now.

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

// CalculateRanks provided by src/game_mp/g_main_mp.cpp now.
// CL_IsClientLocal provided by src/client_mp/cl_main_mp.cpp now.
// CL_IsPlayerMuted provided by src/client_mp/cl_main_pc_mp.cpp now.
// ClientUserinfoChanged provided by src/game_mp/g_client_mp.cpp now.
// void Com_SafeClientDObjFree(unsigned int /*handle*/, int /*localClientNum*/) {}
// char *FS_LoadedIwdPureChecksums() { static char empty[1] = {0}; return empty; }  // provided by com_files.cpp now
// GScr_GetHeadIconIndex provided by src/game_mp/g_scr_main_mp.cpp now.
// GScr_GetStatusIconIndex provided by src/game_mp/g_scr_main_mp.cpp now.
// int  I_stricmpwild(const char *s0, const char *s1) { return strcasecmp(s0 ? s0 : "", s1 ? s1 : ""); }  // provided by q_shared.cpp now
// IN_IsTalkKeyHeld provided by src/client_mp/cl_input.cpp now.
bool Material_IsDefault(const Material * /*material*/) { return true; }
// bool NET_OutOfBandVoiceData(netsrc_t /*sock*/, netadr_t /*adr*/, unsigned char * /*data*/, unsigned int /*len*/) { return false; }  // provided by net_chan_mp.cpp now
// bool Netchan_Transmit(netchan_t * /*chan*/, int /*length*/, char * /*data*/) { return false; }  // provided by net_chan_mp.cpp now
// bool Netchan_TransmitNextFragment(netchan_t * /*chan*/) { return false; }  // provided by net_chan_mp.cpp now

// void NetProf_AddPacket(netProfileStream_t * /*stream*/, int /*size*/, int /*type*/) {}  // provided by net_chan_mp.cpp now
// void NetProf_PrepProfiling(netProfileInfo_t * /*info*/) {}  // provided by net_chan_mp.cpp now
// void NetProf_UpdateStatistics(netProfileStream_t * /*stream*/) {}  // provided by net_chan_mp.cpp now

// PerpendicularVector provided by src/universal/com_math.cpp now.

// void R_CopyDebugLines(trDebugLine_t * /*dst*/, int /*dstCap*/, trDebugLine_t * /*src*/, int /*count*/, int /*offset*/) {}
// void R_CopyDebugStrings(trDebugString_t * /*dst*/, int /*dstCap*/, trDebugString_t * /*src*/, int /*count*/, int /*offset*/) {}
// void R_DebugAlloc(void **out, int size, const char * /*name*/) { if (out) *out = std::calloc(1, size); }  // provided by gfx_d3d batch now
// void R_DebugFree(void **p) { if (p && *p) { std::free(*p); *p = nullptr; } }  // provided by gfx_d3d batch now
// void R_ShutdownDebug() {}

// Scr_*
// void Scr_AddClassField(unsigned int /*classnum*/, char * /*name*/, unsigned int /*offset*/) {}  // provided by scr_variable/scr_stringlist now
void Scr_AddInt(int /*value*/) {}
void Scr_AddString(const char * /*value*/) {}
void Scr_Error(const char * /*msg*/) {}
unsigned int Scr_GetConstString(unsigned int /*paramIndex*/) { return 0; }
float Scr_GetFloat(unsigned int /*paramIndex*/) { return 0; }
int   Scr_GetInt(unsigned int /*paramIndex*/) { return 0; }
const char *Scr_GetString(unsigned int /*paramIndex*/) { return ""; }
// Scr_GetGenericField provided by src/game_mp/g_spawn_mp.cpp now.
// Scr_SetGenericField provided by src/game_mp/g_spawn_mp.cpp now.

// StringTable
// void StringTable_GetAsset(const char * /*filename*/, StringTable ** /*outTable*/) {}
// const char *StringTable_Lookup(const StringTable * /*table*/, int /*column*/, const char * /*key*/, int /*colCount*/) { return nullptr; }

// SV
// SV_CloseDownload provided by src/server_mp/sv_client_mp.cpp now.
// SV_Download_Clear provided by src/server_mp/sv_snapshot_mp.cpp now.
// SV_DropClient provided by src/server_mp/sv_client_mp.cpp now.
// SV_GetConfigstring provided by src/server_mp/sv_init_mp.cpp now.
// SV_SendClientGameState provided by src/server_mp/sv_client_mp.cpp now.

// Voice
void Voice_IncomingVoiceData(unsigned char /*from*/, unsigned char * /*data*/, int /*len*/) {}
bool Voice_IsClientTalking(unsigned int /*clientNum*/) { return false; }

// Globals — typed where we have the type (since game_public.h pulls
// most of the cgame/game/server headers in).
// cg_entitiesArray provided by src/cgame_mp/cg_main_mp.cpp now.
// cl_showSend provided by src/client_mp/cl_main_mp.cpp now.
// cl_voice provided by src/client_mp/cl_main_mp.cpp now.
// g_entities / level provided by src/game_mp/g_main_mp.cpp now.
// const dvar_t *net_profile = nullptr;  // provided by net_chan_mp.cpp now
// sv_maxclients provided by src/server_mp/sv_main_mp.cpp now.
// sv_voice provided by src/server_mp/sv_init_mp.cpp now.
// svs provided by src/server_mp/sv_main_mp.cpp now.

// =========================================================================
// Misc-2 batch — sv_game / g_svcmds / cg_consolecmds_mp cascade.
// =========================================================================

struct XBoneInfo;

// shellshock_parms_t *BG_GetShellshockParms(unsigned int /*index*/) { return nullptr; }  // provided by bg_misc.cpp now
// int  BG_LoadShellShockDvars(const char * /*name*/) { return 0; }  // provided by bg_misc.cpp now
// int  BG_SaveShellShockDvars(const char * /*name*/) { return 0; }  // provided by bg_misc.cpp now
// void BG_SetShellShockParmsFromDvars(shellshock_parms_t * /*parms*/) {}  // provided by bg_misc.cpp now

// bool BoxDistSqrdExceeds(const float * /*center*/, const float * /*mins*/, const float * /*maxs*/, float /*distSqrd*/) { return true; }  // provided by com_math.cpp now

// void CG_ActionSlotDown_f() {}  // provided by cg_weapons.cpp / cg_ammocounter.cpp now
// void CG_ActionSlotUp_f() {}  // provided by cg_weapons.cpp / cg_ammocounter.cpp now
// CG_FxSetTestPosition / CG_FxTest provided by src/cgame_mp/cg_view_mp.cpp now.
// CG_IsScoreboardDisplayed provided by src/cgame_mp/cg_scoreboard_mp.cpp now.
// void CG_NextWeapon_f() {}  // provided by cg_weapons.cpp / cg_ammocounter.cpp now
// void CG_PrevWeapon_f() {}  // provided by cg_weapons.cpp / cg_ammocounter.cpp now
// CG_RestartSmokeGrenades provided by src/cgame_mp/cg_main_mp.cpp now.
// CL_AddReliableCommand provided by src/client_mp/cl_main_mp.cpp now.

// void Com_GetBspFilename(char *filename, unsigned int /*max*/, const char * /*mapname*/)  // provided by com_files.cpp now
// { if (filename) filename[0] = 0; }
// DObj_s *Com_GetServerDObj(unsigned int /*handle*/) { return nullptr; }
void Com_UnloadSoundAliases(snd_alias_system_t /*sys*/) {}

// ConcatArgs provided by src/game_mp/g_cmds_mp.cpp now.

// void DObjCreateSkel(DObj_s * /*obj*/, char * /*partBits*/, int /*controlPartBits*/) {}
// unsigned int DObjGetAllocSkelSize(const DObj_s * /*obj*/) { return 0; }
// void DObjGetBoneInfo(const DObj_s * /*obj*/, XBoneInfo ** /*info*/) {}
// void DObjGetBounds(const DObj_s * /*obj*/, float *mins, float *maxs)
// {
//     if (mins) mins[0] = mins[1] = mins[2] = 0;
//     if (maxs) maxs[0] = maxs[1] = maxs[2] = 0;
// }
// const XModel *DObjGetModel(const DObj_s * /*obj*/, int /*modelIndex*/) { return nullptr; }
// unsigned int DObjGetNumModels(const DObj_s * /*obj*/) { return 0; }
// XAnimTree_s *DObjGetTree(const DObj_s * /*obj*/) { return nullptr; }
// bool DObjIgnoreCollision(const DObj_s * /*obj*/, char /*modelIndex*/) { return false; }
// unsigned int DObjNumBones(const DObj_s * /*obj*/) { return 0; }
// void DObjSkelAreBonesUpToDate(const DObj_s * /*obj*/, int * /*partBits*/) {}
// bool DObjSkelExists(const DObj_s * /*obj*/, int /*boneIndex*/) { return false; }
// bool DObjSkelIsBoneUpToDate(DObj_s * /*obj*/, int /*boneIndex*/) { return false; }

// G_GetEntityTypeName provided by src/game_mp/g_utils_mp.cpp now.
// G_GetFogOpaqueDistSqrd provided by src/game_mp/g_main_mp.cpp now.
// G_GetSavePersist provided by src/game_mp/g_main_mp.cpp now.
// G_InitGame provided by src/game_mp/g_main_mp.cpp now.
// void G_ResetEntityParsePoint() {}  // provided by game/ batch now
// G_ShutdownGame provided by src/game_mp/g_main_mp.cpp now.

// MatrixTransformVector43 provided by src/universal/com_math.cpp now.

// bool NET_IsLocalAddress(netadr_t /*adr*/) { return false; }  // provided by net_chan_mp.cpp now
// Scr_IsValidGameType provided by src/game_mp/g_scr_main_mp.cpp now.

// unsigned int SV_ClipHandleForEntity(const gentity_s * /*ent*/) { return 0; }  // provided by sv_world.cpp now
// SV_GetMapBaseName provided by src/server_mp/sv_ccmds_mp.cpp now.
// void SV_LinkEntity(gentity_s * /*ent*/) {}  // provided by sv_world.cpp now
// SV_SendServerCommand provided by src/server_mp/sv_main_mp.cpp now.
// SV_SetConfigstring provided by src/server_mp/sv_init_mp.cpp now.

unsigned int Sys_MillisecondsRaw() { return Sys_Milliseconds(); }

// uiMenuCommand_t UI_GetActiveMenu(int /*localClientNum*/) { return uiMenuCommand_t{}; }  // provided by ui_main_mp.cpp now
// int  UI_Popup(int /*localClientNum*/, const char * /*ref*/) { return 0; }  // provided by ui_main_mp.cpp now
// char *UI_SafeTranslateString(const char *str) { return const_cast<char *>(str ? str : ""); }  // provided by ui_main_mp.cpp now

// Vec2DistanceSq provided by src/universal/com_math.cpp now.

// g_banIPs provided by src/game_mp/g_main_mp.cpp now.
// g_dedicated provided by src/game_mp/g_main_mp.cpp now.
// sv_gametype provided by src/server_mp/sv_main_mp.cpp now.
// =========================================================================
// Small batch — cl_net_chan_mp / cl_pose_mp / sv_main_pc_mp / g_scr_mover.
// =========================================================================

// AxisToAngles provided by src/universal/com_math.cpp now.


// G_DObjUpdate provided by src/game_mp/g_utils_mp.cpp now.
// G_FreeEntity provided by src/game_mp/g_utils_mp.cpp now.

// void *I_dmaGetDObjSkel(const DObj_s * /*obj*/) { return nullptr; }

// const char *NET_AdrToString(netadr_t /*adr*/) { return ""; }  // provided by net_chan_mp.cpp now
// bool NET_CompareBaseAdr(netadr_t /*a*/, netadr_t /*b*/) { return false; }  // provided by net_chan_mp.cpp now
// bool NET_OutOfBandPrint(netsrc_t /*sock*/, netadr_t /*adr*/, const char * /*data*/) { return false; }  // provided by net_chan_mp.cpp now
// int  NET_StringToAdr(char * /*str*/, netadr_t * /*adr*/) { return 0; }  // provided by net_chan_mp.cpp now

unsigned int Scr_GetNumParam() { return 0; }
void Scr_GetVector(unsigned int /*paramIndex*/, float *v)
{
    if (v) v[0] = v[1] = v[2] = 0;
}
// Scr_Notify provided by src/game_mp/g_spawn_mp.cpp now.
void Scr_ObjectError(const char * /*msg*/) {}
void Scr_ParamError(unsigned int /*paramIndex*/, const char * /*msg*/) {}

// SV_FindClientByAddress provided by src/server_mp/sv_main_mp.cpp now.
// SV_PreGameUserVoice / SV_UserVoice provided by src/server_mp/sv_voice_mp.cpp now.
// SVC_GameCompleteStatus provided by src/server_mp/sv_main_mp.cpp now.

// cl_profileTextHeight provided by src/client_mp/cl_main_mp.cpp now.
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
// void dNormalize3(dVector3 /*v*/) {}  // provided by odemath.cpp now
} // extern "C"

// CG_DebugBox now in cgame/cg_drawtools.cpp.
// CG_DebugLine now in cgame/cg_drawtools.cpp.

// void ClosestApproachOfTwoLines(const float * /*p1*/, const float * /*d1*/, const float * /*p2*/, const float * /*d2*/, float *t1, float *t2)  // provided by com_math.cpp now
// { if (t1) *t1 = 0; if (t2) *t2 = 0; }

// I_strupr provided by src/universal/q_shared.cpp now.

// float kisak_random() { return std::rand() / float(RAND_MAX); }  // provided by com_math.cpp now

// bool ParseConfigStringToStruct(unsigned char * /*pStruct*/, const cspField_t * /*pFieldList*/,  // provided by q_shared.cpp now
                               // int /*iNumFields*/, char * /*pszBuffer*/, int /*iMaxFieldTypes*/,
                               // int  (*)(unsigned char *, const char *, const int) /*parseSpecial*/,
                               // void (*)(unsigned char *, const char *) /*parseStrcpy*/) { return false; }

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

// void Vec3Negate(const float *in, float *out) { out[0] = -in[0]; out[1] = -in[1]; out[2] = -in[2]; }  // provided by com_math.cpp now
// void Vec4Copy(const float *in, float *out) { out[0] = in[0]; out[1] = in[1]; out[2] = in[2]; out[3] = in[3]; }  // provided by com_math.cpp now

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
// SV_ClearPacketAnalysis provided by src/server_mp/sv_snapshot_profile_mp.cpp now.
// SV_GetClientSnapshotPing provided by src/server_mp/sv_snapshot_profile_mp.cpp now.
// SV_NewPacketAnalysisReady provided by src/server_mp/sv_snapshot_profile_mp.cpp now.

// UI_DrawHandlePic provided by src/ui/ui_atoms.cpp now.
// int   UI_TextHeight(Font_s * /*font*/, float /*scale*/) { return 0; }  // provided by ui_main_mp.cpp now
// int   UI_TextWidth(const char * /*text*/, int /*max*/, Font_s * /*font*/, float /*scale*/) { return 0; }  // provided by ui_main_mp.cpp now

// cg_drawLagometer provided by src/cgame_mp/cg_main_mp.cpp now.
// cg_nopredict provided by src/cgame_mp/cg_main_mp.cpp now.
// cg_packetAnalysisClient provided by src/cgame_mp/cg_main_mp.cpp now.
// cg_packetAnalysisEntTextScale provided by src/cgame_mp/cg_main_mp.cpp now.
// cg_packetAnalysisEntTextY provided by src/cgame_mp/cg_main_mp.cpp now.
// cg_packetAnalysisTextScale provided by src/cgame_mp/cg_main_mp.cpp now.
// cg_packetAnalysisTextY provided by src/cgame_mp/cg_main_mp.cpp now.
// cg_synchronousClients provided by src/cgame_mp/cg_main_mp.cpp now.
// const dvar_t *net_showprofile = nullptr;  // provided by net_chan_mp.cpp now

// g_bitsSent provided by src/server_mp/sv_snapshot_profile_mp.cpp now.
// pulled in by stub deps.
// g_bitsSent provided by src/server_mp/sv_snapshot_profile_mp.cpp now.
// g_currentSnapshotPerEntity provided by src/server_mp/sv_snapshot_profile_mp.cpp now.
// g_currentSnapshotFieldsPerEntity provided by src/server_mp/sv_snapshot_profile_mp.cpp now.
// g_currentSnapshotPlayerStateFields provided by src/server_mp/sv_snapshot_profile_mp.cpp now.

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
// void CG_HoldBreathInit(cg_s * /*cg*/) {}  // provided by cg_weapons.cpp / cg_ammocounter.cpp now
// CG_MenuShowNotify provided by src/cgame_mp/cg_servercmds_mp.cpp now.
// CG_ObjectiveIcon / CG_ResetLowHealthOverlay provided by src/cgame_mp/cg_newDraw_mp.cpp now.
// CG_SetEquippedOffHand provided by src/cgame/offhandweapons.cpp now.

// CL_DrawText / CL_DrawTextRotate provided by src/client_mp/cl_main_mp.cpp now.
// CL_IsServerLoadingMap provided by src/client_mp/cl_main_mp.cpp now.
// CL_IsWaitingOnServerToLoadMap provided by src/client_mp/cl_main_mp.cpp now.
// CL_SetStance provided by src/client_mp/cl_input.cpp now.
// CL_SetWaitingOnServerToLoadMap provided by src/client_mp/cl_main_mp.cpp now.

float DB_GetLoadedFraction() { return 1.0f; }

// IsExpressionTrue provided by src/ui/ui_expressions.cpp now.
// const rectDef_s *Item_GetTextRect(int /*localClientNum*/, const itemDef_s * /*item*/) { return nullptr; }  // provided by ui_shared.cpp now

// float kisak_crandom() { return (std::rand() / float(RAND_MAX)) * 2.0f - 1.0f; }  // provided by com_math.cpp now

// SCR_UpdateLoadScreen provided by src/client_mp/cl_scrn_mp.cpp now.
// int  String_Parse(const char ** /*p*/, char * /*out*/, int /*outSize*/) { return 0; }  // provided by ui_shared.cpp now
// void UI_CloseAllMenus(int /*localClientNum*/) {}  // provided by ui_main_mp.cpp now
// void UI_DrawMapLevelshot(int /*localClientNum*/) {}  // provided by ui_main_mp.cpp now
// UI_DrawTextNoSnap provided by src/ui_mp/ui_main_mp.cpp now.

// Vec2NormalizeTo provided by src/universal/com_math.cpp now.

// bool Window_IsVisible(int /*localClientNum*/, const windowDef_t * /*window*/) { return false; }  // provided by ui_shared.cpp now

// YawVectors2D provided by src/universal/com_math.cpp now.

// const dvar_t *bg_viewKickMax = nullptr;  // provided by bg_misc.cpp now
// const dvar_t *bg_viewKickMin = nullptr;  // provided by bg_misc.cpp now
// const dvar_t *bg_viewKickScale = nullptr;  // provided by bg_misc.cpp now
// cg_hudDamageIconTime provided by src/cgame_mp/cg_main_mp.cpp now.
// hud_fade_compass provided by src/cgame_mp/cg_newDraw_mp.cpp now.
// const dvar_t *uiscript_debug = nullptr;  // provided by ui_main_mp.cpp now
// g_waitingForServer provided by src/client_mp/cl_main_mp.cpp now.

// =========================================================================
// cl_cgame_mp + cg_compassfriendlies_mp + cg_visionsets cascade.
// =========================================================================

struct MemoryFile;

// unsigned int BG_GetViewmodelWeaponIndex(const playerState_s * /*ps*/) { return 0; }  // provided by bg_weapons.cpp now
// WeaponDef *BG_GetWeaponDef(unsigned int /*weaponIndex*/) { return nullptr; }  // provided by bg_weapons.cpp now

// CG_ArchiveState provided by src/cgame_mp/cg_newDraw_mp.cpp now.
// CG_Init provided by src/cgame_mp/cg_main_mp.cpp now.
// CG_RegisterSounds provided by src/cgame_mp/cg_main_mp.cpp now.
// CG_Shutdown provided by src/cgame_mp/cg_main_mp.cpp now.
// CL_AnyLocalClientsRunning provided by src/client_mp/cl_main_mp.cpp now.
// CL_DisconnectError provided by src/client_mp/cl_main_mp.cpp now.
// CL_ParseServerMessage / CL_SystemInfoChanged provided by src/client_mp/cl_parse_mp.cpp now.
// CL_WasMapAlreadyLoaded provided by src/client_mp/cl_main_mp.cpp now.

// void CM_LinkWorld() {}
// unsigned char ColorIndex(unsigned char /*c*/) { return 7; }  // provided by q_shared.cpp now
snd_alias_t *Com_PickSoundAlias(const char * /*name*/) { return nullptr; }
// void Com_TouchMemory() {}

// void Con_ClearNotify(int /*localClientNum*/) {}  // provided by cl_console.cpp now
// void Con_Close(int /*localClientNum*/) {}  // provided by cl_console.cpp now
// void Con_InitMessageBuffer() {}  // provided by cl_console.cpp now
// void Con_TimeJumped(int /*localClientNum*/, int /*time*/) {}  // provided by cl_console.cpp now
// void Con_TimeNudged(int /*localClientNum*/, int /*delta*/) {}  // provided by cl_console.cpp now

void DB_EnumXAssets(XAssetType /*type*/, void (*)(XAssetHeader, void*) /*cb*/, void * /*ctx*/, bool /*loaded*/) {}
// void DevGui_AddCommand(const char * /*name*/, char * /*menu*/) {}
// void FX_Archive(int, MemoryFile*) {}  // provided by fx_archive.cpp now

// const char *Info_ValueForKey(const char * /*s*/, const char * /*key*/) { return ""; }  // provided by q_shared.cpp now

// LargeLocal — provided by com_memory.cpp now.

void R_AddCmdDrawStretchPicFlipST(float /*x*/, float /*y*/, float /*w*/, float /*h*/,
                                  float /*s0*/, float /*t0*/, float /*s1*/, float /*t1*/,
                                  const float * /*color*/, Material * /*material*/) {}
void R_AddCmdDrawStretchPicRotateST(float /*x*/, float /*y*/, float /*w*/, float /*h*/,
                                    float /*s0*/, float /*t0*/, float /*s1*/, float /*t1*/,
                                    float /*rotationS*/, float /*rotationT*/,
                                    const float * /*color*/, Material * /*material*/) {}
// void R_ArchiveFogState(MemoryFile * /*memFile*/) {}  // provided by gfx_d3d batch now
void R_EndRegistration() {}
void R_LoadWorld(char * /*name*/, int * /*checksum*/, int /*flag*/) {}
void R_RenderScene(const refdef_s * /*refdef*/) {}
void R_UpdateTeamColors(int /*localClientNum*/, const float * /*color1*/, const float * /*color2*/) {}

// char *SEH_SafeTranslateString(char *str) { return str ? str : (char*)""; }  // provided by stringed_hooks.cpp now
// const char *SEH_StringEd_GetString(const char *str) { return str ? str : ""; }  // provided by stringed_hooks.cpp now

// void UI_CloseAll(int /*localClientNum*/) {}  // provided by ui_main_mp.cpp now
// char *UI_ReplaceConversionString(char *src, const char * /*replace*/) { return src; }  // provided by ui_main_mp.cpp now

// cl_activeAction provided by src/client_mp/cl_main_mp.cpp now.
// cl_freezeDemo provided by src/client_mp/cl_main_mp.cpp now.
// cl_serverLoadingMap provided by src/client_mp/cl_main_mp.cpp now.
// cl_showServerCommands provided by src/client_mp/cl_main_mp.cpp now.
// cl_showTimeDelta provided by src/client_mp/cl_main_mp.cpp now.
// const dvar_t *loc_warnings = nullptr;  // provided by stringed_hooks.cpp now
// const dvar_t *loc_warningsAsErrors = nullptr;  // provided by stringed_hooks.cpp now
// nextdemo provided by src/client_mp/cl_main_mp.cpp now.
// sv_archive_mp cascade.
struct SnapshotInfo_s;
struct clientState_s;
struct archivedEntity_s;

// void MSG_WriteDeltaArchivedEntity(SnapshotInfo_s * /*info*/, msg_t * /*msg*/, int /*time*/, archivedEntity_s * /*from*/, archivedEntity_s * /*to*/, int /*force*/) {}  // provided by sv_msg_write_mp.cpp now
// void MSG_WriteDeltaClient(SnapshotInfo_s * /*info*/, msg_t * /*msg*/, int /*time*/, clientState_s * /*from*/, clientState_s * /*to*/, int /*force*/) {}  // provided by sv_msg_write_mp.cpp now
// void MSG_WriteDeltaPlayerstate(SnapshotInfo_s * /*info*/, msg_t * /*msg*/, int /*time*/, const playerState_s * /*from*/, const playerState_s * /*to*/) {}  // provided by sv_msg_write_mp.cpp now
// void MSG_WriteEntityIndex(SnapshotInfo_s * /*info*/, msg_t * /*msg*/, int /*newnum*/, int /*indexBits*/) {}  // provided by sv_msg_write_mp.cpp now
// SV_PacketDataIsNotNetworkData provided by src/server_mp/sv_snapshot_profile_mp.cpp now.
// SV_PacketDataIsUnknown provided by src/server_mp/sv_snapshot_profile_mp.cpp now.
// SV_ResetPacketData provided by src/server_mp/sv_snapshot_profile_mp.cpp now.
// svsHeader provided by src/server_mp/sv_snapshot_mp.cpp now.
// svsHeaderValid provided by src/server_mp/sv_snapshot_mp.cpp now.

// bullet + ui_gameinfo_mp cascade.
struct BulletFireParams;
struct BulletTraceResults;
struct AntilagClientStore;

// char BG_AdvanceTrace(BulletFireParams * /*p*/, BulletTraceResults * /*r*/, float /*dist*/) { return 0; }  // provided by bg_weapons.cpp now
// double BG_GetSurfacePenetrationDepth(const WeaponDef * /*w*/, unsigned int /*surfType*/) { return 0; }  // provided by bg_weapons.cpp now
// unsigned int BG_GetWeaponIndex(const WeaponDef * /*w*/) { return 0; }  // provided by bg_weapons.cpp now
// unsigned char DirToByte(const float * /*dir*/) { return 0; }  // provided by com_math.cpp now
// FS_GetFileList provided by src/universal/com_files.cpp now.
// void G_AntiLag_RestoreClientPos(AntilagClientStore * /*store*/) {}  // provided by game/ batch now
// void G_AntiLagRewindClientPos(int /*clientNum*/, AntilagClientStore * /*store*/) {}  // provided by game/ batch now
// G_CheckHitTriggerDamage provided by src/game_mp/g_trigger_mp.cpp now.
// G_Damage provided by src/game_mp/g_combat_mp.cpp now.
// G_LocationalTraceAllowChildren provided by src/game_mp/g_main_mp.cpp now.
// G_TempEntity provided by src/game_mp/g_utils_mp.cpp now.
// OnSameTeam provided by src/game_mp/g_main_mp.cpp now.

// bullet_penetrationEnabled provided by src/game_mp/g_main_mp.cpp now.
// const dvar_t *bullet_penetrationMinFxDist = nullptr;  // provided by bg_misc.cpp now
// g_debugLocDamage provided by src/game_mp/g_main_mp.cpp now.
// sv_clientSideBullets provided by src/server_mp/sv_main_mp.cpp now.
// sharedUiInfo_t sharedUiInfo{};  // provided by ui_main_mp.cpp now

// cg_event cascade.
struct FxEffectDef;
struct snd_alias_list_t;

// int  BG_WeaponIsClipOnly(unsigned int /*weaponIndex*/) { return 0; }  // provided by bg_weapons.cpp now
// void ByteToDir(unsigned int /*b*/, float *dir)  // provided by com_math.cpp now
// { if (dir) { dir[0] = 1; dir[1] = 0; dir[2] = 0; } }
// CG_BulletHitClientEvent provided by src/cgame/cg_weapons.cpp now.
// CG_BulletHitEvent provided by src/cgame/cg_weapons.cpp now.
// CG_CalcEntityLerpPositions provided by src/cgame_mp/cg_ents_mp.cpp now.
// CG_DrawScoreboard_GetTeamColorIndex provided by src/cgame_mp/cg_scoreboard_mp.cpp now.
// void CG_EjectWeaponBrass(int /*localClientNum*/, const entityState_s * /*es*/, int /*time*/) {}  // provided by cg_weapons.cpp / cg_ammocounter.cpp now
// CG_FireWeapon provided by src/cgame/cg_weapons.cpp now.
// CG_ImpactEffectForWeapon provided by src/cgame/cg_weapons.cpp now.
// void CG_MeleeBloodEvent(int /*localClientNum*/, const centity_s * /*cent*/) {}  // provided by cg_weapons.cpp / cg_ammocounter.cpp now
// void CG_OutOfAmmoChange(int /*localClientNum*/) {}  // provided by cg_weapons.cpp / cg_ammocounter.cpp now
// CG_PlayClientSoundAlias provided by src/cgame_mp/cg_main_mp.cpp now.
// CG_PlayEntitySoundAlias provided by src/cgame_mp/cg_main_mp.cpp now.
// CG_PlaySoundAlias provided by src/cgame_mp/cg_main_mp.cpp now.
// CG_PlaySoundAliasAsMasterByName provided by src/cgame_mp/cg_main_mp.cpp now.
// CG_PlaySoundAliasByName provided by src/cgame_mp/cg_main_mp.cpp now.
// CG_PrepOffHand provided by src/cgame/offhandweapons.cpp now.
// CG_PriorityCenterPrint provided by src/cgame_mp/cg_draw_mp.cpp now.
// void CG_SelectWeaponIndex(int /*localClientNum*/, unsigned int /*weaponIndex*/) {}  // provided by cg_weapons.cpp / cg_ammocounter.cpp now
// CG_StopSoundAlias provided by src/cgame_mp/cg_main_mp.cpp now.
// CG_StopSoundsOnEnt provided by src/cgame_mp/cg_main_mp.cpp now.
// CG_SwitchOffHandCmd / CG_UseOffHand provided by src/cgame/offhandweapons.cpp now.

// CL_DeathMessagePrint provided by src/client/cl_console.cpp now.
// CL_GetClientName provided by src/client_mp/cl_ui_mp.cpp now.

// void DynEntCl_ExplosionEvent(int /*localClientNum*/, bool /*ent*/, float * /*org*/, float /*r*/, float /*rs*/,
//                              float * /*norm*/, float /*duration*/, int /*type*/, int /*flags*/) {}
// void DynEntCl_JitterEvent(int /*localClientNum*/, float * /*pos*/, float /*radius*/, float /*amp*/, float /*duration*/, float /*frequency*/) {}
// void DynEntCl_MeleeEvent(int /*localClientNum*/, int /*entityNum*/) {}

void FX_PlayBoltedEffect(int /*localClientNum*/, const FxEffectDef * /*effect*/, int /*time*/, unsigned int /*entityNum*/, unsigned int /*boneIndex*/) {}
void FX_PlayOrientedEffect(int /*localClientNum*/, const FxEffectDef * /*effect*/, int /*time*/, const float * /*origin*/, const float (* /*axis*/)[3]) {}

// void Scr_SetString(unsigned short * /*ptr*/, unsigned int /*stringValue*/) {}  // provided by scr_variable/scr_stringlist now

// const dvar_t *bg_fallDamageMaxHeight = nullptr;  // provided by bg_misc.cpp now
// const dvar_t *bg_fallDamageMinHeight = nullptr;  // provided by bg_misc.cpp now
// cg_debugEvents provided by src/cgame_mp/cg_main_mp.cpp now.
// cg_footsteps provided by src/cgame_mp/cg_main_mp.cpp now.
// cgMedia + cgsArray: typed via cg_local_mp.h (already in include chain).
// cgMedia / cgsArray provided by src/cgame_mp/cg_main_mp.cpp now.

// Con_InitChannels now in client/con_channels.cpp.
// Con_IsChannelVisible now in client/con_channels.cpp.
// Con_WriteFilterConfigString now in client/con_channels.cpp.
// void Key_WriteBindings(int /*localClientNum*/, int /*f*/) {}  // provided by cl_keys.cpp now
// char *SEH_LocalizeTextMessage(const char *src, const char * /*context*/, msgLocErrType_t /*err*/) { return const_cast<char *>(src); }  // provided by stringed_hooks.cpp now
// void SEH_UpdateLanguageInfo() {}  // provided by stringed_hooks.cpp now
// const char *StringTable_GetColumnValueForRow(const StringTable * /*table*/, int /*row*/, int /*col*/) { return ""; }
// ProfLoad_Init now in qcommon/com_profilemapload.cpp.
// ProfLoad_IsActive now in qcommon/com_profilemapload.cpp.
// ProfLoad_Deactivate now in qcommon/com_profilemapload.cpp.

// =========================================================================
// Net + msg. Stubs.
// =========================================================================

void NET_Init() {}
// int  NET_GetClientPacket(netadr_t * /*from*/, msg_t * /*msg*/) { return 0; }  // provided by net_chan_mp.cpp now
// int  NET_GetLoopPacket(netsrc_t /*sock*/, netadr_t * /*from*/, msg_t * /*msg*/) { return 0; }  // provided by net_chan_mp.cpp now
// int  NET_GetServerPacket(netadr_t * /*from*/, msg_t * /*msg*/) { return 0; }  // provided by net_chan_mp.cpp now
void NET_RestartDebug() {}
void NET_ShutdownDebug() {}
void NET_Sleep(int /*msec*/) {}
// void Netchan_Init(short /*port*/) {}  // provided by net_chan_mp.cpp now
// MSG_Init now in qcommon/msg_mp.cpp.

// =========================================================================
// Hunk + PMem.
// =========================================================================

// void Hunk_Clear() {}
// void Hunk_ClearTempMemory() {}
// void Hunk_ClearTempMemoryHigh() {}
// void Hunk_InitDebugMemory() {}
// void Hunk_ResetDebugMem() {}
// void Hunk_ShutdownDebugMemory() {}
void PMem_Init() {}
void PMem_BeginAlloc(const char * /*name*/, unsigned int /*allocType*/) {}
void PMem_EndAlloc(const char * /*name*/, unsigned int /*allocType*/) {}

// =========================================================================
// Build number — upstream auto-generates this on Windows. We ship a static
// 0 build (BUILD_NUMBER macro is in src/buildnumber.h).
// =========================================================================

// getBuildNumber / getBuildNumberAsInt — provided by buildnumber.cpp now.

// =========================================================================
// Globals expected by other backbone files.
// =========================================================================

// bgs_t *bgs = nullptr;  // provided by bg_animation_mp.cpp now
// clientUIActives provided by src/client_mp/cl_main_mp.cpp now.
// int com_fileAccessed;  // provided by com_files.cpp now
// const dvar_s *fs_basepath = nullptr;  // provided by com_files.cpp now
// const dvar_s *fs_debug = nullptr;  // provided by com_files.cpp now
// int fs_fakeChkSum;  // provided by com_files.cpp now
// const dvar_s *fs_gameDirVar = nullptr;  // provided by com_files.cpp now
// int fs_numServerIwds;  // provided by com_files.cpp now
// searchpath_s *fs_searchpaths = nullptr;  // provided by com_files.cpp now
// const char *fs_serverIwdNames[1024]{};  // provided by com_files.cpp now
// int fs_serverIwds[1024]{};  // provided by com_files.cpp now
// const dvar_t *loc_language = nullptr;  // provided by stringed_hooks.cpp now
// sv provided by src/server_mp/sv_main_mp.cpp now.
// updateScreenCalled provided by src/client_mp/cl_scrn_mp.cpp now.

// fx_randomTable: 507-entry deterministic random table used by the
// EffectsCore particle system. Real upstream fills this once at startup
// from a fixed seed so spawn positions/velocities are reproducible
// across the network. Storage is const to match the upstream declaration
// `extern const float fx_randomTable[507]` in fx_system.h; the static
// initializer below mutates through a non-const alias.
// const float fx_randomTable[507] — provided by fx_random.cpp now.
int fx_serverVisClient = -1;

// =========================================================================
// Ragdoll / DynEntity / DObj / Phys / CG cascade — landed with ragdoll +
// DynEntity_coll. These will be replaced as the real subsystems come in.
// =========================================================================

// --- Math helpers — real impls -------------------------------------------
// AxisTranspose, MatrixMultiply43, MatrixTransposeTransformVector43,
// QuatToAxis, Q_acos, Vec3AddScalar provided by src/universal/com_math.cpp now.

// Vec3Rotate, Vec4Dot, Vec4LengthSq, Vec4Lerp provided by
// src/universal/com_math.cpp now.

// flrand provided by src/universal/com_math.cpp now.

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
// DObj_s *Com_GetClientDObj(unsigned int /*handle*/, int /*localClientNum*/) { return nullptr; }
// DObjDisplayAnim now provided by xanim/xanim.cpp.
// void DObjGetBasePoseMatrix(const DObj_s * /*obj*/, unsigned char /*boneIndex*/, DObjAnimMat * /*outMat*/) {}
// int  DObjGetBoneIndex(const DObj_s * /*obj*/, unsigned int /*name*/, unsigned char *index)
// {
//     if (index) *index = 255;
//     return 0;
// }
// DObjAnimMat *DObjGetRotTransArray(const DObj_s * /*obj*/) { return nullptr; }
// char DObjSetSkelRotTransIndex(DObj_s * /*obj*/, const int * /*partBits*/, int /*boneIndex*/) { return 0; }

// DynEnt
// DynEntityClient *DynEnt_GetClientEntity(unsigned short /*id*/, DynEntityDrawType /*draw*/) { return nullptr; }
// DynEntityColl *DynEnt_GetEntityColl(DynEntityCollType /*coll*/, unsigned short /*id*/) { return nullptr; }
// unsigned short DynEnt_GetEntityCount(DynEntityCollType /*coll*/) { return 0; }
// const DynEntityDef *DynEnt_GetEntityDef(unsigned short /*id*/, DynEntityDrawType /*draw*/) { return nullptr; }
// const DynEntityProps *DynEnt_GetEntityProps(DynEntityType /*t*/) { return nullptr; }
// unsigned short DynEnt_GetId(const DynEntityDef * /*def*/, DynEntityDrawType /*draw*/) { return 0; }

// XModelGetBounds now provided by xanim/xmodel.cpp.

// R
unsigned int R_GetLocalClientNum() { return 0; }

// SL (stringlist)
// unsigned int SL_FindString(const char * /*str*/) { return 0; }  // provided by scr_variable/scr_stringlist now

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

// unsigned int FindObject(unsigned int /*id*/) { return 0; }  // provided by scr_variable/scr_stringlist now
// unsigned int GetObject(unsigned int /*id*/) { return 0; }  // provided by scr_variable/scr_stringlist now
// unsigned int FindVariable(unsigned int /*parentId*/, unsigned int /*name*/) { return 0; }  // provided by scr_variable/scr_stringlist now
// unsigned int GetVariable(unsigned int /*parentId*/, unsigned int /*name*/) { return 0; }  // provided by scr_variable/scr_stringlist now
// unsigned int FindArrayVariable(unsigned int /*parentId*/, int /*intValue*/) { return 0; }  // provided by scr_variable/scr_stringlist now
// unsigned int GetArrayVariable(unsigned int /*parentId*/, unsigned int /*value*/) { return 0; }  // provided by scr_variable/scr_stringlist now
// unsigned int GetNewVariable(unsigned int /*parentId*/, unsigned int /*value*/) { return 0; }  // provided by scr_variable/scr_stringlist now
// unsigned int GetArray(unsigned int /*id*/) { return 0; }  // provided by scr_variable/scr_stringlist now
// unsigned int GetArraySize(unsigned int /*id*/) { return 0; }  // provided by scr_variable/scr_stringlist now
// unsigned int FindFirstSibling(unsigned int /*id*/) { return 0; }  // provided by scr_variable/scr_stringlist now
// unsigned int FindNextSibling(unsigned int /*id*/) { return 0; }  // provided by scr_variable/scr_stringlist now
// unsigned int GetVariableName(unsigned int /*id*/) { return 0; }  // provided by scr_variable/scr_stringlist now
// Vartype_t    GetValueType(unsigned int /*id*/) { return VAR_UNDEFINED; }  // provided by scr_variable/scr_stringlist now
// VariableValueInternal_u *GetVariableValueAddress(unsigned int /*id*/) { return nullptr; }  // provided by scr_variable/scr_stringlist now
// void         RemoveRefToObject(unsigned int /*id*/) {}  // provided by scr_variable/scr_stringlist now
// void         RemoveVariable(unsigned int /*parentId*/, unsigned int /*name*/) {}  // provided by scr_variable/scr_stringlist now
// void         ClearObject(unsigned int /*parentId*/) {}  // provided by scr_variable/scr_stringlist now
// void         SetVariableValue(unsigned int /*id*/, VariableValue * /*value*/) {}  // provided by scr_variable/scr_stringlist now

// --- Stringlist (scr_stringlist.cpp) --------------------------------------
// const char *SL_ConvertToString(unsigned int /*stringValue*/) { return ""; }  // provided by scr_variable/scr_stringlist now
// const char *SL_DebugConvertToString(unsigned int /*stringValue*/) { return ""; }  // provided by scr_variable/scr_stringlist now
// unsigned int SL_GetLowercaseString_(const char * /*str*/, unsigned int /*user*/, int /*type*/) { return 0; }  // provided by scr_variable/scr_stringlist now
// unsigned int SL_GetString_(const char * /*str*/, unsigned int /*user*/, int /*type*/) { return 0; }  // provided by scr_variable/scr_stringlist now
// void SL_RemoveRefToString(unsigned int /*stringValue*/) {}  // provided by scr_variable/scr_stringlist now
// void SL_ShutdownSystem(unsigned int /*user*/) {}  // provided by scr_variable/scr_stringlist now
// void SL_TransferRefToUser(unsigned int /*stringValue*/, unsigned int /*user*/) {}  // provided by scr_variable/scr_stringlist now

// --- Scr_* lifecycle + helpers --------------------------------------------
char *Scr_AddSourceBuffer(const char * /*filename*/, char * /*extFilename*/,
                          const char * /*codePos*/, bool /*archive*/) { return nullptr; }
// unsigned int Scr_AllocArray() { return 0; }  // provided by scr_variable/scr_stringlist now
void Scr_ClearErrorMessage() {}
// unsigned int Scr_CreateCanonicalFilename(const char * /*filename*/) { return 0; }  // provided by scr_variable/scr_stringlist now
void Scr_EndLoadEvaluate() {}
// VariableValue Scr_EvalVariable(unsigned int /*id*/) { VariableValue v{}; return v; }  // provided by scr_variable/scr_stringlist now
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
// char *TempMalloc(unsigned int len) { return static_cast<char *>(std::malloc(len)); }
// void TempMemoryReset(HunkUser * /*user*/) {}
// unsigned char *Hunk_AllocXAnimPrecache(unsigned int size)
// {
//     return static_cast<unsigned char *>(std::calloc(1, size));
// }
// void Hunk_CheckTempMemoryClear() {}
// void Hunk_CheckTempMemoryHighClear() {}
// HunkUser *Hunk_UserCreate(int /*maxSize*/, const char * /*name*/, bool /*fixed*/,
//                           bool /*tempMem*/, int /*type*/) { return nullptr; }
// void Hunk_UserDestroy(HunkUser * /*user*/) {}

// --- XAnim now provided by xanim/xanim.cpp -------------------------------
// (XAnimBlend, XAnimCreate, XAnimCreateAnims, XAnimPrecache,
//  XAnimSetupSyncNodes, XAnimInit, XAnimShutdown)

// --- Misc -----------------------------------------------------------------
// bool I_iscsym(int c) { return std::isalnum(c) || c == '_'; }  // provided by q_shared.cpp now
// ProfLoad_Begin now in qcommon/com_profilemapload.cpp.
// ProfLoad_End now in qcommon/com_profilemapload.cpp.

// --- Global storage for scr*Pub structures -------------------------------
// All these pub structs have their definitions reached via the script
// headers included at the top, so we can zero-construct them properly.
scrCompilePub_t  scrCompilePub{};
scrParserPub_t   scrParserPub{};
// scrVarPub_t      scrVarPub{};  // provided by scr_variable/scr_stringlist now
// scrVarDebugPub storage provided by scr_variable/scr_stringlist now.
scrVmPub_t       scrVmPub{};
bool g_loadedImpureScript = false;

// FxRandomTableInit / FX_RandomDir — provided by fx_random.cpp now.

// === CGAME draw/debug/reticles/offhandweapons satellites ===========================

// CG_ShouldDrawHud provided by src/cgame_mp/cg_newDraw_mp.cpp now.
void R_TrackStatistics(trStatistics_t *) {}
// void FX_DrawMarkProfile(int, void (*)(const char *, float *), float *) {}  // provided by fx_profile.cpp now
int  PMem_GetFreeAmount() { return 0; }
void Phys_DrawDebugText(const ScreenPlacement *) {}
int  Scr_GetStringUsage() { return 0; }
void Phys_GetPerformance(float *t, int *a, int *b) { if (t) *t = 0.f; if (a) *a = 0; if (b) *b = 0; }
int  SND_GetSoundOverlay(snd_overlay_type_t, snd_overlay_info_t *, int, int *) { return 0; }
// unsigned int Scr_GetNumScriptVars() { return 0u; }  // provided by scr_variable/scr_stringlist now
// BG_GetSpreadForWeapon provided by src/bgame/bg_weapons.cpp now.
snd_entchannel_info_t *SND_GetEntChannelName(int) { return nullptr; }
// void CG_UpdateViewModelPose(const DObj_s *, int) {}  // provided by cg_weapons.cpp / cg_ammocounter.cpp now
// bool UI_ShouldDrawCrosshair() { return false; }  // provided by ui_main_mp.cpp now
unsigned int Scr_GetNumScriptThreads() { return 0u; }
// int  CG_PlayerTurretWeaponIdx(int) { return 0; }  // provided by cg_weapons.cpp / cg_ammocounter.cpp now
void Phys_PerformanceEndFrame() {}
// void AimAssist_DrawDebugOverlay(unsigned int) {}  // provided by aim_assist.cpp now
// int  BG_GetFirstEquippedOffhand(const playerState_s *, int) { return 0; }  // provided by bg_weapons.cpp now
// int  BG_GetFirstAvailableOffhand(const playerState_s *, int) { return 0; }  // provided by bg_weapons.cpp now
// CG_Flashbanged provided by src/cgame/cg_shellshock.cpp now.
// void FX_DrawProfile(int, void (*)(char *), float *) {}  // provided by fx_profile.cpp now
int  R_PickMaterial(int, const float *, const float *, char *, char *, char *, unsigned int) { return 0; }
// uint32_t BG_GetNumWeapons() { return 0u; }  // provided by bg_weapons.cpp now
// int32_t  BG_ClipForWeapon(uint32_t) { return 0; }  // provided by bg_weapons.cpp now
// void     FX_Beam_Add(FxBeam *) {}  // provided by fx_beam.cpp now
// void     FX_PostLight_Add(FxPostLight *) {}  // provided by fx_postlight.cpp now
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

// MatrixMultiply, AxisCopy provided by src/universal/com_math.cpp now.
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
// CL_GetLocalClientActiveCount provided by src/client_mp/cl_main_mp.cpp now.
// CG_GetLocalClientViewParams provided by src/cgame_mp/cg_view_mp.cpp now.

// === cg_effects_load_obj satellites ==============================================

const FxEffectDef *FX_Register(const char *) { return nullptr; }
// compare_impact_files provided by src/ui/ui_expressions_logicfunctions.cpp now.
// Com_SurfaceTypeFromName provided by src/universal/surfaceflags.cpp now.
// unsigned int *Hunk_AllocateTempMemory(int size, const char * /*name*/)
// {
//     return static_cast<unsigned int *>(std::calloc((size + sizeof(unsigned int) - 1) / sizeof(unsigned int), sizeof(unsigned int)));
// }
// unsigned int Hunk_AllocateTempMemoryHigh(int /*size*/, const char * /*name*/) { return 0u; }

// === cg_draw_indicators satellites ===============================================

// UI_FillRectPhysical provided by src/ui/ui_atoms.cpp now.

// const dvar_t *bg_maxGrenadeIndicatorSpeed   = nullptr;  // provided by bg_misc.cpp now
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

// void  FX_SpriteAdd(FxSprite *) {}  // provided by fx_sprite.cpp now
// Vec2Distance provided by src/universal/com_math.cpp now.
// int   SEH_PrintStrlen(const char *s) { return s ? static_cast<int>(std::strlen(s)) : 0; }  // provided by stringed_hooks.cpp now
// void  BG_LerpHudColors(const hudelem_s *, int, hudelem_color_t *) {}  // provided by bg_misc.cpp now
// compare_hudelems provided by src/ui/ui_expressions_logicfunctions.cpp now.
// bool  UI_AnyMenuVisible(int) { return false; }  // provided by ui_main_mp.cpp now
// CG_ServerMaterialName provided by src/cgame_mp/cg_newDraw_mp.cpp now.
// double R_NormalizedTextScale(Font_s *, float scale) { return scale; }
// void  CL_PlayTextFXPulseSounds(uint32_t, int, int, int, int, int, int *) {}  // provided by cl_console.cpp now
// CG_GetViewAxisProjections provided by src/cgame_mp/cg_draw_mp.cpp now.
// CL_DrawTextPhysicalWithEffects provided by src/client_mp/cl_main_mp.cpp now.
// UI_GetKeyBindingLocalizedString provided by src/ui/ui_shared.cpp now.

// === cg_localents satellites =====================================================

// void CG_DrawTracer(const float *, const float *, const refdef_s *) {}  // provided by cg_weapons.cpp / cg_ammocounter.cpp now
// cg_tracerLength provided by src/cgame_mp/cg_main_mp.cpp now.
// === cg_world satellites =========================================================

// void   DObjLock(DObj_s *) {}
// void   DObjUnlock(DObj_s *) {}
// double DObjGetRadius(const DObj_s *) { return 0.0; }
// int    DObjGetContents(const DObj_s *) { return 0; }
// int    DObjHasContents(DObj_s *, int) { return 0; }
// void   DObjGeomTraceline(DObj_s *, float *, float *const, int, DObjTrace_s *) {}
// void   DObjGeomTracelinePartBits(DObj_s *, int, int *) {}
DObjAnimMat *CG_DObjCalcPose(const cpose_t *, const DObj_s *, int32_t *) { return nullptr; }
// void   DynEntCl_ClipMoveTrace(const moveclip_t *, trace_t *) {}
// void   CM_PointTraceStaticModels(trace_t *, const float *, const float *, int) {}

// === cg_predict_mp satellites ====================================================

// CL_SendCmd provided by src/client_mp/cl_input.cpp now.
// bool BG_CanItemBeGrabbed(const entityState_s *, const playerState_s *, int) { return false; }  // provided by bg_misc.cpp now
// bool BG_PlayerTouchesItem(const playerState_s *, const entityState_s *, int) { return false; }  // provided by bg_misc.cpp now
// bool BG_PlayerHasRoomForEntAllAmmoTypes(const entityState_s *, const playerState_s *) { return false; }  // provided by bg_misc.cpp now
// void BG_PlayerStateToEntityState(playerState_s *, entityState_s *, int, uint8_t) {}  // provided by bg_misc.cpp now
// void PM_UpdateViewAngles(playerState_s *, float, usercmd_s *, uint8_t) {}  // provided by bg_pmove.cpp now
// void Pmove(pmove_t *) {}  // provided by bg_pmove.cpp now
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
// void   AimAssist_Setup(int) {}  // provided by aim_assist.cpp now
void   R_InitSceneData(int) {}
// XModel *R_RegisterModel(const char *) { return nullptr; }
void   SND_SetListener(int, int, const float *, const float (*)[3]) {}
void   SND_FadeAllSounds(float, int) {}
// CG_UpdatePlayerDObj provided by src/cgame_mp/cg_players_mp.cpp now.
// CG_UpdateViewOffset provided by src/cgame_mp/cg_view_mp.cpp now.
// void   FX_MarkEntDetachAll(int, int) {}
// CG_ResetPlayerEntity provided by src/cgame_mp/cg_players_mp.cpp now.
void   FX_ThroughWithEffect(int, FxEffect *) {}
// CG_mg42_PreControllers provided by src/cgame_mp/cg_ents_mp.cpp now.
// void   CG_UpdateHandViewmodels(int, XModel *) {}  // provided by cg_weapons.cpp / cg_ammocounter.cpp now
// CG_Player_PreControllers provided by src/cgame_mp/cg_ents_mp.cpp now.
// CG_SetFrameInterpolation provided by src/cgame_mp/cg_ents_mp.cpp now.
// void   CG_UpdateWeaponViewmodels(int) {}  // provided by cg_weapons.cpp / cg_ammocounter.cpp now
// CG_UpdateBModelWorldBounds provided by src/cgame_mp/cg_ents_mp.cpp now.
// CG_ExecuteNewServerCommands provided by src/cgame_mp/cg_servercmds_mp.cpp now.
// CG_CheckOpenWaitingScriptMenu provided by src/cgame_mp/cg_servercmds_mp.cpp now.
// void   AimAssist_ClearEntityReference(int, int) {}  // provided by aim_assist.cpp now

// cg_entityOriginArray provided by src/cgame_mp/cg_main_mp.cpp now.
// cg_fs_debug provided by src/cgame_mp/cg_main_mp.cpp now.
// === cg_view_mp satellites =======================================================

// float BG_GetSpeed(const playerState_s *, int) { return 0.f; }  // provided by bg_pmove.cpp now
void  FX_RewindTo(int, int) {}
void  R_ClearScene(unsigned int) {}
// CG_DrawActive provided by src/cgame_mp/cg_draw_mp.cpp now.
// float BG_GetBobCycle(const playerState_s *) { return 0.f; }  // provided by bg_weapons.cpp now
void  FX_BeginUpdate(int) {}
// void  Key_AddCatcher(int, int) {}  // provided by cl_keys.cpp now
void  R_SetLodOrigin(const refdef_s *) {}
void  CG_VehGunnerPOV(int, float *o, float *a)
{
    if (o) { o[0] = o[1] = o[2] = 0; }
    if (a) { a[0] = a[1] = a[2] = 0; }
}
// void  CG_AddViewWeapon(int) {}  // provided by cg_weapons.cpp / cg_ammocounter.cpp now
// CG_ProcessEntity provided by src/cgame_mp/cg_ents_mp.cpp now.
void  FX_FillUpdateCmd(int, FxCmd *) {}
// void  AddLeanToPosition(float *, float, float, float, float) {}  // provided by q_shared.cpp now
// CG_DObjUpdateInfo provided by src/cgame_mp/cg_ents_mp.cpp now.
// void  Key_RemoveCatcher(int, int) {}  // provided by cl_keys.cpp now
double R_GetFarPlaneDist() { return 0.0; }
double R_GetBaseLodDist(const float * /*origin*/) { return 0.0; }
enum XModelLodRampType : int;
double R_GetAdjustedLodDist(float dist, XModelLodRampType /*lodRampType*/) { return dist; }

// FX_ family — stubs until fx_system / fx_marks / fx_update land.
struct FxSystem;
struct FxSystemBuffers;
struct FxEffectDef;
struct FxMarksSystem;
struct FxMark;
struct MemoryFile;
FxSystem *FX_GetSystem(int /*localClientNum*/) { return nullptr; }
FxSystemBuffers *FX_GetSystemBuffers(int /*localClientNum*/) { return nullptr; }
void FX_LinkSystemBuffers(FxSystem * /*system*/, FxSystemBuffers * /*buffers*/) {}
void FX_RelocateSystem(FxSystem * /*system*/, int /*delta*/) {}
void FX_RunGarbageCollection(FxSystem * /*system*/) {}
void FX_BeginIteratingOverEffects_Cooperative(FxSystem * /*system*/) {}
void FX_ForEachEffectDef(void (* /*cb*/)(const FxEffectDef *, void *), void * /*data*/) {}
// unsigned short FX_MarkToHandle(FxMarksSystem * /*sys*/, FxMark * /*mark*/) { return 0; }
// FxMark *FX_MarkFromHandle(FxMarksSystem * /*sys*/, unsigned short /*handle*/) { return nullptr; }
#include <gfx_d3d/fxprimitives.h>
FxMarksSystem fx_marksSystemPool[1] = {};

dxBody *Phys_ObjLoad(PhysWorld /*w*/, MemoryFile * /*memFile*/) { return nullptr; }
void Phys_ObjSave(dxBody * /*body*/, MemoryFile * /*memFile*/) {}

// void DynEnt_LoadEntities() {}
void R_LinkDynEnt(unsigned int /*idx*/, DynEntityDrawType /*type*/, float * /*mins*/, float * /*maxs*/) {}
void R_UnlinkDynEnt(unsigned int /*idx*/, DynEntityDrawType /*type*/) {}
void Phys_AddJitterRegion(PhysWorld /*w*/, const float * /*pos*/, float /*r*/, float /*j1*/, float /*j2*/, float /*j3*/) {}
struct XModelPieces;
XModelPieces *XModelPiecesPrecache(const char * /*name*/, void *(* /*alloc*/)(int)) { return nullptr; }
struct GfxScaledPlacement;
void R_FilterXModelIntoScene(const XModel * /*model*/, const GfxScaledPlacement * /*placement*/, unsigned short /*flags*/, unsigned short * /*outIds*/) {}
void Phys_ObjSetAngularVelocity(dxBody * /*b*/, float * /*omega*/) {}

// r_dvars satellite storage (declared extern in r_dvars.h, owned by the
// Win32 build's r_init.cpp; we own them here until r_init.cpp lands).
const dvar_t *r_fullscreen;
// const dvar_t *r_warningRepeatDelay;  // provided by r_warn.cpp now
const dvar_t *vid_xpos;
const dvar_t *vid_ypos;

// r_dvars satellite hooks (r_init.cpp's registration routines).
// void R_RegisterSunDvars() {}  // provided by r_sky.cpp now
void Material_PreventOverrideTechniqueGeneration() {}

// r_dobj_skin satellite hooks.
enum WorkerCmdType : int;
// void Z_VirtualCommit(void * /*addr*/, int /*size*/) {}
void R_AddWorkerCmd(WorkerCmdType /*type*/, unsigned char * /*data*/) {}
struct GfxSceneEntity;
// R_UpdateSceneEntBounds — provided by r_model_pose.cpp now.
// void R_XModelDebug(const DObj_s * /*obj*/, int * /*partBits*/) {}

// r_dpvs satellite stubs.
struct DpvsPlane;
float R_DpvsPlaneMaxSignedDistToBox(const DpvsPlane * /*plane*/, const float * /*minmax*/) { return 0.0f; }
void R_CopyClipPlane(const DpvsPlane * /*in*/, DpvsPlane * /*out*/) {}
thread_local unsigned char *g_smodelVisData;
thread_local unsigned char *g_surfaceVisData;

// r_model satellite stubs.
struct IDirect3DVertexBuffer9;
void *R_LockVertexBuffer(IDirect3DVertexBuffer9 * /*vb*/, int /*offset*/, int /*size*/, int /*flags*/) { return nullptr; }
// unsigned char *Hunk_AllocXModelPrecache(unsigned int /*size*/) { return nullptr; }
// unsigned char *Hunk_AllocXModelPrecacheColl(unsigned int /*size*/) { return nullptr; }
void PMem_DumpMemStats() {}
void Sys_OutOfMemErrorInternal(const char * /*file*/, int /*line*/) {}
struct FileDataHashEntry;
FileDataHashEntry *com_fileDataHashTable[8192]{};
#include <gfx_d3d/r_init.h>
GfxConfiguration gfxCfg{};
GfxMetrics gfxMetrics{};
#include <gfx_d3d/r_sky.h>
SunFlareDynamic sunFlareArray[4]{};

void R_AddSpotShadowsForLight(GfxViewInfo * /*viewInfo*/, GfxLight * /*light*/, unsigned int /*idx*/, float /*scale*/) {}

// r_outdoor satellite stubs.
struct GfxImage;
GfxImage *Image_Register(const char * /*name*/, unsigned char /*flags*/, int /*format*/) { return nullptr; }
void Image_Generate2D(GfxImage * /*img*/, unsigned char * /*data*/, int /*w*/, int /*h*/, int /*format*/) {}
GfxImage *Image_Alloc(char * /*name*/, unsigned char /*flags*/, unsigned char /*a*/, unsigned char /*b*/) { return nullptr; }
void Image_GenerateCube(GfxImage * /*img*/, const unsigned char *(* /*faces*/)[15], int /*w*/, int /*h*/, unsigned int /*flags*/) {}
struct GfxImageFileHeader;
char Image_ValidateHeader(GfxImageFileHeader * /*hdr*/, const char * /*name*/) { return 0; }
struct WaveletDecode;
// void Wavelet_DecompressLevel(unsigned char * /*dst*/, unsigned char * /*src*/, WaveletDecode * /*ctx*/) {}  // provided by r_image_wavelet.cpp now
unsigned char *Material_Alloc(unsigned int /*size*/) { return nullptr; }
void Image_BuildWaterMap(GfxImage * /*img*/) {}
void Image_UploadData(const GfxImage * /*img*/, int /*format*/, int /*face*/, unsigned int /*mip*/, unsigned char * /*data*/) {}
unsigned int Image_CubemapFace(unsigned int /*faceIndex*/) { return 0; }
void Image_SetupFromFile(GfxImage * /*img*/, const GfxImageFileHeader * /*hdr*/, int /*flags*/) {}
void Image_FreeTempMemory(unsigned char * /*p*/, int /*size*/) {}
unsigned char *Image_AllocTempMemory(int /*size*/) { return nullptr; }
unsigned int Image_CountMipmapsForFile(const GfxImageFileHeader * /*hdr*/) { return 0; }

// r_meshdata satellite stubs.
struct GfxMeshData;
void *R_GetMeshVerts(GfxMeshData * /*mesh*/, unsigned short /*baseVertex*/) { return nullptr; }
void R_BeginMeshVerts(GfxMeshData * /*mesh*/) {}
char R_ReserveMeshVerts(GfxMeshData * /*mesh*/, int /*count*/, unsigned short * /*out*/) { return 0; }
char R_ReserveMeshIndices(GfxMeshData * /*mesh*/, int /*count*/, r_double_index_t ** /*out*/) { return 0; }

// r_light satellite stubs.
struct GfxSurface;
void R_AddShadowSurfaceToPrimaryLight(GfxWorld * /*world*/, unsigned int /*surfIndex*/, unsigned int /*lightIndex*/) {}
void R_ForEachPrimaryLightAffectingSurface(GfxWorld * /*world*/, const GfxSurface * /*surf*/, unsigned int /*surfIndex*/, void (*)(GfxWorld *, unsigned int, unsigned int)) {}

// r_sunshadow satellite stubs (live in r_dpvs / r_scene / r_view).
void R_SetVisData(unsigned int /*cellIndex*/) {}
void R_SetDpvsPlaneSides(DpvsPlane * /*plane*/) {}
struct GfxViewParms;
void R_SetupShadowSurfacesDpvs(const GfxViewParms * /*viewParms*/, const float (* /*planes*/)[4], unsigned int /*planeCount*/, int /*flags*/) {}
void R_AddWorldSurfacesFrustumOnly() {}
float R_DpvsPlaneMinSignedDistToBox(const DpvsPlane * /*plane*/, const float * /*minmax*/) { return 0.0f; }
void R_SetupViewProjectionMatrices(GfxViewParms * /*viewParms*/) {}

// fx_marks satellite stubs (live in fx_update / fx_system).
struct FxElemVisStateSample;
enum FxRandKey : int32_t;
struct FxElemDef;
struct FxEffect;
struct FxElemPreVisualState;
struct FxElemVisualState;
double FX_InterpolateSize(const FxElemVisStateSample * /*samples*/, int /*count*/, FxRandKey /*key*/, float /*t*/, float /*scale*/, int /*flags*/) { return 0.0; }
void FX_SetupVisualState(const FxElemDef * /*def*/, const FxEffect * /*effect*/, int /*frame*/, float /*t*/, FxElemPreVisualState * /*state*/) {}
void FX_EvaluateVisualState(FxElemPreVisualState * /*pre*/, float /*t*/, FxElemVisualState * /*state*/) {}

// fx_convert satellite stubs.
struct MaterialInfo;
void Material_GetInfo(Material * /*m*/, MaterialInfo * /*out*/) {}
struct PhysPreset;
PhysPreset *FX_RegisterPhysPreset(const char * /*name*/) { return nullptr; }

struct DiskGfxReflectionProbe;
void R_CreateReflectionRawDataFromCubemapShot(DiskGfxReflectionProbe * /*probe*/, int /*downRes*/) {}

// r_add_staticmodel satellite stubs.
union GfxDrawSurf;
enum MaterialTechniqueType : int;
struct XModelLodInfo;
struct GfxStaticModelDrawInst;
// void R_SortDrawSurfs(GfxDrawSurf * /*surfs*/, int /*count*/) {}
struct MaterialTechnique;
const MaterialTechnique *Material_GetTechnique(const Material * /*m*/, MaterialTechniqueType /*t*/) { return nullptr; }
void *R_GetCachedSModelSurf(unsigned int /*idx*/) { return nullptr; }
void R_AddXModelDebugString(const float * /*origin*/, char * /*text*/) {}
void R_CacheStaticModelSurface(unsigned int /*idx*/, unsigned int /*surfIdx*/, const XModelLodInfo * /*lod*/) {}
void *R_AllocStaticModelLighting(GfxStaticModelDrawInst * /*inst*/, unsigned int /*size*/) { return nullptr; }

// Code-mesh stubs (provided once r_drawsurf.cpp lands).
struct Material;
struct r_double_index_t;
struct GfxPackedVertex;
// char R_ReserveCodeMeshIndices(int /*indexCount*/, r_double_index_t ** /*out*/) { return 0; }
// char R_ReserveCodeMeshVerts(int /*vertCount*/, unsigned short * /*out*/) { return 0; }
// char R_ReserveCodeMeshArgs(int /*argCount*/, unsigned int * /*out*/) { return 0; }
// void R_AddCodeMeshDrawSurf(Material * /*m*/, r_double_index_t * /*idx*/, unsigned int /*iCount*/,
//                             unsigned int /*argOffset*/, unsigned int /*argCount*/, const char * /*fxName*/) {}
// float (*R_GetCodeMeshArgs(unsigned int /*argOffset*/))[4] { return nullptr; }
// GfxPackedVertex *R_GetCodeMeshVerts(unsigned short /*baseVertex*/) { return nullptr; }
// CG_AddPacketEntity provided by src/cgame_mp/cg_ents_mp.cpp now.
// bool  Key_IsCatcherActive(int, int) { return false; }  // provided by cl_keys.cpp now
// CG_AddPacketEntities provided by src/cgame_mp/cg_ents_mp.cpp now.
// CL_GetMenuBlurRadius provided by src/client_mp/cl_scrn_mp.cpp now.
void  FX_SetNextUpdateTime(int, int) {}
// CL_ResetSkeletonCache provided by src/client_mp/cl_main_mp.cpp now.
// void  BG_CalculateViewAngles(viewState_t *, float *) {}  // provided by bg_weapons.cpp now
void  FX_SetNextUpdateCamera(int, const refdef_s *, float) {}
// float BG_GetVerticalBobFactor(const playerState_s *, float, float, float) { return 0.f; }  // provided by bg_weapons.cpp now
// int32_t BG_IsAimDownSightWeapon(uint32_t) { return 0; }  // provided by bg_weapons.cpp now
// void  CG_UpdateViewWeaponAnim(int) {}  // provided by cg_weapons.cpp / cg_ammocounter.cpp now
void  CG_VehSphereCoordsToPos(float, float, float, float *out) { if (out) { out[0] = out[1] = out[2] = 0; } }
// G_ExitAfterConnectPaths provided by src/game_mp/g_main_mp.cpp now.
void  R_AddCmdProjectionSet2D() {}
void  R_UpdateSpotLightEffect(FxCmd *) {}
// CG_DObjGetWorldTagMatrix provided by src/cgame_mp/cg_ents_mp.cpp now.
bool  CG_VehLocalClientDriving(int) { return false; }
void  R_UpdateRemainingEffects(FxCmd *) {}
// float BG_GetHorizontalBobFactor(const playerState_s *, float, float, float) { return 0.f; }  // provided by bg_weapons.cpp now
// CG_ProcessClientNoteTracks provided by src/cgame_mp/cg_ents_mp.cpp now.
void  R_UpdateNonDependentEffects(FxCmd *) {}
int32_t CG_VehLocalClientVehicleSlot(int) { return -1; }
// void  AimAssist_UpdateScreenTargets(int, const float *, const float *, float, float) {}  // provided by aim_assist.cpp now
bool  CG_VehLocalClientUsingVehicle(int) { return false; }
// int32_t AimAssist_GetScreenTargetCount(int) { return 0; }  // provided by aim_assist.cpp now
void  CG_VehSeatOriginForLocalClient(int, float *out) { if (out) { out[0] = out[1] = out[2] = 0; } }
// int32_t AimAssist_GetScreenTargetEntity(int, uint32_t) { return -1; }  // provided by aim_assist.cpp now
// CL_LocalActiveIndexFromClientNum provided by src/client_mp/cl_main_mp.cpp now.
// CL_Input provided by src/client_mp/cl_input.cpp now.
// CG_Draw2D provided by src/cgame_mp/cg_draw_mp.cpp now.
void  R_SyncGpu(int (*)(unsigned long long)) {}

// const dvar_t *bg_bobMax           = nullptr;  // provided by bg_misc.cpp now
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
// const dvar_t *vehDriverViewHeightMax  = nullptr;  // provided by ui_main_mp.cpp now

// === cg_players_mp satellites ====================================================

// float RotationToYaw(const float *) { return 0.f; }  // provided by com_math.cpp now
// float vectosignedyaw(const float *) { return 0.f; }  // provided by com_math.cpp now
// YawToAxis provided by src/universal/com_math.cpp now.
void  R_AddDObjToScene(const DObj_s *, const cpose_t *, unsigned int, unsigned int, float *, float) {}
// void  BG_PlayerAnimation(int, const entityState_s *, clientInfo_t *) {}  // provided by bg_animation_mp.cpp now
// void  CG_AddPlayerWeapon(int, const GfxScaledPlacement *, const playerState_s *, centity_s *, int) {}  // provided by cg_weapons.cpp / cg_ammocounter.cpp now
// bool  BG_IsKnifeMeleeAnim(const clientInfo_t *, int) { return false; }  // provided by bg_animation_mp.cpp now
// void  BG_UpdatePlayerDObj(int, DObj_s *, entityState_s *, clientInfo_t *, int) {}  // provided by bg_animation_mp.cpp now
// void  FX_MarkEntUpdateBegin(FxMarkDObjUpdateContext *, DObj_s *, bool, uint16_t) {}
// void  FX_MarkEntUpdateEnd(FxMarkDObjUpdateContext *, int, int, DObj_s *, bool, uint16_t) {}
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

// void R_SwitchFog(unsigned int, int, int) {}  // provided by gfx_d3d batch now
void FX_InitSystem(int) {}
void Phys_Shutdown() {}
void SND_StopMusic(int) {}
// CG_StartAmbient provided by src/cgame_mp/cg_main_mp.cpp now.
// int  Load_ScriptMenu(int, const char *, int) { return 0; }  // provided by ui_main_mp.cpp now
// void CG_RegisterItems(int) {}  // provided by cg_weapons.cpp / cg_ammocounter.cpp now
// void Menus_ShowByName(const UiContext *, const char *) {}  // provided by ui_shared.cpp now
// void CG_SetupWeaponDef(int) {}  // provided by cg_weapons.cpp / cg_ammocounter.cpp now
// CL_ParseMapCenter provided by src/client_mp/cl_parse_mp.cpp now.
// void DynEntCl_Shutdown(int) {}
void FX_KillAllEffects(int) {}
void FX_ShutdownSystem(int) {}
// CG_BoldGameMessage provided by src/cgame_mp/cg_main_mp.cpp now.
// void R_SetFogFromServer(float, unsigned char, unsigned char, unsigned char, float) {}  // provided by gfx_d3d batch now
void SND_PlayMusicAlias(int, const snd_alias_t *, bool, snd_alias_system_t) {}
// void UI_CloseInGameMenu(int) {}  // provided by ui_main_mp.cpp now
// int  UI_PopupScriptMenu(int, const char *, bool) { return 0; }  // provided by ui_main_mp.cpp now
// CG_ClearCenterPrint provided by src/cgame_mp/cg_draw_mp.cpp now.
void LiveStorage_SetStat(int, int, unsigned int) {}
void R_InitPrimaryLights(GfxLight *) {}
// CL_ResetPlayerMuting provided by src/client_mp/cl_main_pc_mp.cpp now.
// void DynEntCl_DestroyEvent(int, uint16_t, DynEntityCollType, const float *, const float *) {}
// void DynEntCl_InitEntities(int) {}
// void UI_ClosePopupScriptMenu(int, bool) {}  // provided by ui_main_mp.cpp now
// CG_PlayClientSoundAliasByName provided by src/cgame_mp/cg_main_mp.cpp now.
// CG_StopClientSoundAliasByName provided by src/cgame_mp/cg_main_mp.cpp now.
// CG_ShouldPlaySoundOnLocalClient provided by src/cgame_mp/cg_main_mp.cpp now.
// void R_ClearShadowedPrimaryLightHistory(int) {}  // provided by r_primarylights.cpp now
// char *UI_GetMapDisplayNameFromPartialLoadNameMatch(const char *, int *) { return nullptr; }  // provided by ui_main_mp.cpp now
void Phys_Init() {}

// cg_chatHeight provided by src/cgame_mp/cg_main_mp.cpp now.
// cg_chatTime provided by src/cgame_mp/cg_main_mp.cpp now.
// cg_teamChatsOnly provided by src/cgame_mp/cg_main_mp.cpp now.
// === cg_draw_mp satellites =======================================================

// void Con_DrawSay(int, int, int) {}  // provided by cl_console.cpp now
// void Menu_PaintAll(UiContext *) {}  // provided by ui_shared.cpp now
// void Con_DrawErrors(int, int, int, float) {}  // provided by cl_console.cpp now
// void Menus_HideByName(const UiContext *, const char *) {}  // provided by ui_shared.cpp now
// int32_t PM_GetSprintLeft(const playerState_s *, int32_t) { return 0; }  // provided by bg_pmove.cpp now
// void Menus_CloseByName(UiContext *, const char *) {}  // provided by ui_shared.cpp now
// int32_t BG_GetMaxSprintTime(const playerState_s *) { return 0; }  // provided by bg_misc.cpp now
// CG_CalcPlayerHealth provided by src/cgame_mp/cg_newDraw_mp.cpp now.
// CL_DrawTextPhysical provided by src/client_mp/cl_main_mp.cpp now.
// void Con_DrawMiniConsole(int, int, int, float) {}  // provided by cl_console.cpp now
// const char *UI_GetTopActiveMenuName(int) { return ""; }  // provided by ui_main_mp.cpp now
// CG_CheckPlayerForLowAmmo / CG_CheckPlayerForLowClip provided by src/cgame_mp/cg_newDraw_mp.cpp now.
// void BG_AssertOffhandIndexOrNone(uint32_t) {}  // provided by bg_weapons.cpp now
// Vec4Mul provided by src/universal/com_math.cpp now.

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
// const dvar_t *ui_showEndOfGame            = nullptr;  // provided by ui_main_mp.cpp now

// === cg_ents_mp satellites =======================================================

// const char *DObjGetName(const DObj_s *) { return ""; }
// void  Vec3ScaleMad(float, const float *, float, const float *, float *out) { if (out) { out[0] = out[1] = out[2] = 0; } }  // provided by com_math.cpp now
GfxBrushModel *R_GetBrushModel(unsigned int) { return nullptr; }
void  CG_DoControllers(const cpose_t *, const DObj_s *, int *) {}
void  R_LinkDObjEntity(unsigned int, unsigned int, float *, float) {}
// void  UnitQuatToAngles(const float *, float *out) { if (out) { out[0] = out[1] = out[2] = 0; } }  // provided by com_math.cpp now
// PhysPreset *DObjGetPhysPreset(const DObj_s *) { return nullptr; }
void  FX_RetriggerEffect(int, FxEffect *, int) {}
void  R_LinkBModelEntity(unsigned int, unsigned int, GfxBrushModel *) {}
void  CG_VehProcessEntity(int, centity_s *) {}
// void  DObjSetHidePartBits(DObj_s *, const unsigned int *) {}
// DObj_s *Com_ClientDObjCreate(DObjModel_s *, unsigned short, XAnimTree_s *, unsigned int, int) { return nullptr; }
// void  DObjGetHierarchyBits(const DObj_s *, int, int *) {}
void  Phys_ObjBulletImpact(PhysWorld, dxBody *, const float *, const float *, float, float) {}
// CG_IsRagdollTrajectory provided by src/cgame_mp/cg_main_mp.cpp now.
void  R_SkinGfxEntityDelayed(GfxSceneEntity *) {}
int32_t CG_VehPlayerVehicleSlot(int, uint32_t) { return -1; }
bool  CG_VehEntityUsingVehicle(int, uint32_t) { return false; }
void  FX_AssertAllocatedEffect(int, FxEffect *) {}
// bool  CG_PlayerUsingScopedTurret(int) { return false; }  // provided by cg_weapons.cpp / cg_ammocounter.cpp now
void  R_UpdateXModelBoundsDelayed(GfxSceneEntity *) {}
// void  BG_Player_DoControllersSetup(const entityState_s *, clientInfo_t *, int) {}  // provided by bg_animation_mp.cpp now
void  CG_VehSeatTransformForPlayer(int, uint32_t, float *o, float *a)
{
    if (o) { o[0] = o[1] = o[2] = 0; }
    if (a) { a[0] = a[1] = a[2] = 0; }
}
// void  FX_MarkEntUpdateHidePartBits(const uint32_t *, const uint32_t *, int, int) {}
void  R_AddBrushModelToSceneFromAngles(const GfxBrushModel *, const float *, const float *, uint16_t) {}
// void  DObjPhysicsSetCollisionFromXModel(const DObj_s *, PhysWorld, dxBody *) {}
// Vec3Avg provided by src/universal/com_math.cpp now.

// controller_names provided by src/game_mp/g_active_mp.cpp now.
// Hunk_AllocXAnimClient provided by src/cgame_mp/cg_main_mp.cpp now.

// === cg_newDraw_mp satellites ====================================================

// uint32_t GetWeaponIndex(const cg_s *) { return 0u; }  // provided by cg_weapons.cpp / cg_ammocounter.cpp now
// bool     PM_IsSprinting(const playerState_s *) { return false; }  // provided by bg_pmove.cpp now
// int32_t  BG_AmmoForWeapon(uint32_t) { return 0; }  // provided by bg_weapons.cpp now
// bool     Key_IsCommandBound(int, const char *) { return false; }  // provided by cl_keys.cpp now
// int32_t  BG_GetAmmoPlayerMax(const playerState_s *, uint32_t, uint32_t) { return 0; }  // provided by bg_weapons.cpp now
// CL_ShouldDisplayHud provided by src/client_mp/cl_main_mp.cpp now.
// bool     BG_WeaponBlocksProne(uint32_t) { return false; }  // provided by bg_weapons.cpp now
// int      UI_GetTalkerClientNum(int, int) { return -1; }  // provided by ui_main_mp.cpp now
// int32_t  BG_GetTotalAmmoReserve(const playerState_s *, uint32_t) { return 0; }  // provided by bg_weapons.cpp now
// void     CG_DrawPlayerActionSlot(int, const rectDef_s *, uint32_t, float *, Font_s *, float, int) {}  // provided by cg_weapons.cpp / cg_ammocounter.cpp now
// void     CG_DrawPlayerWeaponIcon(int, const rectDef_s *, const float *) {}  // provided by cg_weapons.cpp / cg_ammocounter.cpp now
// int32_t  PM_GetSprintLeftLastTime(const playerState_s *) { return 0; }  // provided by bg_pmove.cpp now
// CG_GetPredictedPlayerState provided by src/cgame_mp/cg_main_mp.cpp now.
// void     CG_DrawPlayerActionSlotDpad(int, const rectDef_s *, const float *, Material *) {}  // provided by cg_weapons.cpp / cg_ammocounter.cpp now
// void     CG_DrawPlayerWeaponAmmoStock(int, const rectDef_s *, Font_s *, float, float *, Material *, int) {}  // provided by cg_weapons.cpp / cg_ammocounter.cpp now
// void     CG_DrawPlayerWeaponBackground(int, const rectDef_s *, const float *, Material *) {}  // provided by cg_weapons.cpp / cg_ammocounter.cpp now
// int32_t  BG_PlayerWeaponCountPrimaryTypes(const playerState_s *) { return 0; }  // provided by bg_weapons.cpp now
// void     CG_DrawPlayerWeaponLowAmmoWarning(int, const rectDef_s *, Font_s *, float, int, float, float, char, Material *) {}  // provided by cg_weapons.cpp / cg_ammocounter.cpp now
// void     CG_DrawPlayerWeaponAmmoClipGraphic(int, const rectDef_s *, const float *) {}  // provided by cg_weapons.cpp / cg_ammocounter.cpp now

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

// void Menu_Setup(UiContext *) {}  // provided by ui_shared.cpp now
// void BG_LoadAnim() {}  // provided by bg_animation_mp.cpp now
void CG_Veh_Init() {}
// MenuList *UI_LoadMenus(char *, int) { return nullptr; }  // provided by ui_shared.cpp now
// void AimAssist_Init(int) {}  // provided by aim_assist.cpp now
// void UI_AddMenuList(UiContext *, MenuList *) {}  // provided by ui_shared.cpp now
// CL_RegisterFont provided by src/client_mp/cl_main_mp.cpp now.
void SND_StopAmbient(int, int) {}
// void BG_RegisterDvars() {}  // provided by bg_misc.cpp now
void FX_KillEffectDef(int, const FxEffectDef *) {}
XModel *FX_RegisterModel(const char *) { return nullptr; }
// menuDef_t *Menus_FindByName(const UiContext *, const char *) { return nullptr; }  // provided by ui_shared.cpp now
// void BG_ClearWeaponDef() {}  // provided by bg_weapons.cpp now
// Com_StripExtension provided by src/universal/q_shared.cpp now.
// void UI_LoadIngameMenus(int) {}  // provided by ui_main_mp.cpp now
void CG_VehRegisterDvars() {}
// int32_t CG_WeaponDObjHandle(int32_t) { return 0; }  // provided by cg_weapons.cpp / cg_ammocounter.cpp now
// void Menus_FreeAllMemory(UiContext *) {}  // provided by ui_shared.cpp now
char SND_AddLengthNotify(int, const snd_alias_t *, SndLengthId) { return 0; }
void SND_StopSoundsOnEnt(SndEntHandle) {}
void Com_LoadSoundAliases(const char *, const char *, snd_alias_system_t) {}
void SND_PlayAmbientAlias(int, const snd_alias_t *, int, snd_alias_system_t) {}
void Snd_AssertAliasValid(snd_alias_t *) {}
int32_t DB_GetAllXAssetOfType(XAssetType, XAssetHeader *, int32_t) { return 0; }
// void DynEntCl_RegisterDvars() {}
// FS_ListFilesInLocation provided by src/universal/com_files.cpp now.
void SND_AddPlayFXSoundAlias(snd_alias_t *, SndEntHandle, const float *) {}
void SND_StopSoundAliasOnEnt(SndEntHandle, const char *) {}
void Scr_ShutdownGameStrings() {}
void FX_RegisterDefaultEffect() {}
snd_alias_list_t *Com_FindSoundAliasNoErrors(const char *) { return nullptr; }
snd_alias_t *Com_PickSoundAliasFromList(snd_alias_list_t *) { return nullptr; }
int SND_PlaySoundAliasAsMaster(const snd_alias_t *, SndEntHandle, const float *, int, snd_alias_system_t) { return 0; }
// void CG_AmmoCounterRegisterDvars() {}  // provided by cg_weapons.cpp / cg_ammocounter.cpp now
// void BG_LoadPenetrationDepthTable() {}  // provided by bg_weapons.cpp now
// uint8_t *Hunk_AllocPhysPresetPrecache(unsigned int size) { return static_cast<uint8_t *>(std::calloc(size > 0 ? size : 1, 1)); }

// unsigned int bg_lastParsedWeaponIndex = 0;  // provided by bg_weapons.cpp now
// clientConnections provided by src/client_mp/cl_main_mp.cpp now.
// g_compassShowEnemies provided by src/game_mp/g_main_mp.cpp now.
// === sv_voice_mp satellites ======================================================

// level_bgs provided by src/game_mp/g_main_mp.cpp now.
// voice_deadChat provided by src/game_mp/g_main_mp.cpp now.
// voice_global provided by src/game_mp/g_main_mp.cpp now.
// voice_localEcho provided by src/game_mp/g_main_mp.cpp now.
// === g_trigger_mp satellites =====================================================

// G_SpawnInt provided by src/game_mp/g_spawn_mp.cpp now.
// G_SpawnFloat provided by src/game_mp/g_spawn_mp.cpp now.
// Scr_AddEntity provided by src/game_mp/g_spawn_mp.cpp now.
void    Scr_AddVector(const float *) {}
// int     CM_AreaEntities(const float *, const float *, int *, int, int) { return 0; }
// AddPointToBounds provided by src/universal/com_math.cpp now.
// G_FreeEntityDelay provided by src/game_mp/g_utils_mp.cpp now.
// G_LevelSpawnString provided by src/game_mp/g_spawn_mp.cpp now.
BOOL    Scr_IsSystemActive() { return 0; }
// int     SV_SightTraceToEntity(float *, float *, float *, float *, int, int) { return 0; }  // provided by sv_world.cpp now

// === cl_scrn_mp satellites =======================================================

void   R_EndFrame() {}
// void   UI_Refresh(int) {}  // provided by ui_main_mp.cpp now
// CL_DrawLogo provided by src/client_mp/cl_main_mp.cpp now.
// void   DevGui_Draw(int) {}
void   R_BeginFrame() {}
// void   UI_UpdateTime(int, int) {}  // provided by ui_main_mp.cpp now
// void   Con_DrawConsole(int) {}  // provided by cl_console.cpp now
void   R_EndCubemapShot(CubemapShot) {}
void   SND_InitFXSounds() {}
// double UI_GetBlurRadius(int) { return 0.0; }  // provided by ui_main_mp.cpp now
void   R_AddCmdEndOfList() {}
void   R_SaveCubemapShot(char *, CubemapShot, float, float) {}
void   SCR_DrawCinematic(int) {}
// void   Net_DisplayProfile(int) {}  // provided by net_chan_mp.cpp now
void   R_BeginCubemapShot(int, int) {}
void   R_AddCmdClearScreen(int, const float *, float, unsigned char) {}
void   R_AddCmdDrawProfile() {}
void   R_BeginSharedCmdList() {}
void   Sys_LoadingKeepAlive() {}
// void   UI_DrawConnectScreen(int) {}  // provided by ui_main_mp.cpp now
void   R_IssueRenderCommands(unsigned int) {}
void   R_BeginClientCmdList2D() {}
void   R_ClearClientCmdList2D() {}
// void   R_BspGenerateReflections() {}  // provided by r_reflection_probe.cpp now
void   R_LightingFromCubemapShots(const float *) {}
// CL_AnyLocalClientChallenging provided by src/client_mp/cl_main_mp.cpp now.
// CL_AllLocalClientsDisconnected provided by src/client_mp/cl_main_mp.cpp now.
// unsigned int FS_FTell(int) { return 0u; }  // provided by com_files.cpp now
// const dvar_t *r_reflectionProbeGenerate = nullptr;  // provided by r_dvars.cpp now

// === cl_ui_mp satellites =========================================================

// void UI_Shutdown(int) {}  // provided by ui_main_mp.cpp now
void UI_Component_Init() {}
// const char *Key_KeynumToString(int32_t, int32_t) { return ""; }  // provided by cl_keys.cpp now
// CL_UpdateDirtyPings provided by src/client_mp/cl_main_mp.cpp now.
// char *UI_GetMapDisplayName(const char *) { return const_cast<char *>(""); }  // provided by ui_main_mp.cpp now
void R_PushRemoteScreenUpdate(int) {}
// char *UI_GetGameTypeDisplayName(const char *) { return const_cast<char *>(""); }  // provided by ui_main_mp.cpp now
// void UI_Init(int) {}  // provided by ui_main_mp.cpp now

// === g_misc_mp satellites ========================================================

// G_AddEvent provided by src/game_mp/g_utils_mp.cpp now.
// G_SetAngle provided by src/game_mp/g_utils_mp.cpp now.
// void YawVectors(float, float *f, float *r) { if (f) { f[0] = 1; f[1] = 0; f[2] = 0; } if (r) { r[0] = 0; r[1] = 1; r[2] = 0; } }  // provided by com_math.cpp now
// G_SetOrigin provided by src/game_mp/g_utils_mp.cpp now.
// G_GeneralLink provided by src/game_mp/g_utils_mp.cpp now.
// float ColorNormalize(const float *, float *out) { if (out) { out[0] = 1; out[1] = 1; out[2] = 1; out[3] = 1; } return 1.f; }  // provided by com_math.cpp now
// G_TraceCapsule provided by src/game_mp/g_main_mp.cpp now.
// void SV_UnlinkEntity(gentity_s *) {}  // provided by sv_world.cpp now
// G_PlaySoundAlias provided by src/game_mp/g_utils_mp.cpp now.
// int32_t IsItemRegistered(uint32_t) { return 0; }  // provided by game/ batch now
// G_LocationalTrace provided by src/game_mp/g_main_mp.cpp now.
// G_SoundAliasIndex provided by src/game_mp/g_utils_mp.cpp now.
// SetClientViewAngle provided by src/game_mp/g_client_mp.cpp now.
// G_GetPlayerViewOrigin provided by src/game_mp/g_client_mp.cpp now.
// void DObjSetControlTagAngles(DObj_s *, int *, unsigned int, float *) {}
// G_DObjGetLocalTagMatrix provided by src/game_mp/g_utils_mp.cpp now.
// G_DObjGetWorldTagMatrix provided by src/game_mp/g_utils_mp.cpp now.
// uint32_t G_GetWeaponIndexForName(const char *) { return 0u; }  // provided by game/ batch now
// BG_GetPlayerViewDirection provided by src/bgame/bg_misc.cpp now.
// gentity_s *Weapon_RocketLauncher_Fire(gentity_s *, uint32_t, float, struct weaponParms *, const float *, gentity_s *, const float *) { return nullptr; }  // provided by game/ batch now

// === g_utils_mp satellites =======================================================

// player_die provided by src/game_mp/g_combat_mp.cpp now.
// Helicopter_Die provided by src/game_mp/g_scr_helicopter.cpp now.
// Scr_FreeEntity provided by src/game_mp/g_spawn_mp.cpp now.
void Scr_FreeThread(uint16_t) {}
void DB_ReplaceModel(const char *, const char *) {}
// G_VehFreeEntity provided by src/game_mp/g_vehicles_mp.cpp now.
// Helicopter_Pain provided by src/game_mp/g_scr_helicopter.cpp now.
// MatrixTranspose provided by src/universal/com_math.cpp now.
// void Touch_Item_Auto(gentity_s *, gentity_s *, int) {}  // provided by game/ batch now
// void G_ExplodeMissile(gentity_s *) {}  // provided by g_missile.cpp now
// Helicopter_Think provided by src/game_mp/g_scr_helicopter.cpp now.
// G_VehUnlinkPlayer provided by src/game_mp/g_vehicles_mp.cpp now.
// PlayerCorpse_Free provided by src/game_mp/g_player_corpse_mp.cpp now.
// Scr_ExecEntThread provided by src/game_mp/g_spawn_mp.cpp now.
// void FinishSpawningItem(gentity_s *) {}  // provided by game/ batch now
// G_PlayerController provided by src/game_mp/g_active_mp.cpp now.
// void G_TimedObjectThink(gentity_s *) {}  // provided by g_missile.cpp now
// G_VehEntHandler_Die provided by src/game_mp/g_vehicles_mp.cpp now.
// G_VehEntHandler_Use provided by src/game_mp/g_vehicles_mp.cpp now.
// DObj_s *Com_ServerDObjCreate(DObjModel_s *, unsigned short, XAnimTree_s *, unsigned int) { return nullptr; }
// void DroppedItemClearOwner(gentity_s *) {}  // provided by game/ batch now
// G_VehEntHandler_Think provided by src/game_mp/g_vehicles_mp.cpp now.
// G_VehEntHandler_Touch provided by src/game_mp/g_vehicles_mp.cpp now.
// Helicopter_Controller provided by src/game_mp/g_scr_helicopter.cpp now.
// void Com_SafeServerDObjFree(unsigned int) {}
// unsigned int SL_FindLowercaseString(const char *) { return 0u; }  // provided by scr_variable/scr_stringlist now
// SV_GetConfigstringConst provided by src/server_mp/sv_init_mp.cpp now.
void Hunk_OverrideDataForFile(int, const char *, void *) {}
// MatrixInverseOrthogonal43 provided by src/universal/com_math.cpp now.
// void Missile_FreeAttractorRefs(gentity_s *) {}  // provided by g_missile.cpp now
// G_VehEntHandler_Controller provided by src/game_mp/g_vehicles_mp.cpp now.
// BodyEnd provided by src/game_mp/g_client_script_cmd_mp.cpp now.
// AxisClear provided by src/universal/com_math.cpp now.
// g_scr_data provided by src/game_mp/g_scr_main_mp.cpp now.

// === cl_input satellites =========================================================

// void UI_MouseEvent(int, int, int) {}  // provided by ui_main_mp.cpp now
// bool DevGui_IsActive() { return false; }
bool Sys_IsLANAddress(netadr_t) { return false; }
void IN_ShowSystemCursor(BOOL) {}
// void AimAssist_UpdateMouseInput(const AimInput *, AimOutput *) {}  // provided by aim_assist.cpp now
// CL_SavePredictedOriginForServerTime provided by src/client_mp/cl_parse_mp.cpp now.
// char ClampChar(int v) { if (v < -128) return -128; if (v > 127) return 127; return static_cast<char>(v); }  // provided by com_math.cpp now
void UI_Component::MouseEvent(int, int) {}

// cl_debugMessageKey provided by src/client_mp/cl_main_mp.cpp now.
// cl_freelook provided by src/client_mp/cl_main_mp.cpp now.
// cl_maxpackets provided by src/client_mp/cl_main_mp.cpp now.
// cl_mouseAccel provided by src/client_mp/cl_main_mp.cpp now.
// cl_nodelta provided by src/client_mp/cl_main_mp.cpp now.
// cl_packetdup provided by src/client_mp/cl_main_mp.cpp now.
// cl_sensitivity provided by src/client_mp/cl_main_mp.cpp now.
// cl_showMouseRate provided by src/client_mp/cl_main_mp.cpp now.
// frame_msec provided by src/client_mp/cl_main_mp.cpp now.
// m_filter provided by src/client_mp/cl_main_mp.cpp now.
// m_forward provided by src/client_mp/cl_main_mp.cpp now.
// m_pitch provided by src/client_mp/cl_main_mp.cpp now.
// m_side provided by src/client_mp/cl_main_mp.cpp now.
// m_yaw provided by src/client_mp/cl_main_mp.cpp now.
// playerKeys storage provided by src/client/cl_keys.cpp now.

// === sv_init_mp satellites =======================================================

// void FS_Restart(int, int) {}  // provided by com_files.cpp now
// CL_InitLoad provided by src/client_mp/cl_main_mp.cpp now.
// SV_RunFrame provided by src/server_mp/sv_main_mp.cpp now.
// CL_MapLoading provided by src/client_mp/cl_main_mp.cpp now.
// ClientConnect provided by src/game_mp/g_client_mp.cpp now.
// SV_FreeClients provided by src/server_mp/sv_client_mp.cpp now.
// SV_Heartbeat_f provided by src/server_mp/sv_ccmds_mp.cpp now.
// SV_InitSnapshot provided by src/server_mp/sv_main_mp.cpp now.
// char *FS_LoadedIwdNames() { return const_cast<char *>(""); }  // provided by com_files.cpp now
// SV_SendDisconnect provided by src/server_mp/sv_client_mp.cpp now.
void DB_UpdateDebugZone() {}
// void Hunk_FreeTempMemory(char *) {}
// SV_EndClientSnapshot provided by src/server_mp/sv_snapshot_mp.cpp now.
// void FS_ClearIwdReferences() {}  // provided by com_files.cpp now
// char *FS_LoadedIwdChecksums() { return const_cast<char *>(""); }  // provided by com_files.cpp now
// char *FS_ReferencedIwdNames() { return const_cast<char *>(""); }  // provided by com_files.cpp now
// Scr_ParseGameTypeList provided by src/game_mp/g_scr_main_mp.cpp now.
// CL_IsLocalClientActive provided by src/client_mp/cl_main_mp.cpp now.
// SV_AddOperatorCommands provided by src/server_mp/sv_ccmds_mp.cpp now.
// SV_BeginClientSnapshot provided by src/server_mp/sv_snapshot_mp.cpp now.
// SV_SetSystemInfoConfig provided by src/server_mp/sv_main_mp.cpp now.
char *DB_ReferencedFFNameList() { return const_cast<char *>(""); }
char *DB_ReferencedFFChecksums() { return const_cast<char *>(""); }
// SV_GetServerStaticHeader provided by src/server_mp/sv_snapshot_mp.cpp now.
// SV_SetServerStaticHeader provided by src/server_mp/sv_snapshot_mp.cpp now.
// SV_WriteSnapshotToClient provided by src/server_mp/sv_snapshot_mp.cpp now.
// char *FS_ReferencedIwdChecksums() { return const_cast<char *>(""); }  // provided by com_files.cpp now
// SV_WriteEntityFieldNumbers provided by src/server_mp/sv_snapshot_profile_mp.cpp now.
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

// BG_SetPerk provided by src/game_mp/g_client_script_cmd_mp.cpp now.
// char *I_CleanStr(char *s) { return s; }  // provided by q_shared.cpp now
// int I_DrawStrlen(const char *s) { return s ? static_cast<int>(std::strlen(s)) : 0; }  // provided by q_shared.cpp now
// SV_BanClient provided by src/server_mp/sv_client_mp.cpp now.
void Scr_DoProfile(float) {}
// void FS_ConvertPath(char *) {}  // provided by com_files.cpp now
// SV_UnbanClient provided by src/server_mp/sv_client_mp.cpp now.
void Scr_RunDebugger() {}
// G_GetClientScore provided by src/game_mp/g_main_mp.cpp now.
// G_GetClientState provided by src/game_mp/g_main_mp.cpp now.
// G_SetSavePersist provided by src/game_mp/g_main_mp.cpp now.
// SV_BanGuidBriefly provided by src/server_mp/sv_client_mp.cpp now.
// SV_AddServerCommand provided by src/server_mp/sv_main_mp.cpp now.
// SV_ClientEnterWorld provided by src/server_mp/sv_client_mp.cpp now.
void Scr_DoProfileBuiltin(float) {}
// void Scr_DumpScriptThreads() {}  // provided by scr_variable/scr_stringlist now
void Scr_RunDebuggerRemote() {}
// void Scr_DumpScriptVariables(bool, bool, bool, bool, bool, const char *, const char *, int) {}  // provided by scr_variable/scr_stringlist now
void Steam_SV_AddTestCommands() {}

// === sv_main_mp satellites =======================================================

// G_RunFrame provided by src/game_mp/g_main_mp.cpp now.
// void FakeLag_Frame() {}  // provided by net_chan_mp.cpp now
// void Scr_FreeValue(unsigned int) {}  // provided by scr_variable/scr_stringlist now
// SV_ClientThink provided by src/server_mp/sv_client_mp.cpp now.
void Scr_SetLoading(int) {}
// int  Netchan_Process(netchan_t *, msg_t *) { return 0; }  // provided by net_chan_mp.cpp now
// SV_GetChallenge provided by src/server_mp/sv_client_mp.cpp now.
// SV_ReceiveStats provided by src/server_mp/sv_client_mp.cpp now.
// SV_DirectConnect provided by src/server_mp/sv_client_mp.cpp now.
WinThreadLock Win_GetThreadLock() { return {}; }
// SV_DelayDropClient provided by src/server_mp/sv_client_mp.cpp now.
void Scr_UpdateDebugger() {}
// SV_SendClientMessages provided by src/server_mp/sv_snapshot_mp.cpp now.
// G_GetClientArchiveTime provided by src/game_mp/g_main_mp.cpp now.
// SV_ExecuteClientMessage provided by src/server_mp/sv_client_mp.cpp now.
// void MSG_WriteReliableCommandToBuffer(const char *, char *, int) {}  // provided by sv_msg_write_mp.cpp now

// tempServerMsgBuf provided by src/server_mp/sv_snapshot_mp.cpp now.

// === sv_client_mp satellites =====================================================

// ClientBegin provided by src/game_mp/g_client_mp.cpp now.
// ClientThink provided by src/game_mp/g_active_mp.cpp now.
// int  FS_WriteFile(char *, char *, unsigned int) { return 0; }  // provided by com_files.cpp now
// ClientCommand provided by src/game_mp/g_cmds_mp.cpp now.
// void Netchan_Setup(netsrc_t, netchan_t *, netadr_t, int, char *, int, char *, int) {}  // provided by net_chan_mp.cpp now
// bool NET_CompareAdr(netadr_t, netadr_t) { return false; }  // provided by net_chan_mp.cpp now
// void MSG_WriteEntity(SnapshotInfo_s *, msg_t *, int, entityState_s *, const entityState_s *, int) {}  // provided by sv_msg_write_mp.cpp now
// bool BG_IsWeaponValid(const playerState_s *, uint32_t) { return false; }  // provided by bg_weapons.cpp now
// ClientDisconnect provided by src/game_mp/g_client_mp.cpp now.
// G_SetLastServerTime provided by src/game_mp/g_active_mp.cpp now.
// SV_PacketDataIsHeader provided by src/server_mp/sv_snapshot_profile_mp.cpp now.
bool Steam_CheckClientTicket(unsigned char *, unsigned int, unsigned long long) { return true; }
void Steam_OnClientDropped(unsigned long long) {}
// SV_BuildClientSnapshot provided by src/server_mp/sv_snapshot_mp.cpp now.
// SV_SendMessageToClient provided by src/server_mp/sv_snapshot_mp.cpp now.
// bool BG_ValidateWeaponNumber(uint32_t) { return true; }  // provided by bg_weapons.cpp now
bool Sys_IsLANAddress_IgnoreSubnet(netadr_t) { return false; }
// SV_UpdateServerCommandsToClient provided by src/server_mp/sv_snapshot_mp.cpp now.

// const dvar_t *net_lanauthorize = nullptr;  // provided by net_chan_mp.cpp now

// === sv_snapshot_profile_mp satellites ===========================================

// unsigned int MSG_GetBitCount(int, bool *, int, int) { return 0u; }  // provided by sv_msg_write_mp.cpp now
// cl_profileTextY provided by src/client_mp/cl_main_mp.cpp now.
// s_clientSnapshotData storage provided by src/qcommon/net_chan_mp.cpp now.

// === cl_main_pc_mp satellites ====================================================
// CL_GetServerStatus provided by src/client_mp/cl_main_mp.cpp now.
// int NET_CompareAdrSigned(netadr_t *, netadr_t *) { return 0; }  // provided by net_chan_mp.cpp now
const char *Steam_GetClientID() { return ""; }
void Steam_RequestAuthTicket() {}

// cl_pinglist / cl_serverStatusList provided by src/client_mp/cl_main_mp.cpp now.
// cl_serverStatusResendTime provided by src/client_mp/cl_main_mp.cpp now.
// int g_qport = 0;  // provided by net_chan_mp.cpp now

// === cl_parse_mp satellites ======================================================

// char *FS_ShiftStr(const char *, char) { return const_cast<char *>(""); }  // provided by com_files.cpp now
void  Sys_OpenURL(const char *, int) {}
// void  FS_SV_Rename(char *, char *) {}  // provided by com_files.cpp now
// CL_ClearState provided by src/client_mp/cl_main_mp.cpp now.
// Info_NextPair provided by src/universal/q_shared.cpp now.
// bool  FS_NeedRestart(int) { return false; }  // provided by com_files.cpp now
// CL_DownloadsComplete provided by src/client_mp/cl_main_mp.cpp now.
// int   FS_SV_FOpenFileWrite(const char *) { return 0; }  // provided by com_files.cpp now
// CL_ClearStaticDownload provided by src/client_mp/cl_main_mp.cpp now.
// CL_RequestAuthorization provided by src/client_mp/cl_main_mp.cpp now.

// cl_allowDownload provided by src/client_mp/cl_main_mp.cpp now.
// cl_shownuments provided by src/client_mp/cl_main_mp.cpp now.
// cl_updatefiles provided by src/client_mp/cl_main_mp.cpp now.
// LegacyHacks legacyHacks{};  // provided by ui_main_mp.cpp now

// === sv_snapshot_mp satellites ===================================================

// G_GetClientSize provided by src/game_mp/g_main_mp.cpp now.
// G_GetPlayerState provided by src/game_mp/g_main_mp.cpp now.
// int FS_SV_FOpenFileRead(const char *, int *fp) { if (fp) *fp = 0; return 0; }  // provided by com_files.cpp now
// GetFollowPlayerState provided by src/game_mp/g_active_mp.cpp now.
// G_SetClientArchiveTime provided by src/game_mp/g_main_mp.cpp now.
// int irand(int min, int /*max*/) { return min; }  // provided by com_math.cpp now

// === g_client_mp satellites ======================================================

// StopFollowing provided by src/game_mp/g_cmds_mp.cpp now.
// ClientEndFrame provided by src/game_mp/g_active_mp.cpp now.
// ClientThink_real provided by src/game_mp/g_active_mp.cpp now.
// Scr_PlayerConnect provided by src/game_mp/g_scr_main_mp.cpp now.
// G_SetClientContents provided by src/game_mp/g_active_mp.cpp now.
// Scr_PlayerDisconnect provided by src/game_mp/g_scr_main_mp.cpp now.
// void BG_GetPlayerViewOrigin(const playerState_s *, float *o, int32_t) { if (o) { o[0] = o[1] = o[2] = 0; } }  // provided by bg_misc.cpp now
// void HudElem_ClientDisconnect(gentity_s *) {}  // provided by game/ batch now

// const dvar_t *bg_prone_yawcap = nullptr;  // provided by bg_misc.cpp now
// g_inactivity provided by src/game_mp/g_main_mp.cpp now.
// g_password provided by src/game_mp/g_main_mp.cpp now.
// === g_spawn_mp satellites =======================================================

// unsigned int G_NewString(const char *) { return 0u; }  // provided by game/ batch now
// void G_SpawnItem(gentity_s *, const gitem_s *) {}  // provided by game/ batch now
int Scr_GetType(unsigned int) { return 0; }
// void trigger_use(gentity_s *) {}  // provided by game/ batch now
// G_VehSpawner provided by src/game_mp/g_vehicles_mp.cpp now.
// int32_t G_SpawnString(const SpawnVar *, const char *, const char *def, const char **out) { if (out) *out = def; return 0; }  // provided by game/ batch now
// void Scr_AddFields(const char *, const char *) {}  // provided by scr_variable/scr_stringlist now
void Scr_AddObject(unsigned int) {}
// unsigned int Scr_FindField(const char *, int *type) { if (type) *type = 0; return 0u; }  // provided by scr_variable/scr_stringlist now
unsigned int Scr_GetObject(unsigned int) { return 0u; }
// int Scr_GetOffset(unsigned int, const char *) { return 0; }  // provided by scr_variable/scr_stringlist now
void Scr_MakeArray() {}
// Scr_SetAngles provided by src/game_mp/g_scr_main_mp.cpp now.
// Scr_SetHealth provided by src/game_mp/g_scr_main_mp.cpp now.
// Scr_SetOrigin provided by src/game_mp/g_scr_main_mp.cpp now.
uint16_t Scr_ExecThread(int, unsigned int) { return 0; }
// int32_t G_ParseSpawnVars(SpawnVar *) { return 0; }  // provided by game/ batch now
void Scr_AddEntityNum(unsigned int, unsigned int) {}
scr_entref_t Scr_GetEntityRef(unsigned int) { return {}; }
void Scr_AddExecThread(int, unsigned int) {}
// void Scr_FreeEntityNum(unsigned int, unsigned int) {}  // provided by scr_variable/scr_stringlist now
void Scr_SetStructField(unsigned int, unsigned int) {}
// G_VehCollmapSpawner provided by src/game_mp/g_vehicles_mp.cpp now.
// void Scr_GetHudElemField(uint32_t, uint32_t) {}  // provided by game/ batch now
// void Scr_SetHudElemField(uint32_t, uint32_t) {}  // provided by game/ batch now
// const gitem_s *BG_FindItemForWeapon(uint32_t, int32_t) { return nullptr; }  // provided by bg_misc.cpp now
uint16_t Scr_ExecEntThreadNum(unsigned int, unsigned int, int, unsigned int) { return 0; }
// bool Com_IsLegacyXModelName(const char *) { return false; }  // provided by q_shared.cpp now
void Scr_SetDynamicEntityField(unsigned int, unsigned int, unsigned int) {}
// void Scr_FreeHudElemConstStrings(game_hudelem_s *) {}  // provided by game/ batch now
unsigned int Scr_GetConstStringIncludeNull(unsigned int) { return 0u; }

// g_gravity provided by src/game_mp/g_main_mp.cpp now.
// g_motd provided by src/game_mp/g_main_mp.cpp now.
// g_hudelems storage provided by src/game/g_hudelem.cpp now.
void Scr_NotifyNum(unsigned int, unsigned int, unsigned int, unsigned int) {}

// === player_use_mp satellites ====================================================
// G_VehUsable provided by src/game_mp/g_vehicles_mp.cpp now.
// G_GetHintStringIndex provided by src/game_mp/g_scr_main_mp.cpp now.
// bool BG_ThrowingBackGrenade(const playerState_s *) { return false; }  // provided by bg_weapons.cpp now
// G_TraceCapsuleComplete provided by src/game_mp/g_main_mp.cpp now.

// g_useholdspawndelay provided by src/game_mp/g_main_mp.cpp now.
// g_useholdtime provided by src/game_mp/g_main_mp.cpp now.
// player_MGUseRadius provided by src/game_mp/g_main_mp.cpp now.
// player_throwbackInnerRadius provided by src/game_mp/g_main_mp.cpp now.
// player_throwbackOuterRadius provided by src/game_mp/g_main_mp.cpp now.
// === g_combat_mp satellites ======================================================

// Cmd_Score_f provided by src/game_mp/g_cmds_mp.cpp now.
// void BG_StringCopy(uint8_t *m, const char *k) { if (m && k) std::strcpy(reinterpret_cast<char *>(m), k); }  // provided by bg_weapons.cpp now
// gentity_s *G_FireGrenade(gentity_s *, float *, float *, uint32_t, uint8_t, int32_t, int32_t) { return nullptr; }  // provided by g_missile.cpp now
// bool LogAccuracyHit(gentity_s *, gentity_s *) { return false; }  // provided by game/ batch now
// unsigned int Scr_AllocString(char *, int) { return 0u; }  // provided by scr_variable/scr_stringlist now
void Scr_AddUndefined() {}
// Scr_PlayerDamage provided by src/game_mp/g_scr_main_mp.cpp now.
// Scr_PlayerKilled provided by src/game_mp/g_scr_main_mp.cpp now.
// void Vec3NormalizeFast(float *v) { if (v) { float l = std::sqrt(v[0]*v[0]+v[1]*v[1]+v[2]*v[2]); if (l > 0) { v[0]/=l; v[1]/=l; v[2]/=l; } } }  // provided by com_math.cpp now
// G_VehImmuneToDamage provided by src/game_mp/g_vehicles_mp.cpp now.
// void BG_SetConditionValue(uint32_t, uint32_t, uint64_t) {}  // provided by bg_animation_mp.cpp now
// void DObjPhysicsGetBounds(const DObj_s *, float *mins, float *maxs)
// {
//     if (mins) { mins[0] = mins[1] = mins[2] = 0; }
//     if (maxs) { maxs[0] = maxs[1] = maxs[2] = 0; }
// }
// G_LocationalTracePassed provided by src/game_mp/g_main_mp.cpp now.
// uint32_t BG_FindWeaponIndexForName(const char *) { return 0u; }  // provided by bg_weapons.cpp now

// g_debugDamage provided by src/game_mp/g_main_mp.cpp now.
// radius_damage_debug provided by src/game_mp/g_main_mp.cpp now.
// === g_scr_helicopter satellites =================================================
// SpawnVehicle provided by src/game_mp/g_vehicles_mp.cpp now.
// VEH_JoltBody provided by src/game_mp/g_vehicles_mp.cpp now.
// void VEH_InitEntity(gentity_s *, scr_vehicle_s *, int) {}  // provided by g_scr_vehicle.cpp now
// gentity_s *GScr_GetVehicle(scr_entref_t) { return nullptr; }  // provided by g_scr_vehicle.cpp now
// void VEH_InitVehicle(gentity_s *, scr_vehicle_s *, short) {}  // provided by g_scr_vehicle.cpp now
// VEH_SetPosition provided by src/game_mp/g_vehicles_mp.cpp now.
// void CMD_VEH_GetSpeed(scr_entref_t) {}  // provided by g_scr_vehicle.cpp now
// void CMD_VEH_SetSpeed(scr_entref_t) {}  // provided by g_scr_vehicle.cpp now
// void CMD_VEH_SetWeapon(scr_entref_t) {}  // provided by g_scr_vehicle.cpp now
// void Scr_Vehicle_Think(gentity_s *) {}  // provided by g_scr_vehicle.cpp now
// void CMD_VEH_FireWeapon(scr_entref_t) {}  // provided by g_scr_vehicle.cpp now
// void CMD_VEH_SetGoalPos(scr_entref_t) {}  // provided by g_scr_vehicle.cpp now
// void CMD_VEH_SetGoalYaw(scr_entref_t) {}  // provided by g_scr_vehicle.cpp now
// void CMD_VEH_GetSpeedMPH(scr_entref_t) {}  // provided by g_scr_vehicle.cpp now
// void CMD_VEH_ResumeSpeed(scr_entref_t) {}  // provided by g_scr_vehicle.cpp now
// void CMD_VEH_SetYawSpeed(scr_entref_t) {}  // provided by g_scr_vehicle.cpp now
// void CMD_VEH_ClearGoalYaw(scr_entref_t) {}  // provided by g_scr_vehicle.cpp now
// void CMD_VEH_SetLookAtEnt(scr_entref_t) {}  // provided by g_scr_vehicle.cpp now
// void CMD_VEH_SetTargetYaw(scr_entref_t) {}  // provided by g_scr_vehicle.cpp now
// int DObjSetLocalBoneIndex(DObj_s *, int *, int, const float *, const float *) { return 0; }
// void CMD_VEH_ClearLookAtEnt(scr_entref_t) {}  // provided by g_scr_vehicle.cpp now
// void CMD_VEH_ClearTargetYaw(scr_entref_t) {}  // provided by g_scr_vehicle.cpp now
// void CMD_VEH_SetHoverParams(scr_entref_t) {}  // provided by g_scr_vehicle.cpp now
// void CMD_VEH_SetVehicleTeam(scr_entref_t) {}  // provided by g_scr_vehicle.cpp now
// void CMD_VEH_SetAirResitance(scr_entref_t) {}  // provided by g_scr_vehicle.cpp now
// void CMD_VEH_SetMaxPitchRoll(scr_entref_t) {}  // provided by g_scr_vehicle.cpp now
// void CMD_VEH_SetTurningAbility(scr_entref_t) {}  // provided by g_scr_vehicle.cpp now
// void CMD_VEH_NearGoalNotifyDist(scr_entref_t) {}  // provided by g_scr_vehicle.cpp now
// void CMD_VEH_SetTurretTargetEnt(scr_entref_t) {}  // provided by g_scr_vehicle.cpp now
// void CMD_VEH_SetTurretTargetVec(scr_entref_t) {}  // provided by g_scr_vehicle.cpp now
// void CMD_VEH_ClearTurretTargetEnt(scr_entref_t) {}  // provided by g_scr_vehicle.cpp now

// vehicle_info_t s_vehicleInfos[32]{};  // provided by g_scr_vehicle.cpp now

// === g_active_mp satellites ======================================================

// void FireWeapon(gentity_s *, int32_t) {}  // provided by game/ batch now
// void G_UseOffHand(gentity_s *) {}  // provided by game/ batch now
// void FireWeaponMelee(gentity_s *, int32_t) {}  // provided by game/ batch now
// Cmd_FollowCycle_f provided by src/game_mp/g_cmds_mp.cpp now.
// void BG_WeaponFireRecoil(const playerState_s *, float *, float *) {}  // provided by bg_weapons.cpp now
// void ExpandBoundsToWidth(float *, float *) {}  // provided by com_math.cpp now
// G_VehPlayerRideSlot provided by src/game_mp/g_vehicles_mp.cpp now.
// void HudElem_UpdateClient(gclient_s *, int32_t, hudelem_update_t) {}  // provided by game/ batch now
// void BG_Player_DoControllers(const CEntPlayerInfo *, const DObj_s *, int32_t *) {}  // provided by bg_pmove.cpp now
// void BG_CalculateWeaponAngles(weaponState_t *, float *) {}  // provided by bg_weapons.cpp now
// void BG_CalculateWeaponPosition_Sway(const playerState_s *, float *, float *, float *, float, int32_t) {}  // provided by bg_weapons.cpp now

// g_antilag provided by src/game_mp/g_main_mp.cpp now.
// g_mantleBlockTimeBuffer provided by src/game_mp/g_main_mp.cpp now.
// g_playerCollisionEjectSpeed provided by src/game_mp/g_main_mp.cpp now.
// g_smoothClients provided by src/game_mp/g_main_mp.cpp now.
// g_speed provided by src/game_mp/g_main_mp.cpp now.
// g_synchronousClients provided by src/game_mp/g_main_mp.cpp now.
// === g_main_mp satellites ========================================================

// void G_RunMover(gentity_s *) {}  // provided by game/ batch now
// G_RunCorpse provided by src/game_mp/g_player_corpse_mp.cpp now.
void Scr_IncTime() {}
// void G_RunMissile(gentity_s *) {}  // provided by g_missile.cpp now
// void SV_SightTrace(int *hit, const float *, const float *, const float *, const float *, int, int, int) { if (hit) *hit = 0; }  // provided by sv_world.cpp now
// Scr_LoadLevel provided by src/game_mp/g_scr_main_mp.cpp now.
// void Z_VirtualFree(void *) {}
// G_VehiclesInit provided by src/game_mp/g_vehicles_mp.cpp now.
// int  SV_TracePassed(const float *, const float *, const float *, const float *, int, int, int, int, uint8_t *, int) { return 0; }  // provided by sv_world.cpp now
void Scr_InitSystem(int) {}
// SendScoreboard provided by src/game_mp/g_cmds_mp.cpp now.
// GScr_FreeScripts provided by src/game_mp/g_scr_main_mp.cpp now.
// GScr_LoadScripts provided by src/game_mp/g_scr_main_mp.cpp now.
// G_InitObjectives provided by src/game_mp/g_scr_main_mp.cpp now.
// void G_SetupWeaponDef() {}  // provided by game/ batch now
// Scr_LoadGameType provided by src/game_mp/g_scr_main_mp.cpp now.
// void HudElem_DestroyAll() {}  // provided by game/ batch now
// void Scr_FreeEntityList() {}  // provided by scr_variable/scr_stringlist now
void Scr_ShutdownSystem(uint8_t, int) {}
// void Hunk_ClearToMarkLow(int) {}
// void SaveRegisteredItems() {}  // provided by game/ batch now
// Scr_StartupGameType provided by src/game_mp/g_scr_main_mp.cpp now.
// uint8_t *Hunk_AllocXAnimServer(unsigned int size) { return static_cast<uint8_t *>(std::calloc(size > 0 ? size : 1, 1)); }
// void SaveRegisteredWeapons() {}  // provided by game/ batch now
// void Scr_AllocGameVariable() {}  // provided by scr_variable/scr_stringlist now
void Scr_RunCurrentThreads() {}
// void G_RegisterMissileDvars() {}  // provided by g_missile.cpp now
// void Missile_InitAttractors() {}  // provided by g_missile.cpp now
// void SV_SetupIgnoreEntParams(IgnoreEntParams *, int) {}  // provided by sv_world.cpp now
// G_VehiclesSetupSpawnedEnts provided by src/game_mp/g_vehicles_mp.cpp now.
// void G_RegisterMissileDebugDvars() {}  // provided by g_missile.cpp now
// G_setfog provided by src/game_mp/g_cmds_mp.cpp now.
// SV_Trace provided by src/server/sv_world.cpp now.
// void G_RunItem(gentity_s *) {}  // provided by game/ batch now
// void Rand_Init(int) {}  // provided by com_math.cpp now
// G_VehRegisterDvars provided by src/game_mp/g_vehicles_mp.cpp now.
// int SV_PointContents(float *, int, int) { return 0; }  // provided by sv_world.cpp now

// === g_cmds_mp satellites ========================================================

// const gitem_s *G_FindItem(const char *, int32_t) { return nullptr; }  // provided by bg_misc.cpp now
// void Touch_Item(gentity_s *, gentity_s *, int32_t) {}  // provided by game/ batch now
// Scr_PlayerVote provided by src/game_mp/g_scr_main_mp.cpp now.
// Scr_VoteCalled provided by src/game_mp/g_scr_main_mp.cpp now.
// void G_GetItemClassname(const gitem_s *, uint16_t *out) { if (out) *out = 0; }  // provided by game/ batch now
// int32_t G_GivePlayerWeapon(playerState_s *, int32_t, uint8_t) { return 0; }  // provided by game/ batch now
// int32_t BG_TakePlayerWeapon(playerState_s *, uint32_t, int32_t) { return 0; }  // provided by bg_weapons.cpp now
// void G_SelectWeaponIndex(int32_t, int32_t) {}  // provided by game/ batch now
// bool BG_CanPlayerHaveWeapon(uint32_t) { return false; }  // provided by bg_weapons.cpp now
// Scr_GetGameTypeNameForScript provided by src/game_mp/g_scr_main_mp.cpp now.
// char *vtos(const float *) { static char buf[64] = ""; return buf; }  // provided by game/ batch now
// int32_t Add_Ammo(gentity_s *, uint32_t, uint8_t, int32_t, int32_t) { return 0; }  // provided by game/ batch now

// === g_vehicles_mp satellites ====================================================

// void ExtendBounds(float *, float *, const float *) {}  // provided by com_math.cpp now
// void AnglesSubtract(float *, float *, float *out) { if (out) { out[0] = out[1] = out[2] = 0; } }  // provided by com_math.cpp now
// float DiffTrackAngle(float, float, float, float) { return 0.f; }  // provided by com_math.cpp now
// void VEH_ClipVelocity(float *in, float *, float *out) { if (in && out) { out[0] = in[0]; out[1] = in[1]; out[2] = in[2]; } }  // provided by g_scr_vehicle.cpp now
// int32_t G_TryPushingEntity(gentity_s *, gentity_s *, float *, float *) { return 0; }  // provided by game/ batch now
// int32_t VEH_CorrectAllSolid(gentity_s *, trace_t *) { return 0; }  // provided by g_scr_vehicle.cpp now
// VehicleLocalPhysics s_phys_0{};  // provided by g_scr_vehicle.cpp now
// VehiclePhysicsBackup s_backup_0{};  // provided by g_scr_vehicle.cpp now

// === g_client_script_cmd_mp satellites ===========================================

// gentity_s *Drop_Weapon(gentity_s *, int, unsigned char, unsigned int) { return nullptr; }  // provided by game/ batch now
void Scr_AddBool(unsigned int) {}
// int  BG_WeaponAmmo(const playerState_s *, unsigned int) { return 0; }  // provided by bg_weapons.cpp now
// GScr_AddEntity provided by src/game_mp/g_scr_main_mp.cpp now.
unsigned int Scr_GetPointerType(unsigned int) { return 0u; }
// GScr_GetLocSelIndex provided by src/game_mp/g_scr_main_mp.cpp now.
// void PM_ExitAimDownSight(playerState_s *) {}  // provided by bg_weapons.cpp now
// Scr_MakeGameMessage provided by src/game_mp/g_scr_main_mp.cpp now.
// Scr_PlayerLastStand provided by src/game_mp/g_scr_main_mp.cpp now.
// void G_SetEquippedOffHand(int, unsigned int) {}  // provided by game/ batch now
// Scr_VerifyWeaponIndex provided by src/game_mp/g_scr_main_mp.cpp now.
// GScr_GetScriptMenuIndex provided by src/game_mp/g_scr_main_mp.cpp now.
// Scr_ConstructMessageString provided by src/game_mp/g_scr_main_mp.cpp now.
unsigned int Scr_GetConstLowercaseString(unsigned int) { return 0u; }
// gentity_s *Drop_Item(gentity_s *, const gitem_s *, float, int) { return nullptr; }  // provided by game/ batch now
// void Fill_Clip(playerState_s *, unsigned int) {}  // provided by game/ batch now
// const dvar_t *player_dmgtimer_maxTime      = nullptr;  // provided by bg_misc.cpp now
// const dvar_t *player_dmgtimer_timePerPoint = nullptr;  // provided by bg_misc.cpp now

// === cl_main_mp satellites =======================================================

void R_Shutdown(int) {}
void SND_Update() {}
void Sys_ShowIP() {}
// void DevGui_Init() {}
// void FS_CopyFile(char *, char *) {}  // provided by com_files.cpp now
void SND_Restore(MemoryFile *) {}
// void FS_FileClose(FILE *) {}  // provided by com_files.cpp now
void SND_Shutdown() {}
// int  FS_FileExists(char *) { return 0; }  // provided by com_files.cpp now
// void UI_OpenMenu_f() {}  // provided by ui_main_mp.cpp now
// CL_DevGuiFrame provided by src/client/cl_devgui.cpp now.
// void DevGui_AddDvar(const char *, const dvar_s *) {}
// Font_s *R_RegisterFont(const char *, int) { return nullptr; }
void Sys_NormalExit() {}
// void UI_CloseMenu_f() {}  // provided by ui_main_mp.cpp now
// void UI_ListMenus_f() {}  // provided by ui_main_mp.cpp now
void Voice_Playback() {}
// char *Z_VirtualAlloc(int size, const char *, int) { return static_cast<char *>(std::calloc(size > 0 ? size : 1, 1)); }
// CL_CreateDevGui provided by src/client/cl_devgui.cpp now.
// void DevGui_OpenMenu(const char *) {}
// void DevGui_Shutdown() {}
void R_MakeDedicated(const GfxConfiguration *) {}
void Sys_ShowConsole() {}
// CL_DestroyDevGui provided by src/client/cl_devgui.cpp now.
int  DB_ModFileExists() { return 0; }
void SND_PlayFXSounds() {}
// bool NET_OutOfBandData(netsrc_t, netadr_t, const unsigned char *, int) { return false; }  // provided by net_chan_mp.cpp now
void SCR_StopCinematic(int) {}
void SND_SaveListeners(snd_listener *) {}
void CL_PlayCinematic_f() {}
// int  Hunk_HideTempMemory() { return 0; }
// void Hunk_ShowTempMemory(int) {}
void R_BeginRegistration(vidConfig_t *) {}
void R_ConfigureRenderer(const GfxConfiguration *) {}
// void Con_InitClientAssets() {}  // provided by cl_console.cpp now
void SND_RestoreListeners(snd_listener *) {}
void Sys_HideSplashWindow() {}
// int  FS_ConditionalRestart(int, int) { return 0; }  // provided by com_files.cpp now
void SND_DisconnectListener(int) {}
void LiveStorage_UploadStats() {}
void SND_UpdateLoopingSounds() {}
void Sys_QuitAndStartProcess(const char *, const char *) {}
void Voice_GetLocalVoiceData() {}
void Steam_CancelClientTicket() {}
unsigned long long Steam_GetClientSteamID64() { return 0ULL; }
unsigned int Steam_GetRawClientTicket(unsigned char **t, unsigned int *s) { if (t) *t = nullptr; if (s) *s = 0; return 0u; }
int  LiveStorage_DoWeHaveStats() { return 0; }
void *LiveStorage_GetStatBuffer() { return nullptr; }
// void Com_ClientDObjClearAllSkel() {}
void R_AddCmdDrawTextWithCursor(const char *, int, Font_s *, float, float, float, float, float, const float *, int, int, char) {}
// bool UI_AllowScriptMenuResponse(int) { return false; }  // provided by ui_main_mp.cpp now
void R_AddCmdDrawTextWithEffects(const char *, int, Font_s *, float, float, float, float, float, const float *, int, const float *, Material *, Material *, int, int, int, int) {}
int  LiveStorage_ReadStatsFromDir(char *) { return 0; }
void CL_PlayUnskippableCinematic_f() {}
// char *FS_ReferencedIwdPureChecksums() { return const_cast<char *>(""); }  // provided by com_files.cpp now
// void CL_SelectStringTableEntryInDvar_f() {}  // provided by ui_main_mp.cpp now
void Com_ProcessSoundAliasFileLocalization(char *, char *) {}
// void Con_Init() {}  // provided by cl_console.cpp now
void SND_Save(MemoryFile *) {}
// void FS_Remove(const char *) {}  // provided by com_files.cpp now
// int  Hunk_Used() { return 0; }
// b64_encode provided by src/client_mp/cl_main_mp.cpp now.

// int fs_checksumFeed = 0;  // provided by com_files.cpp now
// char fs_gamedir[256]{};  // provided by com_files.cpp now
// const dvar_t *fs_homepath = nullptr;  // provided by com_files.cpp now
// g_consoleField storage provided by src/client/cl_keys.cpp now.
// float g_console_char_height = 0.f;  // provided by cl_console.cpp now
// int32_t g_console_field_width = 0;  // provided by cl_console.cpp now
// const dvar_t *showpackets = nullptr;  // provided by net_chan_mp.cpp now
// const dvar_t *vehDriverViewHeightMin = nullptr;  // provided by ui_main_mp.cpp now

// === g_scr_main_mp satellites ====================================================

void Scr_AddAnim(scr_anim_s) {}
scr_anim_s Scr_GetAnim(unsigned int, XAnimTree_s *) { return {}; }
void Scr_AddStruct() {}
void GScr_AddVector(const float *) {}
void Scr_AddIString(const char *) {}
const char *Scr_GetIString(unsigned int) { return ""; }
// void GScr_NewHudElem() {}  // provided by game/ batch now
const char *Scr_GetTypeName(unsigned int) { return ""; }
// void Scr_SetClassMap(unsigned int) {}  // provided by scr_variable/scr_stringlist now
// void Scr_AddArrayKeys(unsigned int) {}  // provided by scr_variable/scr_stringlist now
void Scr_ResetTimeout() {}
// void (*HudElem_GetMethod(const char **))(scr_entref_t) { return nullptr; }  // provided by game/ batch now
const char *Scr_GetDebugString(unsigned int) { return ""; }
// void Scr_RemoveClassMap(unsigned int) {}  // provided by scr_variable/scr_stringlist now
// void GScr_NewTeamHudElem() {}  // provided by game/ batch now
// int  DObjGetModelBoneIndex(const DObj_s *, const char *, unsigned int, unsigned char *out) { if (out) *out = 0; return 0; }
// void GScr_NewClientHudElem() {}  // provided by game/ batch now
int  Scr_GetFunctionHandle(const char *, const char *) { return 0; }
void Scr_NeverTerminalError(const char *) {}
// void GScr_AddFieldsForHudElems() {}  // provided by game/ batch now
void Scr_AddArrayStringIndexed(unsigned int) {}
// void Scr_MissileDeleteAttractor() {}  // provided by g_missile.cpp now
// void Scr_MissileCreateRepulsorEnt() {}  // provided by g_missile.cpp now
// void Scr_MissileCreateAttractorEnt() {}  // provided by g_missile.cpp now
// void Scr_MissileCreateRepulsorOrigin() {}  // provided by g_missile.cpp now
// void Scr_MissileCreateAttractorOrigin() {}  // provided by g_missile.cpp now
snd_alias_list_t *Com_TryFindSoundAlias(const char *) { return nullptr; }

// === ui_main_mp satellites =======================================================

// int  Menu_Count(UiContext *) { return 0; }  // provided by ui_shared.cpp now
// char Menu_Paint(UiContext *, menuDef_t *) { return 0; }  // provided by ui_shared.cpp now
// void Menus_Open(UiContext *, menuDef_t *) {}  // provided by ui_shared.cpp now
// MenuList *UI_LoadMenu(char *, int) { return nullptr; }  // provided by ui_shared.cpp now
// void Key_SetCatcher(int, int) {}  // provided by cl_keys.cpp now
// void Menu_HandleKey(UiContext *, menuDef_t *, int, int) {}  // provided by ui_shared.cpp now
// char Menu_IsVisible(UiContext *, menuDef_t *) { return 0; }  // provided by ui_shared.cpp now
// void Menus_CloseAll(UiContext *) {}  // provided by ui_shared.cpp now
void IN_SetCursorPos(tagPOINT) {}
// void Key_ClearStates(int) {}  // provided by cl_keys.cpp now
// menuDef_t *Menu_GetFocused(UiContext *) { return nullptr; }  // provided by ui_shared.cpp now
// int  Menus_OpenByName(UiContext *, const char *) { return 0; }  // provided by ui_shared.cpp now
// int  Display_MouseMove(UiContext *) { return 0; }  // provided by ui_shared.cpp now
Material *Material_Duplicate(Material *, char *) { return nullptr; }
// int  Menus_MenuIsInStack(UiContext *, menuDef_t *) { return 0; }  // provided by ui_shared.cpp now
float Voice_GetVoiceLevel() { return 0.f; }
// int  Display_KeyBindPending() { return 0; }  // provided by ui_shared.cpp now
// int  Item_ListBox_MaxScroll(int, itemDef_s *) { return 0; }  // provided by ui_shared.cpp now
// void Menu_SetFeederSelection(UiContext *, menuDef_t *, int, int, const char *) {}  // provided by ui_shared.cpp now
void R_AddCmdDrawTextSubtitle(const char *, int, Font_s *, float, float, float, float, float, const float *, int, const float *, bool) {}
// void Menus_PrintAllLoadedMenus(UiContext *) {}  // provided by ui_shared.cpp now
// int  Menus_AnyFullScreenVisible(UiContext *) { return 0; }  // provided by ui_shared.cpp now
// int  SEH_VerifyLanguageSelection(int) { return 0; }  // provided by stringed_hooks.cpp now
// void LerpColor(float *, float *, float *out, float) { if (out) { out[0] = out[1] = out[2] = out[3] = 1.f; } }  // provided by ui_shared.cpp now

// field_t *g_editingField = nullptr;  // provided by ui_shared.cpp now

// === bg_animation_mp satellites ====================================================

// const dvar_t *anim_debugSpeeds              = nullptr;  // provided by bg_misc.cpp now
// const dvar_t *animscript_debug              = nullptr;  // provided by bg_misc.cpp now
// const dvar_t *bg_legYawTolerance            = nullptr;  // provided by bg_misc.cpp now
// const dvar_t *bg_swingSpeed                 = nullptr;  // provided by bg_misc.cpp now
// const dvar_t *player_lean_rotate_crouch_left  = nullptr;  // provided by bg_misc.cpp now
// const dvar_t *player_lean_rotate_crouch_right = nullptr;  // provided by bg_misc.cpp now
// const dvar_t *player_lean_rotate_left         = nullptr;  // provided by bg_misc.cpp now
// const dvar_t *player_lean_rotate_right        = nullptr;  // provided by bg_misc.cpp now
// const dvar_t *player_lean_shift_crouch_left   = nullptr;  // provided by bg_misc.cpp now
// const dvar_t *player_lean_shift_crouch_right  = nullptr;  // provided by bg_misc.cpp now
// const dvar_t *player_lean_shift_left          = nullptr;  // provided by bg_misc.cpp now
// const dvar_t *player_lean_shift_right         = nullptr;  // provided by bg_misc.cpp now
// const dvar_t *player_move_factor_on_torso     = nullptr;  // provided by bg_misc.cpp now
// const dvar_t *player_sprintSpeedScale         = nullptr;  // provided by bg_misc.cpp now
// const dvar_t *xanim_debug                     = nullptr;  // provided by bg_misc.cpp now

// void BG_CheckThread() {}  // provided by bg_misc.cpp now
// double GetLeanFraction(float v) { return (double)v; }  // provided by q_shared.cpp now

// === bg_pmove satellites ===========================================================

// const dvar_t *bg_foliagesnd_fastinterval    = nullptr;  // provided by bg_misc.cpp now
// const dvar_t *bg_foliagesnd_maxspeed        = nullptr;  // provided by bg_misc.cpp now
// const dvar_t *bg_foliagesnd_minspeed        = nullptr;  // provided by bg_misc.cpp now
// const dvar_t *bg_foliagesnd_resetinterval   = nullptr;  // provided by bg_misc.cpp now
// const dvar_t *bg_foliagesnd_slowinterval    = nullptr;  // provided by bg_misc.cpp now
// const dvar_t *bg_ladder_yawcap              = nullptr;  // provided by bg_misc.cpp now
// const dvar_t *friction                      = nullptr;  // provided by bg_misc.cpp now
// const dvar_t *inertiaAngle                  = nullptr;  // provided by bg_misc.cpp now
// const dvar_t *inertiaDebug                  = nullptr;  // provided by bg_misc.cpp now
// const dvar_t *inertiaMax                    = nullptr;  // provided by bg_misc.cpp now
// const dvar_t *player_backSpeedScale         = nullptr;  // provided by bg_misc.cpp now
// const dvar_t *player_dmgtimer_flinchTime    = nullptr;  // provided by bg_misc.cpp now
// const dvar_t *player_dmgtimer_minScale      = nullptr;  // provided by bg_misc.cpp now
// const dvar_t *player_dmgtimer_stumbleTime   = nullptr;  // provided by bg_misc.cpp now
// const dvar_t *player_footstepsThreshhold    = nullptr;  // provided by bg_misc.cpp now
// const dvar_t *player_meleeChargeFriction    = nullptr;  // provided by bg_misc.cpp now
// const dvar_t *player_moveThreshhold         = nullptr;  // provided by bg_misc.cpp now
// const dvar_t *player_spectateSpeedScale     = nullptr;  // provided by bg_misc.cpp now
// const dvar_t *player_sprintCameraBob        = nullptr;  // provided by bg_misc.cpp now
// const dvar_t *player_sprintForwardMinimum   = nullptr;  // provided by bg_misc.cpp now
// const dvar_t *player_sprintMinTime          = nullptr;  // provided by bg_misc.cpp now
// const dvar_t *player_sprintRechargePause    = nullptr;  // provided by bg_misc.cpp now
// const dvar_t *player_sprintStrafeSpeedScale = nullptr;  // provided by bg_misc.cpp now
// const dvar_t *player_strafeAnimCosAngle     = nullptr;  // provided by bg_misc.cpp now
// const dvar_t *player_strafeSpeedScale       = nullptr;  // provided by bg_misc.cpp now
// const dvar_t *player_turnAnims              = nullptr;  // provided by bg_misc.cpp now
// const dvar_t *player_view_pitch_down        = nullptr;  // provided by bg_misc.cpp now
// const dvar_t *player_view_pitch_up          = nullptr;  // provided by bg_misc.cpp now
// const dvar_t *stopspeed                     = nullptr;  // provided by bg_misc.cpp now

// bool   BG_UsingSniperScope(playerState_s *) { return false; }  // provided by bg_weapons.cpp now
// void   DObjSetLocalTag(DObj_s *, int *, unsigned int, const float *, const float *) {}
// float  PitchForYawOnNormal(float, const float *) { return 0.f; }  // provided by com_math.cpp now
// void   PM_AdjustAimSpreadScale(pmove_t *, pml_t *) {}  // provided by bg_weapons.cpp now
// int    PM_InteruptWeaponWithProneMove(playerState_s *) { return 0; }  // provided by bg_weapons.cpp now
// void   PM_ResetWeaponState(playerState_s *) {}  // provided by bg_weapons.cpp now
// void   PM_UpdateAimDownSightFlag(pmove_t *, pml_t *) {}  // provided by bg_weapons.cpp now
// void   PM_UpdateAimDownSightLerp(pmove_t *, pml_t *) {}  // provided by bg_weapons.cpp now
// void   PM_Weapon(pmove_t *, pml_t *) {}  // provided by bg_weapons.cpp now
// int    PM_WeaponAmmoAvailable(playerState_s *) { return 0; }  // provided by bg_weapons.cpp now
// void   ProjectPointOnPlane(const float *, const float *, float *out) { if (out) { out[0] = out[1] = out[2] = 0.f; } }  // provided by com_math.cpp now
void   Sys_SnapVector(float *) {}
// double UnGetLeanFraction(float v) { return (double)v; }  // provided by q_shared.cpp now
// float  Vec2LengthSq(const float *v) { return v ? v[0] * v[0] + v[1] * v[1] : 0.f; }  // provided by com_math.cpp now

// === bg_weapons satellites =========================================================

// const dvar_t *bg_aimSpreadMoveSpeedThreshold = nullptr;  // provided by bg_misc.cpp now
// const dvar_t *bg_bobAmplitudeDucked          = nullptr;  // provided by bg_misc.cpp now
// const dvar_t *bg_bobAmplitudeProne           = nullptr;  // provided by bg_misc.cpp now
// const dvar_t *bg_bobAmplitudeSprinting       = nullptr;  // provided by bg_misc.cpp now
// const dvar_t *bg_bobAmplitudeStanding        = nullptr;  // provided by bg_misc.cpp now
// const dvar_t *player_adsExitDelay            = nullptr;  // provided by bg_misc.cpp now
// const dvar_t *player_breath_fire_delay       = nullptr;  // provided by bg_misc.cpp now
// const dvar_t *player_breath_gasp_lerp        = nullptr;  // provided by bg_misc.cpp now
// const dvar_t *player_breath_gasp_scale       = nullptr;  // provided by bg_misc.cpp now
// const dvar_t *player_breath_gasp_time        = nullptr;  // provided by bg_misc.cpp now
// const dvar_t *player_breath_hold_lerp        = nullptr;  // provided by bg_misc.cpp now
// const dvar_t *player_breath_hold_time        = nullptr;  // provided by bg_misc.cpp now
// const dvar_t *player_burstFireCooldown       = nullptr;  // provided by bg_misc.cpp now
// const dvar_t *player_scopeExitOnDamage       = nullptr;  // provided by bg_misc.cpp now
// const dvar_t *player_sustainAmmo             = nullptr;  // provided by bg_misc.cpp now

// float DiffTrack(float current, float target, float /*rate*/, float /*frametime*/) { return target - current; }  // provided by com_math.cpp now

// === bg_misc satellites ============================================================

int SND_GetEntChannelCount() { return 0; }
// vectopitch provided by src/universal/com_math.cpp now.

// === cg_weapons satellites =========================================================

const dvar_t *heli_barrelRotation = nullptr;

// void DObjClearSkel(const DObj_s *) {}
// char DynEntCl_DynEntImpactEvent(int, int, float *, float *, int, bool) { return 0; }
// void DynEntCl_EntityImpactEvent(const trace_t *, int, int, const float *, const float *, bool) {}
char FX_GetBoneOrientation(int, unsigned int, int, orientation_t *) { return 0; }
void FX_PlayOrientedEffectWithMarkEntity(int, const FxEffectDef *, int, const float *, const float (*)[3], unsigned int) {}
// RotatePointAroundVector provided by src/universal/com_math.cpp now.
char SND_GetKnownLength(int, int *out) { if (out) *out = 0; return 0; }
// UI_DrawWrappedText provided by src/ui/ui_shared.cpp now.

// === cl_devgui satellites ==========================================================

// R_CreateDevGui provided by src/gfx_d3d/r_devgui.cpp now.
// void DevGui_RemoveMenu(const char *) {}
void Com_InitSoundDevGuiGraphs() {}

// === cl_keys satellites ============================================================

// bool con_ignoreMatchPrefixOnly = false;  // provided by cl_console.cpp now
// int32_t con_inputMaxMatchesShown = 0;  // provided by cl_console.cpp now
// const dvar_t *con_matchPrefixOnly = nullptr;  // provided by cl_console.cpp now
// const dvar_t *con_restricted      = nullptr;  // provided by cl_console.cpp now

// void  Con_AllowAutoCompleteCycling(bool) {}  // provided by cl_console.cpp now
// char  Con_AnySpaceAfterCommand() { return 0; }  // provided by cl_console.cpp now
// void  Con_AutoCompleteFromList(const char **, unsigned int, const char *, char *, unsigned int) {}  // provided by cl_console.cpp now
// void  Con_Bottom() {}  // provided by cl_console.cpp now
// char  Con_CancelAutoComplete() { return 0; }  // provided by cl_console.cpp now
// char  Con_CommitToAutoComplete() { return 0; }  // provided by cl_console.cpp now
// char  Con_CycleAutoComplete(int) { return 0; }  // provided by cl_console.cpp now
// bool  Con_HasTooManyMatchesToShow() { return false; }  // provided by cl_console.cpp now
// bool  Con_IsActive(int) { return false; }  // provided by cl_console.cpp now
// bool  Con_IsAutoCompleteMatch(const char *, const char *, int) { return false; }  // provided by cl_console.cpp now
// bool  Con_IsDvarCommand(const char *) { return false; }  // provided by cl_console.cpp now
// void  Con_PageDown() {}  // provided by cl_console.cpp now
// void  Con_PageUp() {}  // provided by cl_console.cpp now
// void  Con_ToggleConsole() {}  // provided by cl_console.cpp now
// void  Con_ToggleConsoleOutput() {}  // provided by cl_console.cpp now
// const char *Con_TokenizeInput() { return ""; }  // provided by cl_console.cpp now
// void  Con_Top() {}  // provided by cl_console.cpp now
// bool  DevGui_KeyPressed(int) { return false; }
// void  ReplaceString(const char **, const char *) {}
void  Scr_AddDebugText(char *) {}
void  Scr_KeyEvent(int) {}
// int   SEH_GetCurrentLanguage() { return 0; }  // provided by stringed_hooks.cpp now
const char *Sys_GetClipboardData() { return nullptr; }
// bool I_isdigit(int c) { return c >= '0' && c <= '9'; }  // provided by q_shared.cpp now

// === cl_console satellites =========================================================

bool IsValidMaterialHandle(Material *m) { return m != nullptr; }
void R_AddCmdDrawConsoleText(char *, int, int, int, Font_s *, float, float, float, float, const float *, int) {}
void R_AddCmdDrawConsoleTextPulseFX(char *, int, int, int, Font_s *, float, float, float, float, const float *, int, const float *, int, int, int, int, Material *, Material *) {}
void R_AddCmdDrawConsoleTextSubtitle(char *, int, int, int, Font_s *, float, float, float, float, const float *, int, const float *) {}
// int  R_ConsoleTextWidth(const char *, int, int, int, Font_s *) { return 0; }
// const char *R_TextLineWrapPosition(const char *s, int, int, Font_s *, float) { return s; }
// unsigned int SEH_DecodeLetter(unsigned int c0, unsigned int /*c1*/, int *used, int * /*flags*/) { if (used) *used = 1; return c0; }  // provided by stringed_hooks.cpp now
// SEH_ReadCharFromString provided by src/stringed/stringed_hooks.cpp now.
int SND_PlayLocalSoundAliasByName(unsigned int, const char *, snd_alias_system_t) { return 0; }
// Vec4Scale provided by src/universal/com_math.cpp now.

// === sv_world satellites ===========================================================

// void CM_ClipMoveToEntities(moveclip_t *, trace_t *) {}
// int  CM_ClipSightTraceToEntities(sightclip_t *) { return 0; }
// void CM_LinkEntity(svEntity_s *, float *, float *, unsigned int) {}
// int  CM_PointSightTraceToEntities(sightpointtrace_t *) { return 0; }
// int  CM_PointTraceStaticModelsComplete(const float *, const float *, int) { return 0; }
// void CM_PointTraceToEntities(pointtrace_t *, trace_t *) {}
// void CM_UnlinkEntity(svEntity_s *) {}
// void DObjTraceline(DObj_s *, float *, float *, unsigned char *, DObjTrace_s *) {}
// void DObjTracelinePartBits(DObj_s *, int *) {}
// RadiusFromBounds2D provided by src/universal/com_math.cpp now.

// === net_chan_mp satellites ========================================================

bool Sys_GetPacket(netadr_t *, msg_t *) { return false; }
bool Sys_SendPacket(int, unsigned char *, netadr_t) { return false; }
bool Sys_StringToAdr(const char *, netadr_t *out) { if (out) std::memset(out, 0, sizeof(*out)); return false; }

// === ui_expressions_logicfunctions satellites ======================================

// GetSourceInt provided by src/ui/ui_expressions.cpp now.

// === ui_expressions satellites =====================================================

int LiveStorage_GetStat(int, int) { return 0; }

// === gfx_d3d satellites ============================================================

DxGlobals dx{};
r_globals_t rg{};
r_global_permanent_t rgp{};
// const dvar_t *r_drawDynEnts = nullptr;  // provided by r_dvars.cpp now
// const dvar_t *r_clear = nullptr;          // provided by r_dvars.cpp now
// const dvar_t *r_clearColor = nullptr;     // provided by r_dvars.cpp now
// const dvar_t *r_clearColor2 = nullptr;    // provided by r_dvars.cpp now
// const dvar_t *developer = nullptr;        // provided by r_dvars.cpp now
#include <gfx_d3d/r_scene.h>
GfxBuffers gfxBuf{};
GfxBackEndData *frontEndDataOut = nullptr;
GfxScene scene{};

struct IDirect3DIndexBuffer9;
struct GfxReadCmdBuf;
int  R_LockIndexBuffer(IDirect3DIndexBuffer9 *, int, int, int) { return 0; }
int  R_ReadPrimDrawSurfData(GfxReadCmdBuf *, unsigned int) { return 0; }
int  R_ReadPrimDrawSurfInt(GfxReadCmdBuf *) { return 0; }
GfxWorld s_world{};
r_globals_load_t rgl{};
// DynEntityPose *DynEnt_GetClientModelPoseList() { return nullptr; }

void Material_UpdatePicmipAll() {}
// void R_Cmd_LoadSun() {}  // provided by r_sky.cpp now
void R_Cmd_ReloadMaterialTextures() {}
// void R_Cmd_SaveSun() {}  // provided by r_sky.cpp now
void R_ImageList_f() {}
void R_MaterialList_f() {}
// void R_ModelList_f() {}
enum GfxScreenshotType : int;
void R_ScreenshotCommand(GfxScreenshotType) {}
void R_StaticModelCacheFlush_f() {}
void R_StaticModelCacheStats_f() {}
void RB_Stats_f() {}
// int DObjGetSurfaces(const DObj_s *, int *, const char *) { return 0; }

struct GfxReflectionProbe;
struct DiskGfxReflectionProbe;
// void R_GenerateReflectionImages(GfxReflectionProbe *, const DiskGfxReflectionProbe *, int, int) {}  // provided by r_reflection_probe.cpp now

struct GfxCmdBufSourceState;
void R_WarnOncePerFrame(GfxWarningType, ...) {}
void R_MatrixIdentity44(float (*m)[4]) {
    if (!m) return;
    for (int i = 0; i < 4; ++i) {
        m[i][0] = (i == 0) ? 1.0f : 0.0f;
        m[i][1] = (i == 1) ? 1.0f : 0.0f;
        m[i][2] = (i == 2) ? 1.0f : 0.0f;
        m[i][3] = (i == 3) ? 1.0f : 0.0f;
    }
}
void R_GetActiveWorldMatrix(GfxCmdBufSourceState *) {}

// === com_files satellites ==========================================================

// Com_GetExtensionSubString provided by src/universal/q_shared.cpp now.
// char *Hunk_CopyString(HunkUser *, const char *s) {
//     if (!s) return nullptr;
//     char *r = static_cast<char *>(std::malloc(std::strlen(s) + 1));
//     if (r) std::strcpy(r, s);
//     return r;
// }
// void *Hunk_UserAlloc(HunkUser *, unsigned int size, int /*align*/) {
//     return std::calloc(size > 0 ? size : 1, 1);
// }
// bool I_islower(int c) { return c >= 'a' && c <= 'z'; }  // provided by q_shared.cpp now
// int  SEH_GetLanguageIndexForName(const char *, int *out) { if (out) *out = 0; return 0; }  // provided by stringed_hooks.cpp now
// const char *SEH_GetLanguageName(unsigned int) { return "english"; }  // provided by stringed_hooks.cpp now
// void SEH_Init_StringEd() {}  // provided by stringed_hooks.cpp now
// void SEH_InitLanguage() {}  // provided by stringed_hooks.cpp now
// void SEH_Shutdown_StringEd() {}  // provided by stringed_hooks.cpp now
int  Sys_CountFileList(char **list) {
    if (!list) return 0;
    int n = 0;
    while (list[n]) ++n;
    return n;
}
const char *Sys_Cwd() { return "."; }
const char *Sys_DefaultCDPath() { return ""; }
char **Sys_ListFiles(const char *, const char *, const char *, int *nFound, int) {
    if (nFound) *nFound = 0;
    return nullptr;
}
void Sys_Mkdir(const char *) {}

// unzip API: not yet ported. Stubs return null/0 so iwd file enumeration
// gracefully skips zip-backed bundles at startup.
// unzClose / unzCloseCurrentFile / unzGetCurrentFileInfo[Position] /
// unzGetGlobalInfo / unzGoToFirstFile / unzGoToNextFile / unzOpen /
// unzOpenCurrentFile / unzReadCurrentFile / unzReOpen /
// unzSetCurrentFileInfoPosition / unztell — provided by unzip.cpp now.

// === scr_variable satellites =======================================================

VariableValue GetEntityFieldValue(unsigned int, int, int) { VariableValue v{}; return v; }
void Scr_CancelNotifyList(unsigned int) {}
const char *Scr_PrevCodePosFileName(char *) { return ""; }
bool Scr_PrevCodePosFileNameMatches(char *, const char *) { return false; }
const char *Scr_PrevCodePosFunctionName(char *) { return ""; }
void Scr_PrintPrevCodePos(int, char *, unsigned int) {}
void Scr_PrintPrevCodePosSpreadSheet(int, char *, bool, bool) {}
void Scr_TerminalError(const char *) {}
char SetEntityFieldValue(unsigned int, int, int, VariableValue *) { return 0; }
// unsigned int SL_ConvertFromString(const char *) { return 0; }  // provided by scr_variable/scr_stringlist now
// unsigned int SL_GetStringForFloat(float) { return 0; }  // provided by scr_variable/scr_stringlist now
// unsigned int SL_GetStringForInt(int) { return 0; }  // provided by scr_variable/scr_stringlist now
// unsigned int SL_GetStringForVector(const float *) { return 0; }  // provided by scr_variable/scr_stringlist now
// int SL_GetStringLen(unsigned int) { return 0; }  // provided by scr_variable/scr_stringlist now
// void TempMemorySetPos(char *) {}
void VM_CancelNotify(unsigned int, unsigned int) {}
// char *Z_TryVirtualAlloc(int size, const char *, int) { return static_cast<char *>(std::calloc(size > 0 ? size : 1, 1)); }

// === g_mover satellites ============================================================

// void VEH_ClearGround() {}  // provided by g_scr_vehicle.cpp now
// bool VEH_SlideMove(gentity_s *, int) { return false; }  // provided by g_scr_vehicle.cpp now

// === ui_shared satellites ==========================================================

// free_expression provided by src/ui/ui_shared_obj.cpp now.
// bool I_isforfilename(int c) { return (c > ' ' && c < 127); }  // provided by q_shared.cpp now
// Item_SetupKeywordHash provided by src/ui/ui_shared_obj.cpp now.
void LiveStorage_ValidateCaCStat(int, int, int) {}
// Menu_FreeItemMemory provided by src/ui/ui_shared_obj.cpp now.
// Menu_SetupKeywordHash provided by src/ui/ui_shared_obj.cpp now.
// UI_LoadMenu_LoadObj provided by src/ui/ui_shared_obj.cpp now.
// UI_LoadMenus_LoadObj provided by src/ui/ui_shared_obj.cpp now.

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
// const dvar_t *player_debugHealth       = nullptr;  // provided by bg_misc.cpp now
const dvar_t *snd_drawEqChannels       = nullptr;
// snd_drawInfo provided by src/cgame_mp/cg_main_mp.cpp now.
// cg_weaponsArray provided by src/cgame_mp/cg_main_mp.cpp now.