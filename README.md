# Kindlebrew

KPM package repository for jailbroken Kindle devices using hdnext/KPM.

## Add the repository

On the Kindle home screen search bar:

```text
;kpm add-repo https://ve.uy/repo
;kpm update
```

Install packages directly by package ID:

```text
;kpm install gambatte-k2
;kpm install wordle
;kpm install chess
;kpm install mines
;kpm install dropbear-ssh
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

### Dropbear SSH

On-demand SSH/SFTP server over the Kindle's existing Wi-Fi connection. Kindlebrew indexes upstream `dropbear-ssh` v0.1.17 directly instead of mirroring its binaries. The package provides a Kindle UI to start/stop the server, shows the connection command and generated password, can keep Wi-Fi reachable while the Kindle would otherwise suspend, and selects the appropriate upstream binary set from the running kernel.

Default SSH port: `2022`.

After installation:

```text
;kpm launch dropbear-ssh
```

Then use the connection command shown by the app, for example:

```text
ssh -p 2022 root@192.168.x.x
```

The package uses a generated root-equivalent master password. Treat it as a root password and only enable the server on networks you trust. Keeping the Kindle reachable while asleep prevents normal suspend and therefore increases battery use.

Upstream: https://github.com/akshaylahudkar/dropbear-ssh

## Repository layout

- `manifest.json` — KPM repository manifest.
- `package-src/<package>/` — KPM metadata and lifecycle scripts maintained by Kindlebrew.
- `packages/<package>/artifacts/` — Kindlebrew-built `.kpkg` artifacts.
- External upstream KPM packages may be indexed by absolute artifact URL instead of being mirrored.
- `.github/workflows/` — package builders and validation.
