# Contribuindo para KisakCOD-Switch

Esse fork porta o [KisakCOD](https://github.com/SwagSoftware/KisakCOD) — uma
decompilação de Call of Duty 4: Modern Warfare — para o Nintendo Switch via
homebrew (devkitPro + libnx + Atmosphere CFW).

## Antes de abrir um PR

1. **Leia [`docs/SWITCH_PORT.md`](docs/SWITCH_PORT.md)** — descreve as 3 fases do
   porte e em qual estamos. Mudanças que pulam fase tendem a quebrar coisas.
2. **Mantenha PRs focados em um subsistema.** O upstream divide em `gfx_d3d`,
   `sound`, `win32`, etc. Cada PR deveria mexer em **um** desses.
3. **Não distribua assets do CoD4.** O fork é só código. `.iwd`/`.ff` ficam no SD
   card do usuário, nunca no repo.
4. **Mantenha compatibilidade com upstream.** Quando possível, fazer mudanças no
   `master` upstream e só fazer port-specific em `port/switch`. Evita drift.

## Convenções de commit

[Conventional Commits](https://www.conventionalcommits.org/) é encorajado, não
obrigatório:

```
<tipo>(<escopo>): <descrição curta>

[corpo opcional]

[footer opcional]
```

Tipos comuns: `feat`, `fix`, `refactor`, `perf`, `build`, `ci`, `docs`, `chore`,
`port`. Escopos: subsistemas (`gfx`, `sound`, `net`, `posix`, `switch`, `cmake`,
`deps`, etc.).

Exemplo:

```
port(gfx): substituir D3DXMatrixIdentity por glm::mat4(1.0f)

A camada D3DX não existe fora do Windows. glm é header-only e já está
disponível via switch-glm no devkitPro.
```

## Sync com upstream

O remote `upstream` aponta para `SwagSoftware/KisakCOD`. Para puxar mudanças:

```bash
git fetch upstream
git checkout master
git merge upstream/master
git push origin master
# em seguida, rebase port/switch sobre o master atualizado:
git checkout port/switch
git rebase master
```

## Estilo de código

- Seguir o estilo do upstream (Allman braces, snake_case para variáveis, etc.).
  Não reformatar arquivos existentes em PRs de feature — abra um PR de
  formatação separado se necessário.
- `.editorconfig` define EOL/indent básicos.

## Changelog

Toda mudança user-visible (não-chore) adiciona uma entrada em `CHANGELOG.md`
sob `[Unreleased]`, seção apropriada (Added/Changed/Fixed/Removed/Security).
Releases promovem `[Unreleased]` para uma seção versionada.

## Licença

Esse projeto é GPL-3.0, herdado do upstream. Qualquer contribuição é
automaticamente licenciada sob os mesmos termos. Você precisa ter direito
sobre o código que contribui.
