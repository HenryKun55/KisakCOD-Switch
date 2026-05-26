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
    "${SRC_DIR}/universal/base64.cpp"
    "${SRC_DIR}/universal/com_constantconfigstrings.cpp"
    "${SRC_DIR}/universal/com_convexhull.cpp"
    "${SRC_DIR}/universal/com_math_anglevectors.cpp"
    "${SRC_DIR}/universal/com_shared.cpp"
    "${SRC_DIR}/universal/dvar.cpp"
    "${SRC_DIR}/universal/dvar_cmds.cpp"
    "${SRC_DIR}/universal/memfile.cpp"
    "${SRC_DIR}/universal/profile.cpp"
    "${SRC_DIR}/universal/q_parse.cpp"
)

set(COMMON_PORTABLE_SOURCES
    "${SRC_DIR}/common/brush_edges.cpp"
    # ^ compiles clean on clang. On the Switch GCC target, the
    # -Warray-bounds flagging on hex-rays-style `(*xyz)[3*k]` indexing
    # may need a signature refactor or a localized pragma.
)

set(QCOMMON_PORTABLE_SOURCES
    "${SRC_DIR}/qcommon/cm_load.cpp"
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
    "${SRC_DIR}/qcommon/common.cpp"
    "${SRC_DIR}/qcommon/dl_main.cpp"
    "${SRC_DIR}/qcommon/files.cpp"
    "${SRC_DIR}/qcommon/graph.cpp"
    "${SRC_DIR}/qcommon/huffman.cpp"
    "${SRC_DIR}/qcommon/md4.cpp"
    "${SRC_DIR}/qcommon/mem_track.cpp"
    "${SRC_DIR}/qcommon/msg_mp.cpp"
    "${SRC_DIR}/qcommon/statmonitor.cpp"
    # qcommon/threads.cpp deliberately skipped: it's a 934-line Win32-only
    # threading implementation (CreateThread / CreateEvent / WaitForSingleObject
    # everywhere). The single-threaded port uses pthread-backed Sys_* stubs
    # in posix/posix_stubs.cpp instead. See docs/RISKS.md.
)

set(SCRIPT_PORTABLE_SOURCES
    "${SRC_DIR}/script/scr_animtree.cpp"
    "${SRC_DIR}/script/scr_const.cpp"
    "${SRC_DIR}/script/scr_main.cpp"
    "${SRC_DIR}/script/scr_memorytree.cpp"
)

# physics/ode/: 15 of 34 ODE source files compile clean (collision_kernel,
# collision_quadtreespace, ode, mass, matrix, util, rotation, odemath, ...).
# They reference dSpaceCollide2 / dSpaceRemove / etc. from the OTHER 19
# files which have hex-rays-style issues (uninitialized warnings,
# array-bounds, sign-compare). All 34 land together once the harder
# subset gets cleaned. Skipping ODE for this pass.
set(PHYSICS_ODE_SOURCES)

set(EFFECTSCORE_PORTABLE_SOURCES
    "${SRC_DIR}/EffectsCore/fx_dvars.cpp"
    "${SRC_DIR}/EffectsCore/fx_graph.cpp"
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
    "${SRC_DIR}/DynEntity/DynEntity_coll.cpp"
)

set(DATABASE_PORTABLE_SOURCES
    "${SRC_DIR}/database/db_assetnames.cpp"
    "${SRC_DIR}/database/db_auth.cpp"
    "${SRC_DIR}/database/db_memory.cpp"
    "${SRC_DIR}/database/db_stream.cpp"
    "${SRC_DIR}/database/db_stream_load.cpp"
    "${SRC_DIR}/database/db_stringtable_load.cpp"
)

set(DEVGUI_PORTABLE_SOURCES
    "${SRC_DIR}/devgui/devgui_input.cpp"
    "${SRC_DIR}/devgui/devgui_util.cpp"
)

set(AIMASSIST_PORTABLE_SOURCES
    "${SRC_DIR}/aim_assist/aim_target_mp.cpp"
)

set(PHYSICS_PORTABLE_SOURCES
    "${SRC_DIR}/physics/phys_coll_capsulebrush.cpp"
    "${SRC_DIR}/physics/phys_coll_cylinderbrush.cpp"
    "${SRC_DIR}/physics/phys_contacts.cpp"
    "${SRC_DIR}/physics/physpreset_load_obj.cpp"
)

set(STRINGED_PORTABLE_SOURCES
    "${SRC_DIR}/stringed/stringed_ingame.cpp"
)

set(UI_PORTABLE_SOURCES
    "${SRC_DIR}/ui/ui_atoms.cpp"
    "${SRC_DIR}/ui/ui_localvars.cpp"
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
    "${SRC_DIR}/sound/snd_utils.cpp"
)

set(CGAME_PORTABLE_SOURCES
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
    "${SRC_DIR}/game/g_debug.cpp"
    "${SRC_DIR}/game/g_scr_mover.cpp"
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
    "${SRC_DIR}/xanim/xanim.cpp"
    "${SRC_DIR}/xanim/xmodel.cpp"
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
