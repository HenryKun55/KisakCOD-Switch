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
    "${SRC_DIR}/universal/memfile.cpp"
    "${SRC_DIR}/universal/profile.cpp"
    "${SRC_DIR}/universal/q_parse.cpp"
)

set(QCOMMON_PORTABLE_SOURCES
    "${SRC_DIR}/qcommon/cm_load.cpp"
    "${SRC_DIR}/qcommon/cm_mesh.cpp"
    "${SRC_DIR}/qcommon/cm_showcollision.cpp"
    "${SRC_DIR}/qcommon/cm_staticmodel.cpp"
    "${SRC_DIR}/qcommon/cm_test.cpp"
    "${SRC_DIR}/qcommon/cm_trace.cpp"
    "${SRC_DIR}/qcommon/cm_tracebox.cpp"
    "${SRC_DIR}/qcommon/com_fileaccess.cpp"
    "${SRC_DIR}/qcommon/com_pack.cpp"
    "${SRC_DIR}/qcommon/dl_main.cpp"
    "${SRC_DIR}/qcommon/huffman.cpp"
    "${SRC_DIR}/qcommon/md4.cpp"
)

add_executable(kisak_posix
    ${POSIX_SOURCES}
    ${UNIVERSAL_PORTABLE_SOURCES}
    ${QCOMMON_PORTABLE_SOURCES}
)

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
