// Entry point para targets POSIX (macOS/Linux) e Switch (libnx).
//
// Substitui src/win32/win_main.cpp (WinMain) no porte. Por enquanto e um
// stub vazio que serve so para validar que o pipeline de build cross-platform
// (CMake -> clang/devkitA64) produz um binario funcional.
//
// A medida que os subsistemas portateis (qcommon, common, universal, etc.)
// forem trazidos para o build POSIX, este arquivo vai inicializar o loop
// principal do jogo de forma equivalente ao win_main.cpp upstream.
//
// Ver docs/SWITCH_PORT.md para o estado atual do porte.

#include <cstdio>

int main(int /*argc*/, char ** /*argv*/)
{
    std::printf("KisakCOD-Switch posix bootstrap (Phase 1 skeleton)\n");
    return 0;
}
