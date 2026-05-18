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
    "${SRC_DIR}/qcommon/com_fileaccess.cpp"
    "${SRC_DIR}/qcommon/com_pack.cpp"
    "${SRC_DIR}/qcommon/common.cpp"
    "${SRC_DIR}/qcommon/dl_main.cpp"
    "${SRC_DIR}/qcommon/files.cpp"
    "${SRC_DIR}/qcommon/huffman.cpp"
    "${SRC_DIR}/qcommon/md4.cpp"
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
