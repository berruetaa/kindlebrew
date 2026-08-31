# Kindlebrew

KPM package repository for jailbroken Kindle devices using hdnext/KPM.

## Add the repository

KPM itself supports `add-repo`, but some current Kindle firmware/debug-command stacks reject `.` and `:` in search-bar debug arguments before KPM ever sees them. Do not treat a silent return to Home as a repository/network failure.

If URL arguments work on the device, the normal command is:

```text
;kpm add-repo https://ve.uy/repo
;kpm update
```

On affected devices, install kTerm from the official KindleModding repository first:

```text
;kpm install kterm
;kpm launch kterm
```

Then add Kindlebrew directly from kTerm, bypassing the search-bar debug transport:

```sh
/var/local/kmc/bin/su -c "/var/local/kmc/bin/kpm add-repo https://ve.uy/repo"
/var/local/kmc/bin/su -c "/var/local/kmc/bin/kpm update"
```

Install games directly by package ID:

```text
;kpm install ink2048
;kpm install gambatte-k2
;kpm install wordle
;kpm install chess
;kpm install mines
```

`https://ve.uy/repo` is the stable public entrypoint. It currently resolves to this repository's `manifest.json`, so the backend can move without requiring users to reconfigure KPM.

## Packages

### Ink 2048

Native Kindle 2048 built on the Kindlebrew Game Engine. It uses the engine's event loop, swipe gestures, low-latency interactive refresh policy, debounced grayscale settling, atomic persistence, suspend/resume lifecycle and responsive relayout.

It is also the reference/conformance title for KBGE.

### InkLab

KBGE hardware probe. It exercises framebuffer paths, touch mapping, dirty regions, refresh modes, lifecycle and diagnostics. Compatibility reports should include the generated `Kindlebrew-InkLab-diagnostics.txt`.

### Gambatte-K2

Game Boy / Game Boy Color / Game Boy Advance emulator frontend by crazy-electron. Kindlebrew packages the upstream GPL-3.0 release for KPM/hdnext; ROMs are not included.

Upstream: https://github.com/crazy-electron/gambatte-k2

### Wordle

KWordle v1.5.0 by crizmo. The upstream repository currently declares no software license, so Kindlebrew does **not** redistribute its source or binaries. The tiny KPM wrapper downloads the official release during installation and verifies the SHA-256 published by GitHub before installing it.

Upstream: https://github.com/crizmo/KWordle

### Chess / Ink Chess

Ink Chess 2.0.0 is the native Kindlebrew Game Engine chess title. It bundles Stockfish 18, build-time-rasterized GNOME Chess "simple" pieces, and the required licenses, so installation and first launch do not need Internet access. The previous GNOME Chess 1.1.0 wrapper remains indexed as a legacy downgrade artifact; new installs select Ink Chess 2.0.0.

The package was validated on a Kindle 11th gen (KT6/Rossini/MT8110, firmware 5.19.5) with no GTK, Cairo, librsvg, OpenGL, X11, or libstdc++ runtime dependency. Physical e-ink ghosting and human fingertip input still require visual/device sign-off; framebuffer and synthetic-input evidence are kept outside the repository by the Kindle lab harness.

Source: `package-src/chess/` and `games/inkchess/`

### Mines

GNOME Mines / classic Minesweeper from the same GnomeGames4Kindle v1.1 release, using the upstream touch-oriented Kindle shortcut and armhf binary.

Upstream: https://github.com/crazy-electron/GnomeGames4Kindle

## Kindlebrew Game Engine

`engine/` contains KBGE, a native Gray8/event-driven runtime designed around Kindle e-ink rather than an LCD frame loop. `games/ink2048/` is the reference implementation.

The engine is tested on the host, under ASan/UBSan, and cross-built with the pinned KindleModding `kindlehf` toolchain. Physical behavior is tracked separately in `engine/docs/hardware-matrix.md`; a successful cross-build is intentionally not presented as hardware validation.

## Repository layout

- `manifest.json` — KPM repository manifest.
- `package-src/<package>/` — KPM metadata and lifecycle scripts.
- `packages/<package>/artifacts/` — reproducibly built `.kpkg` artifacts.
- `engine/` — Kindlebrew Game Engine, docs, tests and InkLab.
- `games/` — native games built on KBGE.
- `.github/workflows/` — package builders, tests, sanitizers and cross-build validation.
