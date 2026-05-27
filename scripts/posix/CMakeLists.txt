# === POSIX target ================================================================
# Phase 1 build: produces a `kisak_posix` executable that opens an SDL2
# window with a GL context and runs the gfx_gl renderer. Portable upstream
# subsystems are migrated into UNIVERSAL_PORTABLE_SOURCES as they get
# brought into compilable shape.
#
# See docs/SWITCH_PORT.md for the full Phase 1 roadmap.

message(STATUS "[KisakCOD/posix] Building POSIX skeleton target")

find_package(SDL2 REQUIRED CONFIG)
find_package(OpenGL REQUIRED)
find_package(glm CONFIG REQUIRED)
find_package(ZLIB REQUIRED)

set(POSIX_SOURCES
    "${SRC_DIR}/posix/posix_gl_main.cpp"
    "${SRC_DIR}/posix/posix_assert.cpp"
    "${SRC_DIR}/posix/posix_stubs.cpp"
    "${SRC_DIR}/posix/posix_backbone_stubs.cpp"
    "${SRC_DIR}/gfx_gl/gl_renderer.cpp"
)

# Upstream subsystems brought into the POSIX build. Each file here was
# validated as portable (no Win32/DX/MSS dependencies) and compiles cleanly
# with clang/gcc after the shims in src/posix/kisak_compat.h.
set(UNIVERSAL_PORTABLE_SOURCES
    "${SRC_DIR}/universal/aabbtree.cpp"
    # assertive.cpp uses Win32 clipboard (OpenClipboard/EmptyClipboard/
    # GlobalAlloc), ExitProcess, GetModuleHandleA, FindFirstFile, and
    # intrin.h __debugbreak. Land behind a clipboard/process shim.
    "${SRC_DIR}/universal/base64.cpp"
    "${SRC_DIR}/universal/com_constantconfigstrings.cpp"
    "${SRC_DIR}/universal/com_convexhull.cpp"
    "${SRC_DIR}/universal/com_files.cpp"
    "${SRC_DIR}/universal/com_math.cpp"
    "${SRC_DIR}/universal/com_math_anglevectors.cpp"
    "${SRC_DIR}/universal/com_shared.cpp"
    # com_sndalias.cpp / com_sndalias_load_obj.cpp use hex-rays
    # negative-index struct walking (`g_sa.soundFileInfo[-4]`) plus
    # 32-bit pointer-as-int storage. Land after typed soundalias
    # struct refactor.
    "${SRC_DIR}/universal/pool_allocator.cpp"
    "${SRC_DIR}/universal/q_shared.cpp"
    "${SRC_DIR}/universal/surfaceflags.cpp"
    "${SRC_DIR}/universal/dvar.cpp"
    "${SRC_DIR}/universal/dvar_cmds.cpp"
    "${SRC_DIR}/universal/memfile.cpp"
    "${SRC_DIR}/universal/profile.cpp"
    "${SRC_DIR}/universal/com_loadutils.cpp"
    # com_sndalias[_load_obj] both pull a dozen 32-bit int<->pointer
    # casts, deprecated sprintf, and negative-index artifacts. Lands
    # alongside snd_driver behind the audio abstraction.
    "${SRC_DIR}/universal/com_memory.cpp"
    "${SRC_DIR}/universal/fft.cpp"
    "${SRC_DIR}/universal/com_stringtable.cpp"
    "${SRC_DIR}/universal/q_parse.cpp"
)

set(COMMON_PORTABLE_SOURCES
    "${SRC_DIR}/buildnumber.cpp"
    "${SRC_DIR}/common/brush_edges.cpp"
    # ^ compiles clean on clang. On the Switch GCC target, the
    # -Warray-bounds flagging on hex-rays-style `(*xyz)[3*k]` indexing
    # may need a signature refactor or a localized pragma.
)

set(QCOMMON_PORTABLE_SOURCES
    "${SRC_DIR}/qcommon/cm_load.cpp"
    "${SRC_DIR}/qcommon/cm_load_obj.cpp"
    "${SRC_DIR}/qcommon/cm_mesh.cpp"
    "${SRC_DIR}/qcommon/cm_showcollision.cpp"
    "${SRC_DIR}/qcommon/cm_staticmodel.cpp"
    "${SRC_DIR}/qcommon/cm_test.cpp"
    "${SRC_DIR}/qcommon/cm_trace.cpp"
    "${SRC_DIR}/qcommon/cm_tracebox.cpp"
    "${SRC_DIR}/qcommon/cmd.cpp"
    "${SRC_DIR}/qcommon/com_bsp.cpp"
    "${SRC_DIR}/qcommon/com_bsp_load_obj.cpp"
    "${SRC_DIR}/qcommon/com_fileaccess.cpp"
    "${SRC_DIR}/qcommon/com_pack.cpp"
    "${SRC_DIR}/qcommon/com_playerprofile.cpp"
    "${SRC_DIR}/qcommon/com_profilemapload.cpp"
    "${SRC_DIR}/qcommon/cm_world.cpp"
    "${SRC_DIR}/qcommon/common.cpp"
    "${SRC_DIR}/qcommon/dl_main.cpp"
    "${SRC_DIR}/qcommon/dobj_management.cpp"
    "${SRC_DIR}/qcommon/files.cpp"
    "${SRC_DIR}/qcommon/graph.cpp"
    "${SRC_DIR}/qcommon/huffman.cpp"
    "${SRC_DIR}/qcommon/md4.cpp"
    "${SRC_DIR}/qcommon/mem_track.cpp"
    "${SRC_DIR}/qcommon/msg_mp.cpp"
    "${SRC_DIR}/qcommon/net_chan_mp.cpp"
    "${SRC_DIR}/qcommon/statmonitor.cpp"
    "${SRC_DIR}/qcommon/sv_msg_write_mp.cpp"
    "${SRC_DIR}/qcommon/unzip.cpp"
    # qcommon/threads.cpp deliberately skipped: it's a 934-line Win32-only
    # threading implementation (CreateThread / CreateEvent / WaitForSingleObject
    # everywhere). The single-threaded port uses pthread-backed Sys_* stubs
    # in posix/posix_stubs.cpp instead. See docs/RISKS.md.
)

set(SCRIPT_PORTABLE_SOURCES
    "${SRC_DIR}/script/scr_animtree.cpp"
    # scr_compiler.cpp and scr_compiler2.cpp emit script bytecode that
    # stores `(unsigned int)pos` into 4-byte code slots and later casts
    # them back via `(const char *)(int)stored`. Requires bytecode
    # widening to 64-bit pointer slots before landing.
    "${SRC_DIR}/script/scr_const.cpp"
    # scr_evaluate.cpp depends on scr_debugger / scr_compiler / scr_parser
    # batch (debugger_node*, Scr_CompileStatement, Scr_ClearDebugExpr,
    # scrCompileGlob, scrVmDebugPub). Land together.
    # scr_debugger.cpp stores Scr_WatchElement_s* in 32-bit elementList
    # slots (`elementList[i] = (uint32_t)&newElements[i]`) and uses
    # `*(uint32_t *)(slot + offset)` to re-dereference. Land after
    # 32→64-bit element-list widening.
    "${SRC_DIR}/script/scr_main.cpp"
    "${SRC_DIR}/script/scr_memorytree.cpp"
    "${SRC_DIR}/script/scr_parser.cpp"
    "${SRC_DIR}/script/scr_parsetree.cpp"
    "${SRC_DIR}/script/scr_stringlist.cpp"
    "${SRC_DIR}/script/scr_variable.cpp"
    # scr_yacc2.cpp / scr_yacc.cpp use MSVC `#pragma optimize("", off)`
    # and have Bison short tables with literal 32768 that overflow
    # signed short under clang. Land after pragma + narrowing fix.
)

set(GFX_PORTABLE_SOURCES
    # r_buffers.cpp is hard-tied to IDirect3DDevice9/IDirect3DVertexBuffer9/
    # IDirect3DIndexBuffer9. Land behind a graphics abstraction layer.
    # r_cinematic.cpp pulls Bink (BINKTEXTURESET/BINKTEXTURES/BINKREALTIME)
    # and Miles MSS.H. Land behind an audio/video abstraction layer.
    "${SRC_DIR}/gfx_d3d/r_cmdbuf.cpp"
    "${SRC_DIR}/gfx_d3d/r_debug.cpp"
    "${SRC_DIR}/gfx_d3d/r_debug_alloc.cpp"
    "${SRC_DIR}/gfx_d3d/r_material_override.cpp"
    "${SRC_DIR}/gfx_d3d/r_dobj_skin.cpp"
    # r_image_load_common.cpp needs D3DFMT_* constants — land with the
    # graphics abstraction pass.
    "${SRC_DIR}/gfx_d3d/r_devgui.cpp"
    "${SRC_DIR}/gfx_d3d/r_draw_pixelshader.cpp"
    "${SRC_DIR}/gfx_d3d/r_fog.cpp"
    "${SRC_DIR}/gfx_d3d/r_font.cpp"
    # r_image_load_common.cpp calls
    # IDirect3DVolumeTexture9 / IDirect3DTexture9 / IDirect3DCubeTexture9
    # member functions and uses _D3DLOCKED_BOX / _D3DLOCKED_RECT.
    # Land behind D3D9 abstraction.
    "${SRC_DIR}/gfx_d3d/r_image_utils.cpp"
    # r_image_load_obj.cpp uses D3DCUBEMAP_FACE_* / D3DFMT_G16R16F.
    # Land behind a graphics format/cubemap abstraction.
    "${SRC_DIR}/gfx_d3d/r_image_wavelet.cpp"
    "${SRC_DIR}/gfx_d3d/r_imagedecode.cpp"
    "${SRC_DIR}/gfx_d3d/r_light_load_obj.cpp"
    "${SRC_DIR}/gfx_d3d/r_light.cpp"
    "${SRC_DIR}/gfx_d3d/r_model.cpp"
    "${SRC_DIR}/gfx_d3d/r_cmds.cpp"
    # r_dpvs.cpp drags R_AddDObjSurfaces / R_AddBModelSurfaces /
    # R_AddXModelSurfaces / R_AllocSceneModel / R_AllocModelLighting_*
    # / R_WaitWorkerCmdsOfType / R_ShowLightVisCachePoints — land
    # together with the renderer-frame batch.
    "${SRC_DIR}/gfx_d3d/r_dpvs_dynmodel.cpp"
    "${SRC_DIR}/gfx_d3d/r_dpvs_entity.cpp"
    "${SRC_DIR}/gfx_d3d/r_dpvs_sceneent.cpp"
    "${SRC_DIR}/gfx_d3d/r_dpvs_static.cpp"
    # r_draw_bsp / r_draw_staticmodel / r_draw_xmodel need RB_ShowTess,
    # R_Set{Index,Vertex,Stream,Constant,...} batch — all live in
    # r_rendercmds / r_setstate_d3d. Land alongside the renderer-frame
    # batch.
    "${SRC_DIR}/gfx_d3d/r_draw_method.cpp"
    "${SRC_DIR}/gfx_d3d/r_drawsurf.cpp"
    "${SRC_DIR}/gfx_d3d/r_dvars.cpp"
    "${SRC_DIR}/gfx_d3d/r_buffers.cpp"
    "${SRC_DIR}/gfx_d3d/r_add_bsp.cpp"
    "${SRC_DIR}/gfx_d3d/r_add_cmdbuf.cpp"
    # r_bsp_load_obj.cpp packs pointers into uint32_t slots
    # (anonymousConfig[0] / GfxPortal *, lmap dstPixel pointer math
    # via int casts) and has ~30 pointer-truncating sites. Land
    # after 32→64-bit BSP slot widening.
    "${SRC_DIR}/gfx_d3d/r_add_staticmodel.cpp"
    "${SRC_DIR}/gfx_d3d/r_marks.cpp"
    # r_material.cpp pulls IDirect3DDevice9 / _D3DVERTEXELEMENT9 D3D9
    # types. Land behind graphics abstraction.
    # r_model_lighting.cpp pulls IDirect3DVolumeTexture9 / _D3DLOCKED_BOX
    # / _D3DBOX. Land behind graphics abstraction.
    "${SRC_DIR}/gfx_d3d/r_model_pose.cpp"
    "${SRC_DIR}/gfx_d3d/r_model_skin.cpp"
    "${SRC_DIR}/gfx_d3d/r_outdoor.cpp"
    # r_meshdata.cpp needs ~15 R_Setup* / R_Draw* / R_BeginMaterial /
    # R_LockVertexBuffer satellites; land with the D3D draw batch.
    "${SRC_DIR}/gfx_d3d/r_pretess.cpp"
    "${SRC_DIR}/gfx_d3d/r_reflection_probe.cpp"
    # r_rendertarget.cpp pulls IDirect3D9 + D3DRTYPE_SURFACE +
    # D3DMULTISAMPLE_NONE — D3D9 swap chain / multisample setup.
    # Land behind graphics abstraction.
    # r_scene.cpp pulls r_cinematic.h (Bink BINKTEXTURESET / BINKREALTIME)
    # and IDirect3DQuery9 member access. Land alongside r_cinematic /
    # graphics abstraction.
    # r_screenshot.cpp uses tagMONITORINFO/HMONITOR/ClientToScreen
    # (Win32) and IDirect3DSurface9/IDirect3DSwapChain9 (D3D9). Land
    # behind a screen-capture abstraction.
    # r_shade.cpp pulls D3DLOCK_NOOVERWRITE / DISCARD plus
    # IDirect3DDevice9 member access. Land behind graphics
    # abstraction.
    "${SRC_DIR}/gfx_d3d/r_setstate_d3d.cpp"
    "${SRC_DIR}/gfx_d3d/r_sky.cpp"
    # r_state.cpp uses _D3DVIEWPORT9 (incomplete) and dozens of
    # D3DRS_* constants; needs full D3D9 enum coverage and
    # `&`-in-`|` precedence + past-end matrix indexing pass. Land
    # behind D3D9 abstraction expansion.
    "${SRC_DIR}/gfx_d3d/r_sunshadow.cpp"
    # r_texturemem.cpp uses DirectDraw7 (ddraw.h) for VRAM queries.
    # Land behind a video-memory abstraction.
    # r_spotshadow.cpp drags R_DrawCall / R_DrawSurfs / R_ClearScreen /
    # R_AddSceneDObj / R_DrawQuadMesh / R_SetRenderTarget / Image_Alloc.
    # Land with renderer-frame batch.
    # r_state_utils.cpp has hard D3D9 dependencies past LOD functions —
    # bring back after D3D9 abstraction layer or selective extraction.
    "${SRC_DIR}/gfx_d3d/r_utils.cpp"
    "${SRC_DIR}/gfx_d3d/r_warn.cpp"
    "${SRC_DIR}/gfx_d3d/r_water.cpp"
    "${SRC_DIR}/gfx_d3d/r_water_load_obj.cpp"
    "${SRC_DIR}/gfx_d3d/r_workercmds.cpp"
    "${SRC_DIR}/gfx_d3d/r_workercmds_common.cpp"
    "${SRC_DIR}/gfx_d3d/r_pixelcost_load_obj.cpp"
    "${SRC_DIR}/gfx_d3d/r_primarylights.cpp"
    "${SRC_DIR}/gfx_d3d/r_reflection_probe_load_obj.cpp"
    # r_shadowcookie.cpp needs R_SortDrawSurfs / R_AllocViewParms /
    # R_AddBspDrawSurfs / R_AddDObjSurfaces / R_FrustumClipPlanes /
    # Material_GetTechnique / R_SetupViewProjectionMatrices —
    # land with renderer-mid-frame batch.
    "${SRC_DIR}/gfx_d3d/r_staticmodel.cpp"
    # r_staticmodelcache.cpp has a layout static_assert that fails on
    # 64-bit (smodel leaf grows beyond 256 bytes), 6 past-end int[3]
    # array accesses, and a hex-rays self-compare. Focused pass.
    "${SRC_DIR}/gfx_d3d/r_xsurface.cpp"
    # rb_backend.cpp uses IDirect3DSurface9 member functions (GetDesc /
    # LockRect / UnlockRect / GetGammaRamp) and _D3DLOCKED_RECT /
    # _D3DGAMMARAMP. Land behind D3D9 surface abstraction.
    "${SRC_DIR}/gfx_d3d/rb_debug.cpp"
    "${SRC_DIR}/gfx_d3d/rb_depthprepass.cpp"
    "${SRC_DIR}/gfx_d3d/rb_fog.cpp"
    "${SRC_DIR}/gfx_d3d/rb_imagetouch.cpp"
    "${SRC_DIR}/gfx_d3d/rb_showcollision.cpp"
    "${SRC_DIR}/gfx_d3d/rb_sky.cpp"
    "${SRC_DIR}/gfx_d3d/rb_spotshadow.cpp"
    "${SRC_DIR}/gfx_d3d/rb_sunshadow.cpp"
    "${SRC_DIR}/gfx_d3d/rb_shade.cpp"
    "${SRC_DIR}/gfx_d3d/rb_state.cpp"
    "${SRC_DIR}/gfx_d3d/rb_uploadshaders.cpp"
    "${SRC_DIR}/gfx_d3d/rb_drawprofile.cpp"
    "${SRC_DIR}/gfx_d3d/rb_imagefilter.cpp"
    "${SRC_DIR}/gfx_d3d/rb_light.cpp"
    "${SRC_DIR}/gfx_d3d/rb_logfile.cpp"
    "${SRC_DIR}/gfx_d3d/rb_postfx.cpp"
    "${SRC_DIR}/gfx_d3d/rb_shadowcookie.cpp"
    # rb_tess.cpp drags R_DrawIndexedPrimitive / R_SetStreamSource /
    # R_ChangeIndices / DB_GetIndexBufferAndBase and the whole BSP draw
    # pipeline. Land behind D3D9 abstraction.
    "${SRC_DIR}/gfx_d3d/rb_pixelcost.cpp"
    "${SRC_DIR}/gfx_d3d/rb_stats.cpp"
)

# physics/ode/: 15 of 34 ODE source files compile clean (collision_kernel,
# collision_quadtreespace, ode, mass, matrix, util, rotation, odemath, ...).
# They reference dSpaceCollide2 / dSpaceRemove / etc. from the OTHER 19
# files which have hex-rays-style issues (uninitialized warnings,
# array-bounds, sign-compare). All 34 land together once the harder
# subset gets cleaned. Skipping ODE for this pass.
set(PHYSICS_ODE_SOURCES
    # mat.cpp / mass.cpp / matrix.cpp / odemath.cpp / rotation.cpp /
    # util.cpp / error.cpp / array.cpp depend on dArrayBase, dAlloc,
    # dFree (memory.h and array.h are wrapped in `#if 0` upstream).
    # Land together when ODE memory/array storage is reimplemented.
    # collision_space.cpp / collision_std.cpp pull ODE_AllocateGeom /
    # dxGeom ctor from collision_kernel (gated). Land together.
    "${SRC_DIR}/physics/ode/collision_util.cpp"
    "${SRC_DIR}/physics/ode/error.cpp"
    "${SRC_DIR}/physics/ode/fastdot.c"
    "${SRC_DIR}/physics/ode/fastldlt.c"
    "${SRC_DIR}/physics/ode/fastlsolve.c"
    "${SRC_DIR}/physics/ode/fastltsolve.c"
    # lcp.cpp pulls dStopwatch (gated in timer.h). Land with the
    # broader ODE gated set.
    "${SRC_DIR}/physics/ode/matrix.cpp"
    "${SRC_DIR}/physics/ode/odemath.cpp"
    "${SRC_DIR}/physics/ode/odemisc.cpp"
    "${SRC_DIR}/physics/ode/rotation.cpp"
    # stack.cpp pulls dStack (gated in stack.h). Land with broader
    # ODE gated set.
    "${SRC_DIR}/physics/ode/timer.cpp"
)

set(EFFECTSCORE_PORTABLE_SOURCES
    "${SRC_DIR}/EffectsCore/fx_archive.cpp"
    "${SRC_DIR}/EffectsCore/fx_beam.cpp"
    "${SRC_DIR}/EffectsCore/fx_convert.cpp"
    "${SRC_DIR}/EffectsCore/fx_draw.cpp"
    "${SRC_DIR}/EffectsCore/fx_dvars.cpp"
    "${SRC_DIR}/EffectsCore/fx_graph.cpp"
    "${SRC_DIR}/EffectsCore/fx_marks.cpp"
    # fx_load_obj.cpp packs DWORD-sized pointers into byte buffers for
    # FxEffectDef relocation (effectDef[0..7] is a 32-bit ptr-as-int).
    # Land after typed struct refactor.
    "${SRC_DIR}/EffectsCore/fx_postlight.cpp"
    "${SRC_DIR}/EffectsCore/fx_profile.cpp"
    "${SRC_DIR}/EffectsCore/fx_random.cpp"
    "${SRC_DIR}/EffectsCore/fx_sort.cpp"
    "${SRC_DIR}/EffectsCore/fx_sprite.cpp"
    "${SRC_DIR}/EffectsCore/fx_system.cpp"
    "${SRC_DIR}/EffectsCore/fx_update.cpp"
    "${SRC_DIR}/EffectsCore/fx_update_util.cpp"
    "${SRC_DIR}/EffectsCore/fxcurve.cpp"
)

set(RAGDOLL_PORTABLE_SOURCES
    "${SRC_DIR}/ragdoll/ragdoll.cpp"
    "${SRC_DIR}/ragdoll/ragdoll_controller.cpp"
    "${SRC_DIR}/ragdoll/ragdoll_quat.cpp"
    "${SRC_DIR}/ragdoll/ragdoll_update.cpp"
)

set(DYNENTITY_PORTABLE_SOURCES
    "${SRC_DIR}/DynEntity/DynEntity_client.cpp"
    "${SRC_DIR}/DynEntity/DynEntity_coll.cpp"
    "${SRC_DIR}/DynEntity/DynEntity_load_obj.cpp"
    "${SRC_DIR}/DynEntity/DynEntity_pieces.cpp"
)

set(DATABASE_PORTABLE_SOURCES
    "${SRC_DIR}/database/db_assetnames.cpp"
    "${SRC_DIR}/database/db_auth.cpp"
    # db_file_load.cpp is Win32-only (CloseHandle/SleepEx/GetLastError/
    # GetFileSize / OVERLAPPED I/O). Re-port through Sys_FS abstraction.
    # db_load.cpp drags 60+ Load_*Asset / Mark_*Asset hooks (renderer/
    # font/menu/sound/water/clipmap). Land alongside the rest of the asset
    # subsystem batch.
    "${SRC_DIR}/database/db_memory.cpp"
    # db_registry.cpp calls CreateFileA / CloseHandle / DeleteFileA
    # directly and packs pointers into int slots. Re-port through
    # Sys_FS abstraction.
    "${SRC_DIR}/database/db_stream.cpp"
    "${SRC_DIR}/database/db_stream_load.cpp"
    "${SRC_DIR}/database/db_stringtable_load.cpp"
)

set(DEVGUI_PORTABLE_SOURCES
    "${SRC_DIR}/devgui/devgui.cpp"
    "${SRC_DIR}/devgui/devgui_input.cpp"
    "${SRC_DIR}/devgui/devgui_util.cpp"
)

set(AIMASSIST_PORTABLE_SOURCES
    "${SRC_DIR}/aim_assist/aim_assist.cpp"
    # aim_target.cpp is SP-only.
    "${SRC_DIR}/aim_assist/aim_target_mp.cpp"
)

set(PHYSICS_PORTABLE_SOURCES
    # phys_coll_boxbrush.cpp has ~30 hex-rays issues (past-end float[3]
    # array access on flat normals, _isnan vs isnan, precedence parens,
    # KISAKHACK casts). Land with focused pass.
    "${SRC_DIR}/physics/phys_coll_capsulebrush.cpp"
    "${SRC_DIR}/physics/phys_coll_cylinderbrush.cpp"
    "${SRC_DIR}/physics/phys_contacts.cpp"
    # phys_ode.cpp drags 50+ ODE_*/Phys_* helpers from the gated ODE
    # collision/joint kernel files. Land with the full ODE batch.
    # phys_world_collision.cpp depends on phys_coll_boxbrush and 15
    # ODE collide helpers — land alongside the broader physics batch.
    "${SRC_DIR}/physics/physpreset_load_obj.cpp"
)

set(STRINGED_PORTABLE_SOURCES
    "${SRC_DIR}/stringed/stringed_hooks.cpp"
    "${SRC_DIR}/stringed/stringed_ingame.cpp"
)

set(UI_PORTABLE_SOURCES
    "${SRC_DIR}/ui/ui_atoms.cpp"
    # ui_component.cpp uses SetForegroundWindow (Win32) and has pervasive
    # 32-bit ptr/precedence patterns — land after ifdef pass.
    "${SRC_DIR}/ui/ui_expressions.cpp"
    "${SRC_DIR}/ui/ui_expressions_logicfunctions.cpp"
    "${SRC_DIR}/ui/ui_localvars.cpp"
    "${SRC_DIR}/ui/ui_shared.cpp"
    "${SRC_DIR}/ui/ui_shared_obj.cpp"
    "${SRC_DIR}/ui/ui_utils.cpp"
    "${SRC_DIR}/ui_mp/ui_gameinfo_mp.cpp"
    "${SRC_DIR}/ui_mp/ui_main_mp.cpp"
)

set(SERVER_PORTABLE_SOURCES
    "${SRC_DIR}/server/sv_game.cpp"
    "${SRC_DIR}/server/sv_world.cpp"
    "${SRC_DIR}/server_mp/sv_archive_mp.cpp"
    "${SRC_DIR}/server_mp/sv_ccmds_mp.cpp"
    "${SRC_DIR}/server_mp/sv_client_mp.cpp"
    "${SRC_DIR}/server_mp/sv_init_mp.cpp"
    "${SRC_DIR}/server_mp/sv_main_mp.cpp"
    "${SRC_DIR}/server_mp/sv_snapshot_mp.cpp"
    "${SRC_DIR}/server_mp/sv_snapshot_profile_mp.cpp"
    "${SRC_DIR}/server_mp/sv_voice_mp.cpp"
    "${SRC_DIR}/server_mp/sv_main_pc_mp.cpp"
    "${SRC_DIR}/server_mp/sv_net_chan_mp.cpp"
    "${SRC_DIR}/server_mp/ucmds.cpp"
    # sv_init_mp + sv_snapshot_mp compile clean but each drags 60+
    # sv_*/SV_*Track* dvars/stubs. Land with bigger server batch.
)

set(SOUND_PORTABLE_SOURCES
    # snd.cpp / snd_driver.cpp / snd_mss.cpp / snd_driver_load_obj.cpp
    # all pull Miles Sound System (AIL_*, _AILMIXINFO). Bring back
    # behind an audio abstraction layer.
    "${SRC_DIR}/sound/snd_utils.cpp"
)

set(CGAME_PORTABLE_SOURCES
    # cg_actors.cpp is SP-only (AI subsystem)
    "${SRC_DIR}/cgame/cg_camerashake.cpp"
    "${SRC_DIR}/cgame/cg_colltree.cpp"
    "${SRC_DIR}/cgame/cg_compass.cpp"
    "${SRC_DIR}/cgame/cg_draw_debug.cpp"
    "${SRC_DIR}/cgame/cg_draw_indicators.cpp"
    "${SRC_DIR}/cgame/cg_draw_reticles.cpp"
    "${SRC_DIR}/cgame/cg_drawtools.cpp"
    "${SRC_DIR}/cgame/cg_effects_load_obj.cpp"
    "${SRC_DIR}/cgame/cg_event.cpp"
    "${SRC_DIR}/cgame/cg_hudelem.cpp"
    "${SRC_DIR}/cgame/cg_info.cpp"
    "${SRC_DIR}/cgame/cg_laser.cpp"
    "${SRC_DIR}/cgame/cg_localents.cpp"
    "${SRC_DIR}/cgame/offhandweapons.cpp"
    "${SRC_DIR}/cgame/cg_playerstate.cpp"
    "${SRC_DIR}/cgame/cg_pose_utils.cpp"
    "${SRC_DIR}/cgame/cg_shellshock.cpp"
    "${SRC_DIR}/cgame/cg_ammocounter.cpp"
    "${SRC_DIR}/cgame/cg_visionsets.cpp"
    "${SRC_DIR}/cgame/cg_weapons.cpp"
    "${SRC_DIR}/cgame/cg_world.cpp"
    "${SRC_DIR}/cgame_mp/cg_animtree_mp.cpp"
    "${SRC_DIR}/cgame_mp/cg_client_side_effects_mp.cpp"
    "${SRC_DIR}/cgame_mp/cg_draw_mp.cpp"
    "${SRC_DIR}/cgame_mp/cg_ents_mp.cpp"
    "${SRC_DIR}/cgame_mp/cg_compassfriendlies_mp.cpp"
    "${SRC_DIR}/cgame_mp/cg_consolecmds_mp.cpp"
    "${SRC_DIR}/cgame_mp/cg_draw_net_mp.cpp"
    "${SRC_DIR}/cgame_mp/cg_gamepad.cpp"
    "${SRC_DIR}/cgame_mp/cg_main_mp.cpp"
    "${SRC_DIR}/cgame_mp/cg_newDraw_mp.cpp"
    "${SRC_DIR}/cgame_mp/cg_players_mp.cpp"
    # cg_pose_mp.cpp uses unions to store pointer-as-uint32 (vehicle
    # wheel pose math) and past-end float[6] axis indexing. Land after
    # 32→64-bit pointer-in-union pass.
    "${SRC_DIR}/cgame_mp/cg_vehicles_mp.cpp"
    "${SRC_DIR}/cgame_mp/cg_predict_mp.cpp"
    "${SRC_DIR}/cgame_mp/cg_scoreboard_mp.cpp"
    "${SRC_DIR}/cgame_mp/cg_servercmds_mp.cpp"
    "${SRC_DIR}/cgame_mp/cg_snapshot_mp.cpp"
    "${SRC_DIR}/cgame_mp/cg_view_mp.cpp"
    "${SRC_DIR}/cgame_mp/dedicated_cgame.cpp"
    # cg_servercmds_mp compiles clean but drags 50+ CG_/CL_/FX_/SND_/UI_
    # subsystem stubs (game messages, ambient/music, item registration,
    # menu popups). Land with bigger cgame batch.
    # cg_predict_mp, cg_snapshot_mp, cg_view_mp, cg_players_mp compile
    # clean but each drags 30-60 stubs (cg/cl/UI/SV/FX dependencies).
    # Land later with bgame_mp / bg_pmove batch.
    #
    # cg_ammocounter, cg_effects_load_obj, cg_laser, cg_shellshock —
    # compile clean but each drags a wide swath of dvar globals and
    # BG_* helpers (Ammo/Clip/Shellshock/Hud). Land with bgame batch.
)

set(CLIENT_PORTABLE_SOURCES
    "${SRC_DIR}/client/cl_console.cpp"
    "${SRC_DIR}/client/cl_debugdata.cpp"
    "${SRC_DIR}/client/cl_devgui.cpp"
    "${SRC_DIR}/client/cl_keys.cpp"
    "${SRC_DIR}/client/con_channels.cpp"
    "${SRC_DIR}/client/screen_placement.cpp"
    "${SRC_DIR}/client_mp/cl_cgame_mp.cpp"
    "${SRC_DIR}/client_mp/cl_input.cpp"
    "${SRC_DIR}/client_mp/cl_main_mp.cpp"
    "${SRC_DIR}/client_mp/cl_main_pc_mp.cpp"
    "${SRC_DIR}/client_mp/cl_parse_mp.cpp"
    "${SRC_DIR}/client_mp/cl_net_chan_mp.cpp"
    "${SRC_DIR}/client_mp/cl_pose_mp.cpp"
    "${SRC_DIR}/client_mp/cl_scrn_mp.cpp"
    "${SRC_DIR}/client_mp/cl_ui_mp.cpp"
    "${SRC_DIR}/client_mp/cl_rank.cpp"
    "${SRC_DIR}/client_mp/cl_voice.cpp"
    # cl_input + cl_parse_mp compile clean but drag 20+ cl_* dvars and
    # the MSG_Write*/FS_SV_* stub family. Land with a bigger client batch.
)

set(GAME_PORTABLE_SOURCES
    "${SRC_DIR}/game/bullet.cpp"
    "${SRC_DIR}/game/enthandle.cpp"
    "${SRC_DIR}/game/g_client_fields.cpp"
    "${SRC_DIR}/game/g_bsp.cpp"
    "${SRC_DIR}/game/g_debug.cpp"
    "${SRC_DIR}/game/g_helicopter.cpp"
    "${SRC_DIR}/game/g_hudelem.cpp"
    "${SRC_DIR}/game/g_items.cpp"
    "${SRC_DIR}/game/g_load_utils.cpp"
    # pathnode + pathnode_load_obj are SP-only (AI navigation).
    "${SRC_DIR}/game/g_missile.cpp"
    "${SRC_DIR}/game/g_scr_vehicle.cpp"
    "${SRC_DIR}/game/g_mover.cpp"
    "${SRC_DIR}/game/g_scr_mover.cpp"
    "${SRC_DIR}/game/g_weapon.cpp"
    "${SRC_DIR}/game/g_svcmds.cpp"
    "${SRC_DIR}/game/g_weapon_load_obj.cpp"
    "${SRC_DIR}/game_mp/g_active_mp.cpp"
    "${SRC_DIR}/game_mp/g_client_mp.cpp"
    "${SRC_DIR}/game_mp/g_client_script_cmd_mp.cpp"
    "${SRC_DIR}/game_mp/g_cmds_mp.cpp"
    "${SRC_DIR}/game_mp/g_combat_mp.cpp"
    "${SRC_DIR}/game_mp/g_main_mp.cpp"
    "${SRC_DIR}/game_mp/g_misc_mp.cpp"
    "${SRC_DIR}/game_mp/g_player_corpse_mp.cpp"
    "${SRC_DIR}/game_mp/g_scr_helicopter.cpp"
    "${SRC_DIR}/game_mp/g_scr_main_mp.cpp"
    "${SRC_DIR}/game_mp/g_spawn_mp.cpp"
    "${SRC_DIR}/game_mp/g_trigger_mp.cpp"
    "${SRC_DIR}/game_mp/g_utils_mp.cpp"
    "${SRC_DIR}/game_mp/g_vehicles_mp.cpp"
    "${SRC_DIR}/game_mp/player_use_mp.cpp"
    # g_helicopter, g_hudelem, g_items — compile clean but each drags 20+
    # vehHelicopter / g_drop / pickup dvar globals and BG_* helpers.
    # Land when bgame_mp (bg_pmove + bg_weapons + bg_animation_mp) is ready.
)

set(XANIM_PORTABLE_SOURCES
    "${SRC_DIR}/xanim/dobj_skel.cpp"
    "${SRC_DIR}/xanim/dobj.cpp"
    "${SRC_DIR}/xanim/dobj_utils.cpp"
    "${SRC_DIR}/xanim/xanim.cpp"
    "${SRC_DIR}/xanim/xanim_calc.cpp"
    # xanim_load_obj.cpp has hex-rays scratch dest[char buffer] using
    # 32-bit pointer offsets — needs typed struct refactor before landing.
    # xanim_readwrite.cpp is SP-only (pulls server.h saves)
    "${SRC_DIR}/xanim/xmodel.cpp"
    # xmodel_load_obj.cpp pending until xanim_load_obj refactor.
    "${SRC_DIR}/xanim/xmodel_utils.cpp"
    # xmodel_load_phys_collmap.cpp compiles clean but pulls ODE
    # (dMass*). Brings back once ODE physics is in the build.
)

set(BGAME_PORTABLE_SOURCES
    "${SRC_DIR}/bgame/bg_animation_mp.cpp"
    "${SRC_DIR}/bgame/bg_pmove.cpp"
    "${SRC_DIR}/bgame/bg_weapons.cpp"
    "${SRC_DIR}/bgame/bg_misc.cpp"
    "${SRC_DIR}/bgame/bg_jump.cpp"
    "${SRC_DIR}/bgame/bg_mantle.cpp"
    "${SRC_DIR}/bgame/bg_misctables.cpp"
    "${SRC_DIR}/bgame/bg_perks_mp.cpp"
    "${SRC_DIR}/bgame/bg_slidemove.cpp"
    "${SRC_DIR}/bgame/bg_vehicles_mp.cpp"
    "${SRC_DIR}/bgame/bg_weapons_load_obj.cpp"
)

add_executable(kisak_posix
    ${POSIX_SOURCES}
    ${UNIVERSAL_PORTABLE_SOURCES}
    ${QCOMMON_PORTABLE_SOURCES}
    ${COMMON_PORTABLE_SOURCES}
    ${SCRIPT_PORTABLE_SOURCES}
    ${GFX_PORTABLE_SOURCES}
    ${PHYSICS_ODE_SOURCES}
    ${EFFECTSCORE_PORTABLE_SOURCES}
    ${RAGDOLL_PORTABLE_SOURCES}
    ${DYNENTITY_PORTABLE_SOURCES}
    ${XANIM_PORTABLE_SOURCES}
    ${BGAME_PORTABLE_SOURCES}
    ${DATABASE_PORTABLE_SOURCES}
    ${DEVGUI_PORTABLE_SOURCES}
    ${AIMASSIST_PORTABLE_SOURCES}
    ${UI_PORTABLE_SOURCES}
    ${SERVER_PORTABLE_SOURCES}
    ${SOUND_PORTABLE_SOURCES}
    ${CGAME_PORTABLE_SOURCES}
    ${CLIENT_PORTABLE_SOURCES}
    ${GAME_PORTABLE_SOURCES}
    ${PHYSICS_PORTABLE_SOURCES}
    ${STRINGED_PORTABLE_SOURCES}
)

# ODE source uses `#include "X.h"` for its own headers. Add the directory
# so they resolve.
set_property(SOURCE ${PHYSICS_ODE_SOURCES} APPEND PROPERTY
    INCLUDE_DIRECTORIES "${SRC_DIR}/physics/ode")

target_include_directories(kisak_posix PRIVATE
    "${SRC_DIR}"
    "${SRC_DIR}/universal"
    "${SRC_DIR}/posix/include_shims" # makes <zlib/zlib.h> resolve to system zlib
    "${DEPS_DIR}"                    # makes <ode/ode.h> resolve to deps/ode/ode.h
)

target_link_libraries(kisak_posix PRIVATE
    SDL2::SDL2
    OpenGL::GL
    glm::glm
    ZLIB::ZLIB
)

target_compile_definitions(kisak_posix PRIVATE KISAK_MP)

# Force-include the MSVC compat shim before every source file. Covers the
# __cdecl/__declspec/__int16/... tokens the upstream uses directly.
target_compile_options(kisak_posix PRIVATE
    $<$<COMPILE_LANG_AND_ID:CXX,Clang,AppleClang,GNU>:-Wall>
    # Project-wide policy: zero warnings. Any warning that appears must be
    # fixed at the site (explicit casts for signedness, parentheses for
    # precedence, [[maybe_unused]] for intentionally-unused symbols) rather
    # than suppressed with -Wno-*. -Werror enforces the policy.
    $<$<COMPILE_LANG_AND_ID:CXX,Clang,AppleClang,GNU>:-Werror>
    $<$<COMPILE_LANG_AND_ID:CXX,Clang,AppleClang,GNU>:-include${SRC_DIR}/posix/kisak_compat.h>
)

set_target_properties(kisak_posix PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${BIN_DIR}/posix"
)
