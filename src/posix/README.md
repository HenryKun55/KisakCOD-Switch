# `src/posix/`

Stubs e adapters POSIX que substituem as APIs Win32 usadas pelo `src/win32/`
upstream. Compilado quando `KISAK_TARGET ∈ {posix, switch}` (o target Switch
adiciona uma camada `src/switch/` em cima desse).

## Estado atual

Esqueleto mínimo — apenas `posix_main.cpp` (entry point placeholder).
Subsistemas Win32 a serem portados nessa pasta, em ordem prevista:

| Upstream (`src/win32/`) | Aqui | Status |
|---|---|---|
| `win_main.cpp` | `posix_main.cpp` | placeholder |
| `win_input.cpp` | `posix_input.cpp` | pendente (SDL2/HID) |
| `win_net.cpp` | `posix_net.cpp` | pendente (BSD sockets) |
| `win_storage.cpp` | `posix_storage.cpp` | pendente (XDG / SD card) |
| `win_syscon.cpp` | `posix_syscon.cpp` | pendente (termios) |
| `win_wndproc.cpp` | `posix_wndproc.cpp` | pendente (SDL2 events) |
| `win_voice.cpp` | `posix_voice.cpp` | pendente (OpenAL capture) |
| `win_steam.cpp` | `posix_steam.cpp` | stub no-op (sem Steamworks) |
| `win_localize.cpp` | `posix_localize.cpp` | pendente |
| `win_configure.cpp` | `posix_configure.cpp` | pendente |
| `win_net_debug.cpp` | `posix_net_debug.cpp` | pendente |

Cada porte vira um commit separado seguindo as convenções de
[`CONTRIBUTING.md`](../../CONTRIBUTING.md).
