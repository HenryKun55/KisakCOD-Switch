// Entry point para targets POSIX (macOS/Linux) e Switch (libnx).
//
// Substitui src/win32/win_main.cpp (WinMain) no porte. Por enquanto e um
// driver minimal que exercita os primeiros subsistemas upstream trazidos
// para o build POSIX, servindo como smoke test do pipeline cross-platform.
//
// Ver docs/SWITCH_PORT.md para o estado atual do porte.

#include <cstdio>
#include <cstring>

#include "base64.h"

int main(int /*argc*/, char ** /*argv*/)
{
    std::printf("KisakCOD-Switch posix bootstrap (Phase 1 skeleton)\n");

    const char *plain = "KisakCOD";
    unsigned char encoded[64] = {0};
    unsigned int out_len = b64_encode(
        reinterpret_cast<const unsigned char *>(plain),
        static_cast<unsigned int>(std::strlen(plain)),
        encoded);

    std::printf("[smoke] base64('%s') = '%s' (%u bytes)\n",
                plain, encoded, out_len);

    return 0;
}
