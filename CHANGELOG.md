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

### Notes
- Toolchain alvo: devkitPro/devkitA64 + libnx + mesa-nouveau/deko3d.
- Trabalho intermediário em macOS/Linux ARM64 antes de cross-compilar pro Switch.

[Unreleased]: https://github.com/HenryKun55/KisakCOD-Switch/compare/v0.0.0...HEAD
