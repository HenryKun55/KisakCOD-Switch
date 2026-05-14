# Porte para Nintendo Switch — overview

Este documento descreve o esforço de porte do KisakCOD para o Nintendo Switch
via homebrew (devkitPro + libnx + Atmosphere CFW).

## Estado atual

**Fase 0 — Setup** ✅
- Fork criado, branch `port/switch` ativa
- Toolchain devkitA64/libnx instalada
- Mapa de subsistemas concluído

**Fase 1 — Port intermediário macOS/Linux ARM64** ⏳ (em andamento)
**Fase 2 — Cross-compile para Switch** (futuro)
**Fase 3 — Otimização Switch** (futuro)

## Subsistemas e estratégia

| Subsistema | Pasta upstream | Estratégia Switch |
|---|---|---|
| Renderer DX9 | `src/gfx_d3d/` | Substituir por backend GL/Vulkan (deko3d). Primeiro implementar GL desktop em `src/gfx_gl/`, depois variant Switch. |
| Win32 | `src/win32/` | Stubs POSIX em `src/posix/`; layer libnx em `src/switch/` para HID/socket/storage específicos. |
| Som (Miles) | `src/sound/` | Substituir wrapper Miles por OpenAL-soft (já no devkitPro como `switch-openal-soft`). |
| Vídeo (Bink) | `deps/binklib/` (headers) | Pular cinemáticas inicialmente. FFmpeg disponível como fallback futuro. |
| Steam SDK | `deps/steamsdk/` | Stub completo (no-op). Switch homebrew não tem Steamworks. |
| Física ODE | `src/physics/ode/` + `deps/ode/` | Portável; recompilar p/ ARM64. Sem mudanças no source esperadas. |
| Áudio voz (Speex) | `src/groupvoice/speex` | Portável; recompilar. |
| Engine common | `src/qcommon`, `src/common`, `src/universal` | Auditar assembly inline x86 e SSE intrinsics. Maioria portável. |
| Scripting GSC | `src/script` | Portável; referência: [CoD2rev_Server](https://github.com/voron00/CoD2rev_Server). |
| Game logic | `src/game*`, `src/bgame`, `src/cgame*` | Portável; auditar endianness/alinhamento ARM64. CoD4 é 32-bit, audit casts `ptr↔int`. |
| Animação | `src/xanim` | Auditar SIMD. |
| UI | `src/ui*` | Depende do renderer; portar após GL/Vulkan funcionar. |
| DevGUI | `src/devgui` | Windows-only (ferramenta dev). Pular. |

## Restrições da plataforma

- **CPU**: ARM Cortex-A57 quad-core (Tegra X1), AArch64. Sem SSE/AVX — substituir por NEON ou portable C.
- **GPU**: Nvidia Maxwell GM20B (~1 TFLOPS docked). APIs: deko3d (low-level), Vulkan via deko3d-vk, ou GLES 3.x via mesa-nouveau.
- **RAM**: 4GB total, ~3.2GB usable. CoD4 PC usa ~1.5GB → cabe, margem apertada.
- **Storage**: microSD com latência alta em I/O aleatório — streaming de assets precisa cuidado.
- **Endianness**: little-endian (compatível com x86).
- **Tamanho de ponteiro**: 64-bit (upstream é 32-bit) — auditar todos os casts pointer↔int e structs com sizeof dependente.

## Restrições legais

- **Decompilação** (upstream): trabalho público de reverse engineering sob GPL-3.0. Activision/IW não processou.
- **Assets do CoD4**: cada usuário precisa ser dono de uma cópia. **Nunca commitar** `.iwd`/`.ff`.
- **Homebrew Switch**: legal, mas requer mod de hardware (Atmosphere CFW). Distribuir o `.nro` (sem assets) é OK.
- **GPL-3.0**: forks/distribuições precisam publicar source modificado.

## Como contribuir

Ver [`CONTRIBUTING.md`](../CONTRIBUTING.md).

## Referência: build no Windows (upstream)

Para validar paridade com upstream antes/depois de cada mudança, o build
Windows original ainda funciona seguindo o [README do upstream](../README.md).
Quem só tem macOS/Linux pode usar uma VM Windows + DirectX SDK 2010 + VS 2022.
