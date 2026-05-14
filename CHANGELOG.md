# Changelog

Todas as mudanças notáveis nesse fork são registradas aqui.

O formato é baseado em [Keep a Changelog](https://keepachangelog.com/pt-BR/1.1.0/),
e o projeto adere a [Semantic Versioning](https://semver.org/lang/pt-BR/).

Esse fork é um esforço de porte do [KisakCOD](https://github.com/SwagSoftware/KisakCOD)
para o Nintendo Switch (homebrew via devkitPro/libnx). Versões `0.x.y` indicam
trabalho pré-jogável; `1.0.0` será marcada quando uma sessão single-player rodar
em hardware real.

## [Unreleased]

### Added
- Fork inicializado a partir de `SwagSoftware/KisakCOD@master`.
- Branch `port/switch` criada para trabalho de porte.
- Scaffolding: `CHANGELOG.md`, `CONTRIBUTING.md`, `.editorconfig`,
  `.github/workflows/ci.yml`, templates de issue/PR.
- `docs/SWITCH_PORT.md` com mapa de subsistemas e fases do porte.
- Variável de cache `KISAK_TARGET` (`windows|posix|switch`) no CMake, com
  auto-detecção por sistema/toolchain.
- Esqueleto POSIX em `src/posix/` com `posix_main.cpp` (entry point placeholder)
  e `README.md` mapeando os arquivos `src/win32/*.cpp` upstream que serão
  portados para essa pasta.
- `scripts/posix/CMakeLists.txt` construindo o executável `bin/posix/kisak_posix`
  quando `KISAK_TARGET=posix`. Primeiro binário do porte que builda end-to-end
  fora do Windows.
- Primeiro arquivo upstream integrado ao build POSIX: `src/universal/base64.cpp`
  (encoder/decoder MIT-derived, sem dependências Win32). `posix_main.cpp` chama
  `b64_encode` como smoke test de linkage.
- `src/posix/kisak_compat.h`: shim de compatibilidade MSVC com `__cdecl`,
  `__stdcall`, `__fastcall`, `__forceinline`, `__declspec`, `__int8/16/32/64`,
  `__pragma`. Force-incluído pelo CMake antes de qualquer source. Cobre os
  ~3.7k usos de keywords MSVC no upstream sem patches invasivos.
- `src/posix/posix_assert.cpp`: implementação stub de `MyAssertHandler`
  (imprime no stderr e aborta). Permite linkar arquivos upstream que chamam
  `MyAssertHandler` diretamente (não via macro `iassert`).
- `src/posix/posix_stubs.cpp`: stubs provisórios para `I_stricmp` (mapeia
  pra `strcasecmp` POSIX), `AxisToQuat` (retorna quaternion identidade) e
  `Vec2Normalize` (implementação portátil). Removíveis quando seus arquivos
  donos forem portados.
- 3 arquivos novos do upstream integrados ao build POSIX:
  `src/universal/com_math_anglevectors.cpp`, `com_convexhull.cpp`,
  `com_constantconfigstrings.cpp`. Compilam e linkam em macOS arm64.
- **Primeiro `.nro` homebrew gerado**: `scripts/switch/CMakeLists.txt`
  cross-compila o mesmo esqueleto (`posix_main` + stubs + 4 arquivos
  upstream) com devkitA64+libnx, gerando `bin/switch/kisak_switch.elf`
  (2.6 MB, ARM64 static-pie) e `bin/switch/kisak_switch.nro` (166 KB,
  magic `HOMEBREWNRO0`). Carregável em Atmosphere CFW ou Ryujinx.
- `src/qcommon/thread_context.h`: enum `ThreadContext_t` extraída de
  `gfx_d3d/rb_backend.h` pra ser incluída em targets POSIX/Switch sem
  arrastar `<d3d9.h>`. `qcommon/threads.h` agora usa esse header em
  caminho não-Windows.

### Changed
- `src/posix/kisak_compat.h`: agora também inclui `<climits>` (para
  `INT_MIN`/`INT_MAX` usados em `DvarLimits`) e `<cstdlib>` + macros
  `random` → `kisak_random` e `crandom` → `kisak_crandom` (evita colisão
  com `<stdlib.h>` POSIX). `__int8/16/32/64` agora são `#define` em vez
  de `typedef` — preserva o uso de `unsigned __int8` no source upstream.
- `src/universal/q_shared.h`: adicionado bloco `#else` no `#ifdef WIN32`
  com equivalentes POSIX para `MAC_STATIC`, `CPUSTRING` (detecta
  Switch/macOS/Linux), `ID_INLINE`, `BigShort`/`BigLong` (via
  `__builtin_bswap*`), `LittleShort`/`LittleLong`/`LittleFloat` (no-ops em
  little-endian), `PATH_SEP = '/'`.
- `src/qcommon/qcommon.h`: includes `<xmmintrin.h>` e `<intrin.h>` agora
  guardados por arquitetura (x86 only); `SnapFloatToInt(float/double)`
  ganha fallback `std::lrintf`/`std::lrint` para ARM64 — mesmo
  arredondamento round-to-nearest-even que `_mm_cvtss_si32`.
- `static_assert(sizeof(X) == N)` em q_shared.h, qcommon.h e msg_mp.h
  agora condicionais a `UINTPTR_MAX == 0xFFFFFFFFu` (i.e., só ativos em
  builds 32-bit). Em 64-bit os layouts mudam por causa de ponteiros
  maiores — porte 64-bit virá em fase própria.

### Changed
- `CMakeLists.txt` raiz refatorado para suportar configuração em hosts não-MSVC.
  Flags MSVC (`/MT /O2 /Ot /MP /W3 /Zi /permissive-`) agora dentro de `if(MSVC)`.
  Em `KISAK_TARGET ∈ {posix,switch}`, os subdirs Windows (`mp/sp/dedi`) são
  ignorados. Não altera o comportamento do build Windows upstream.
- CI workflow `posix-build`: roda em Ubuntu **e** macOS, exige configure +
  build + smoke run de `kisak_posix` (não é mais `continue-on-error`).

### Notes
- Toolchain alvo: devkitPro/devkitA64 + libnx + mesa-nouveau/deko3d.
- Trabalho intermediário em macOS/Linux ARM64 antes de cross-compilar pro Switch.
- `cmake -B build-posix -S .` agora configura limpo no macOS arm64
  (`target=posix`), pronto para os próximos passos da Fase 1.

[Unreleased]: https://github.com/HenryKun55/KisakCOD-Switch/compare/v0.0.0...HEAD
