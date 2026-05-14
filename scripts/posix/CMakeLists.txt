# === POSIX target =================================================================
# Build minimo da Fase 1: produz um executavel `kisak_posix` que so contem o
# entry point placeholder em src/posix/posix_main.cpp. Conforme subsistemas
# portateis forem migrados para esse target, eles serao adicionados aqui.
#
# Ver docs/SWITCH_PORT.md para o roadmap completo da Fase 1.

message(STATUS "[KisakCOD/posix] Building POSIX skeleton target")

set(POSIX_SOURCES
    "${SRC_DIR}/posix/posix_main.cpp"
    "${SRC_DIR}/posix/posix_assert.cpp"
    "${SRC_DIR}/posix/posix_stubs.cpp"
)

# Primeiros subsistemas upstream trazidos para o build POSIX. Cada arquivo
# aqui foi validado como portavel (sem deps Win32/DX/MSS) e compila limpo
# com clang/gcc apos os shims em src/posix/kisak_compat.h.
set(UNIVERSAL_PORTABLE_SOURCES
    "${SRC_DIR}/universal/base64.cpp"
    "${SRC_DIR}/universal/com_constantconfigstrings.cpp"
    "${SRC_DIR}/universal/com_convexhull.cpp"
    "${SRC_DIR}/universal/com_math_anglevectors.cpp"
)

add_executable(kisak_posix
    ${POSIX_SOURCES}
    ${UNIVERSAL_PORTABLE_SOURCES}
)

target_include_directories(kisak_posix PRIVATE
    "${SRC_DIR}"
    "${SRC_DIR}/universal"
)

# Force-include o shim de compatibilidade MSVC antes de qualquer fonte. Cobre
# __cdecl, __declspec, __int16 e companhia que o codigo upstream usa direto.
target_compile_options(kisak_posix PRIVATE
    $<$<COMPILE_LANG_AND_ID:CXX,Clang,AppleClang,GNU>:-Wall>
    $<$<COMPILE_LANG_AND_ID:CXX,Clang,AppleClang,GNU>:-include${SRC_DIR}/posix/kisak_compat.h>
)

set_target_properties(kisak_posix PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${BIN_DIR}/posix"
)
