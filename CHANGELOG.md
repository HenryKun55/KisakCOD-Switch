# Changelog

All notable changes to this fork are documented here.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and the project adheres to [Semantic Versioning](https://semver.org/).

This fork is a porting effort of [KisakCOD](https://github.com/SwagSoftware/KisakCOD)
to the Nintendo Switch (homebrew via devkitPro/libnx). Versions `0.x.y` indicate
pre-playable work; `1.0.0` will be tagged when a single-player session runs on
real hardware.

## [Unreleased]

### Added
- Fork initialized from `SwagSoftware/KisakCOD@master`.
- `port/switch` branch created for porting work.
- Scaffolding: `CHANGELOG.md`, `CONTRIBUTING.md`, `.editorconfig`,
  `.github/workflows/ci.yml`, issue/PR templates.
- `docs/SWITCH_PORT.md` with subsystem map and port phases.
- `KISAK_TARGET` CMake cache variable (`windows|posix|switch`) with
  auto-detection based on the host system / toolchain.
- POSIX skeleton under `src/posix/` with `posix_main.cpp` (placeholder entry
  point) and `README.md` mapping the upstream `src/win32/*.cpp` files that
  will be ported into that folder.
- `scripts/posix/CMakeLists.txt` producing the `bin/posix/kisak_posix`
  executable when `KISAK_TARGET=posix`. First binary of the port that builds
  end-to-end off Windows.
- First upstream source integrated into the POSIX build:
  `src/universal/base64.cpp` (MIT-derived encoder/decoder, no Win32
  dependencies). `posix_main.cpp` calls `b64_encode` as a linkage smoke test.
- `src/posix/kisak_compat.h`: MSVC compatibility shim providing `__cdecl`,
  `__stdcall`, `__fastcall`, `__forceinline`, `__declspec`,
  `__int8/16/32/64`, and `__pragma`. Force-included by the build before every
  source file. Covers the ~3.7k MSVC keyword usages in upstream without
  invasive patches.
- `src/posix/posix_assert.cpp`: stub implementation of `MyAssertHandler`
  (prints to stderr and aborts). Allows linking upstream files that call
  `MyAssertHandler` directly (not via the `iassert` macro).
- `src/posix/posix_stubs.cpp`: provisional stubs for `I_stricmp` (maps to
  POSIX `strcasecmp`), `AxisToQuat` (returns identity quaternion), and
  `Vec2Normalize` (portable implementation). Removable once their owning
  files are properly ported.
- Three new upstream files integrated into the POSIX build:
  `src/universal/com_math_anglevectors.cpp`, `com_convexhull.cpp`,
  `com_constantconfigstrings.cpp`. They compile and link on macOS arm64.
- **First homebrew `.nro` generated**: `scripts/switch/CMakeLists.txt`
  cross-compiles the same skeleton (`posix_main` + stubs + 4 upstream files)
  with devkitA64+libnx, producing `bin/switch/kisak_switch.elf` (2.6 MB,
  ARM64 static-pie) and `bin/switch/kisak_switch.nro` (166 KB,
  `HOMEBREWNRO0` magic). Loadable in Atmosphere CFW or Ryujinx.
- **First rendered pixel**: `src/switch/switch_main.cpp` replaces the
  `consoleInit`-based entry point with a full GLES2 pipeline (EGL +
  mesa-nouveau via libnx). Renders an RGB triangle into the screen
  framebuffer using our own vertex/fragment shaders. NRO grows to ~5.8 MB
  (mesa-nouveau is statically linked). Press `+` to exit.
- **Animated triangle**: vertex shader gains `uniform float u_time` and
  applies a Z-axis 2D rotation; monotonic time via libnx's
  `armGetSystemTick` + `armTicksToNs`. Proves uniforms + transforms + time
  on the graphics pipeline.
- **`src/gfx_gl/`** (new): GLES2 renderer extracted from `switch_main.cpp`
  with `init()` / `set_viewport()` / `render_frame(time)` / `shutdown()`
  API, agnostic to windowing. `switch_main.cpp` now handles only the libnx
  + EGL bootstrap and the main loop.
- **POSIX desktop parity**: `src/posix/posix_gl_main.cpp` (new) uses SDL2
  to create a window + GL 2.1 Compat context and calls the same `gfx_gl`
  module the Switch build uses. `kisak_posix` on macOS arm64 now opens a
  window with the same rotating triangle. Lets us iterate on shaders /
  geometry without round-tripping through Ryujinx. Esc closes the window.
- 3D upgrade: cube with perspective projection, depth test, MVP via GLM.
  Replaces the 2D triangle. Shader uses `uniform mat4 u_mvp`; vertices are
  now `vec3`. Camera at (0,0,3) looking at origin; cube rotates on Y and X.
- `src/qcommon/thread_context.h`: `ThreadContext_t` enum extracted from
  `gfx_d3d/rb_backend.h` so it can be included on POSIX/Switch targets
  without pulling `<d3d9.h>`. `qcommon/threads.h` now uses this header on
  non-Windows code paths.
- `src/universal/q_parse.cpp` brought into the POSIX build — the upstream
  Quake3-derived text parser. Compiles after fixing `qcommon/threads.h` to
  guard `<gfx_d3d/rb_backend.h>` behind `#ifdef _WIN32`. Links against new
  stubs (`Com_Printf`, `Com_PrintError`, `Com_Error`, `Sys_IsMainThread` /
  `IsRenderThread` / `IsDatabaseThread`) in `src/posix/posix_stubs.cpp`.
  Real implementations come when `qcommon/common.cpp` and
  `qcommon/threads.cpp` are ported.
- `_vsnprintf` → `vsnprintf` macro shim in `kisak_compat.h` (MSVC CRT
  underscore prefix not present on POSIX/newlib).

### Fixed
- Strict-aliasing UB in the `BYTEn`/`WORDn`/`DWORDn` (and signed variants)
  macros in `q_shared.h`. Added typedefs with
  `__attribute__((__may_alias__))` on GCC/clang, eliminating the
  `-Wstrict-aliasing` warning that showed up on the Switch (GCC) build and
  the latent UB that could manifest under aggressive optimization.

### Changed
- CI workflow is now **manual-only** (`workflow_dispatch`). Push/PR triggers
  removed while the port is in rapid iteration to avoid email spam.
  Reinstate once the pipeline stabilizes.
- POSIX and Switch builds now use `-Wno-sign-compare`. Upstream source is
  reverse-engineered (hex-rays never propagates signedness), with dozens of
  `int` vs `unsigned int` comparison sites that are cosmetic noise. Will be
  re-enabled per subsystem during signedness audit.
- `src/posix/kisak_compat.h`: also includes `<climits>` (for
  `INT_MIN`/`INT_MAX` used in `DvarLimits`) and `<cstdlib>` plus macros
  `random` → `kisak_random` and `crandom` → `kisak_crandom` (avoids
  collision with POSIX `<stdlib.h>`). `__int8/16/32/64` are now `#define`
  instead of `typedef` — preserves the use of `unsigned __int8` in upstream.
- `src/universal/q_shared.h`: added an `#else` block to the `#ifdef WIN32`
  with POSIX equivalents for `MAC_STATIC`, `CPUSTRING` (detects
  Switch/macOS/Linux), `ID_INLINE`, `BigShort`/`BigLong` (via
  `__builtin_bswap*`), `LittleShort`/`LittleLong`/`LittleFloat` (no-ops on
  little-endian), `PATH_SEP = '/'`.
- `src/qcommon/qcommon.h`: `<xmmintrin.h>` and `<intrin.h>` includes are
  now guarded by architecture (x86 only); `SnapFloatToInt(float/double)`
  gains an ARM64 fallback using `std::lrintf` / `std::lrint` — same
  round-to-nearest-even rounding as `_mm_cvtss_si32`.
- `static_assert(sizeof(X) == N)` in `q_shared.h`, `qcommon.h`, and
  `msg_mp.h` now gated by `UINTPTR_MAX == 0xFFFFFFFFu` (i.e., active only
  on 32-bit builds). On 64-bit the layouts change due to wider pointers —
  the 64-bit port will get its own dedicated phase.
- Root `CMakeLists.txt` refactored to support configuration on non-MSVC
  hosts. MSVC flags (`/MT /O2 /Ot /MP /W3 /Zi /permissive-`) are now
  wrapped in `if(MSVC)`. On `KISAK_TARGET ∈ {posix,switch}` the Windows
  subdirs (`mp/sp/dedi`) are skipped. Upstream Windows build behavior is
  unchanged.

### Notes
- Target toolchain: devkitPro/devkitA64 + libnx + mesa-nouveau (with
  potential migration to deko3d once the game runs and we need more
  performance).
- Intermediate work on macOS/Linux ARM64 before cross-compiling for Switch.
- `cmake -B build-posix -S .` now configures cleanly on macOS arm64
  (`target=posix`), ready for the next Phase 1 steps.

[Unreleased]: https://github.com/HenryKun55/KisakCOD-Switch/compare/v0.0.0...HEAD
