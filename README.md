# Kindlebrew

KPM package repository for jailbroken Kindle devices using hdnext/KPM.

## Add the repository

On the Kindle home screen search bar:

```text
;kpm add-repo https://ve.uy/repo
;kpm update
```

Then install a package, for example:

```text
;kpm install gambatte-k2
```

`https://ve.uy/repo` is the stable public entrypoint. It currently resolves to this repository's `manifest.json`, so the backend can move without requiring users to reconfigure KPM.

## Packages

### Gambatte-K2

Gambatte-K2 is developed by crazy-electron and licensed under GPL-3.0. Kindlebrew packages the upstream release for KPM/hdnext; ROMs are not included.

Upstream: https://github.com/crazy-electron/gambatte-k2

## Repository layout

- `manifest.json` — KPM repository manifest.
- `package-src/<package>/` — KPM metadata and lifecycle scripts.
- `packages/<package>/artifacts/` — reproducibly built `.kpkg` artifacts.
- `.github/workflows/` — package builders and validation.
