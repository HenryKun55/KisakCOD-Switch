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
)

# Mesmos arquivos validados como portateis no target POSIX.
set(UNIVERSAL_PORTABLE_SOURCES
    "${SRC_DIR}/universal/aabbtree.cpp"
    "${SRC_DIR}/universal/base64.cpp"
    "${SRC_DIR}/universal/com_constantconfigstrings.cpp"
    "${SRC_DIR}/universal/com_convexhull.cpp"
    "${SRC_DIR}/universal/com_math_anglevectors.cpp"
    "${SRC_DIR}/universal/com_shared.cpp"
    "${SRC_DIR}/universal/memfile.cpp"
    "${SRC_DIR}/universal/profile.cpp"
    "${SRC_DIR}/universal/q_parse.cpp"
)

set(QCOMMON_PORTABLE_SOURCES
    "${SRC_DIR}/qcommon/com_pack.cpp"
)

add_executable(kisak_switch
    ${SWITCH_SOURCES}
    ${UNIVERSAL_PORTABLE_SOURCES}
    ${QCOMMON_PORTABLE_SOURCES}
)

target_include_directories(kisak_switch PRIVATE
    "${SRC_DIR}"
    "${SRC_DIR}/universal"
    "${SRC_DIR}/posix/include_shims" # makes <zlib/zlib.h> resolve to switch-zlib
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
