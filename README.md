# Kindlebrew

KPM package repository for jailbroken Kindle devices using hdnext/KPM.

## Add the repository

On the Kindle home screen search bar:

```text
;kpm add-repo https://ve.uy/repo
;kpm update
```

Install games directly by package ID:

```text
;kpm install gambatte-k2
;kpm install wordle
;kpm install chess
;kpm install mines
```

`https://ve.uy/repo` is the stable public entrypoint. It currently resolves to this repository's `manifest.json`, so the backend can move without requiring users to reconfigure KPM.

## Packages

### Gambatte-K2

Game Boy / Game Boy Color / Game Boy Advance emulator frontend by crazy-electron. Kindlebrew packages the upstream GPL-3.0 release for KPM/hdnext; ROMs are not included.

Upstream: https://github.com/crazy-electron/gambatte-k2

### Wordle

KWordle v1.5.0 by crizmo. The upstream repository currently declares no software license, so Kindlebrew does **not** redistribute its source or binaries. The tiny KPM wrapper downloads the official release during installation and verifies the SHA-256 published by GitHub before installing it.

Upstream: https://github.com/crizmo/KWordle

### Chess

GNOME Chess from crazy-electron's GPL-licensed GnomeGames4Kindle v1.1. Includes Stockfish 11 and the upstream armhf build. The KPM wrapper downloads and verifies the pinned upstream release at install time.

Upstream: https://github.com/crazy-electron/GnomeGames4Kindle

### Mines

GNOME Mines / classic Minesweeper from the same GnomeGames4Kindle v1.1 release, using the upstream touch-oriented Kindle shortcut and armhf binary.

Upstream: https://github.com/crazy-electron/GnomeGames4Kindle

## Repository layout

- `manifest.json` — KPM repository manifest.
- `package-src/<package>/` — KPM metadata and lifecycle scripts.
- `packages/<package>/artifacts/` — reproducibly built `.kpkg` artifacts.
- `.github/workflows/` — package builders and validation.
