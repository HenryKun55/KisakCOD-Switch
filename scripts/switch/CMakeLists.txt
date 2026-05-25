# === Switch target ================================================================
# Cross-compile do esqueleto Fase 1 para Nintendo Switch via devkitA64 + libnx.
# Configure com:
#   cmake -B build-switch -S . \
#         -DCMAKE_TOOLCHAIN_FILE=$DEVKITPRO/cmake/Switch.cmake
#   cmake --build build-switch
# Saida: bin/switch/kisak_switch.nro (homebrew NRO format)
#
# Por ora reusa as mesmas fontes do target POSIX — kisak_compat.h e o
# arquivo posix_main.cpp ja sao agnosticos a Switch (libnx fornece main).
# Ver docs/SWITCH_PORT.md para o roadmap.

message(STATUS "[KisakCOD/switch] Building Switch (devkitA64) skeleton target")

if(NOT NINTENDO_SWITCH)
    message(FATAL_ERROR
        "scripts/switch nao deve ser ativado sem o toolchain Switch. "
        "Use -DCMAKE_TOOLCHAIN_FILE=$ENV{DEVKITPRO}/cmake/Switch.cmake")
endif()

set(SWITCH_SOURCES
    "${SRC_DIR}/switch/switch_main.cpp"
    "${SRC_DIR}/gfx_gl/gl_renderer.cpp"
    "${SRC_DIR}/posix/posix_assert.cpp"
    "${SRC_DIR}/posix/posix_stubs.cpp"
    "${SRC_DIR}/posix/posix_backbone_stubs.cpp"
)

# Same files validated as portable on the POSIX target.
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
    # brush_edges.cpp: compiles clean on clang, but GCC's -Warray-bounds
    # flags hex-rays-style `(*xyz)[3*k]` indexing. Needs signature
    # refactor before it lands on Switch (devkitA64 GCC).
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
    # qcommon/threads.cpp deliberately skipped: 934-line Win32-only threading.
    # The single-threaded port uses pthread-backed Sys_* stubs in
    # posix/posix_stubs.cpp + posix_backbone_stubs.cpp instead.
)

set(SCRIPT_PORTABLE_SOURCES
    "${SRC_DIR}/script/scr_animtree.cpp"
    "${SRC_DIR}/script/scr_const.cpp"
    "${SRC_DIR}/script/scr_main.cpp"
    "${SRC_DIR}/script/scr_memorytree.cpp"
)

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

set(UI_PORTABLE_SOURCES
    "${SRC_DIR}/ui/ui_localvars.cpp"
    "${SRC_DIR}/ui/ui_utils.cpp"
    "${SRC_DIR}/ui_mp/ui_gameinfo_mp.cpp"
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

set(SERVER_PORTABLE_SOURCES
    "${SRC_DIR}/server/sv_game.cpp"
    "${SRC_DIR}/server_mp/sv_archive_mp.cpp"
    "${SRC_DIR}/server_mp/sv_main_pc_mp.cpp"
    "${SRC_DIR}/server_mp/sv_net_chan_mp.cpp"
    "${SRC_DIR}/server_mp/ucmds.cpp"
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
    "${SRC_DIR}/cgame/cg_playerstate.cpp"
    "${SRC_DIR}/cgame/cg_pose_utils.cpp"
    "${SRC_DIR}/cgame/cg_shellshock.cpp"
    "${SRC_DIR}/cgame/cg_visionsets.cpp"
    "${SRC_DIR}/cgame/cg_world.cpp"
    "${SRC_DIR}/cgame/offhandweapons.cpp"
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
)

set(CLIENT_PORTABLE_SOURCES
    "${SRC_DIR}/client/cl_debugdata.cpp"
    "${SRC_DIR}/client/con_channels.cpp"
    "${SRC_DIR}/client/screen_placement.cpp"
    "${SRC_DIR}/client_mp/cl_cgame_mp.cpp"
    "${SRC_DIR}/client_mp/cl_net_chan_mp.cpp"
    "${SRC_DIR}/client_mp/cl_pose_mp.cpp"
    "${SRC_DIR}/client_mp/cl_rank.cpp"
    "${SRC_DIR}/client_mp/cl_voice.cpp"
)

set(GAME_PORTABLE_SOURCES
    "${SRC_DIR}/game/bullet.cpp"
    "${SRC_DIR}/game/enthandle.cpp"
    "${SRC_DIR}/game/g_client_fields.cpp"
    "${SRC_DIR}/game/g_debug.cpp"
    "${SRC_DIR}/game/g_scr_mover.cpp"
    "${SRC_DIR}/game/g_svcmds.cpp"
    "${SRC_DIR}/game/g_weapon_load_obj.cpp"
)

set(XANIM_PORTABLE_SOURCES
    "${SRC_DIR}/xanim/dobj_skel.cpp"
    "${SRC_DIR}/xanim/xanim.cpp"
    "${SRC_DIR}/xanim/xmodel.cpp"
    "${SRC_DIR}/xanim/xmodel_utils.cpp"
)

set(BGAME_PORTABLE_SOURCES
    "${SRC_DIR}/bgame/bg_jump.cpp"
    "${SRC_DIR}/bgame/bg_mantle.cpp"
    "${SRC_DIR}/bgame/bg_misctables.cpp"
    "${SRC_DIR}/bgame/bg_perks_mp.cpp"
    "${SRC_DIR}/bgame/bg_slidemove.cpp"
    "${SRC_DIR}/bgame/bg_vehicles_mp.cpp"
    "${SRC_DIR}/bgame/bg_weapons_load_obj.cpp"
)

add_executable(kisak_switch
    ${SWITCH_SOURCES}
    ${UNIVERSAL_PORTABLE_SOURCES}
    ${QCOMMON_PORTABLE_SOURCES}
    ${COMMON_PORTABLE_SOURCES}
    ${SCRIPT_PORTABLE_SOURCES}
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

target_include_directories(kisak_switch PRIVATE
    "${SRC_DIR}"
    "${SRC_DIR}/universal"
    "${SRC_DIR}/posix/include_shims" # makes <zlib/zlib.h> resolve to switch-zlib
    "${DEPS_DIR}"                    # makes <ode/ode.h> resolve to deps/ode/ode.h
    # portlibs/switch has EGL/GLES2/zlib/etc — the toolchain only adds libnx
    # by default, portlibs has to be explicit.
    "$ENV{DEVKITPRO}/portlibs/switch/include"
)

target_compile_definitions(kisak_switch PRIVATE KISAK_MP)

target_compile_options(kisak_switch PRIVATE
    -Wall
    # Project-wide policy: zero warnings. Any warning must be fixed at the
    # site (explicit casts, parentheses, [[maybe_unused]]) rather than
    # suppressed with -Wno-*. -Werror enforces the policy.
    -Werror
    -include${SRC_DIR}/posix/kisak_compat.h
)

set_target_properties(kisak_switch PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${BIN_DIR}/switch"
)

# Gera o .nro (homebrew NRO) a partir do .elf do target. NACP default e
# icone default sao usados (libnx fornece). Customizar quando o jogo
# estiver jogavel.
# Renderer GLES2 via mesa-nouveau no Switch. Ordem dos -l importa: EGL/GLESv2
# antes de glapi e drm_nouveau pra resolver simbolos no link estatico.
target_link_libraries(kisak_switch PRIVATE
    EGL
    GLESv2
    glapi
    drm_nouveau
    z
)

nx_create_nro(kisak_switch
    OUTPUT  "${BIN_DIR}/switch/kisak_switch.nro"
)
