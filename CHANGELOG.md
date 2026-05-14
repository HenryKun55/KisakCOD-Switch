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
