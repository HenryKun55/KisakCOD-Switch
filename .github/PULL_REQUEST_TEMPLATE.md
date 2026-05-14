<!--
Obrigado por contribuir com o porte! Antes de submeter:
1. Leia CONTRIBUTING.md
2. PR deveria mexer em um subsistema por vez (gfx, sound, net, etc.)
3. Não distribuir assets do CoD4
-->

## Subsistema afetado

<!-- gfx / sound / net / posix / switch / cmake / deps / docs / ci -->

## O que mudou

<!-- Descrição clara. Se substitui API Windows-only, diga qual e por quê. -->

## Como testar

<!-- Comandos exatos. Em qual plataforma. -->

## Fase do porte

- [ ] Fase 1 (macOS/Linux baseline)
- [ ] Fase 2 (cross-compile Switch)
- [ ] Fase 3 (otimização Switch)
- [ ] Independente de fase

## Checklist

- [ ] `CHANGELOG.md` atualizado sob `[Unreleased]` na seção apropriada
- [ ] Não introduz dependência proprietária nova
- [ ] Não quebra build Windows upstream (ou justifica por que)
- [ ] Não inclui assets do CoD4
