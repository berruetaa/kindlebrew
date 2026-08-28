#!/usr/bin/env python3
"""Cross-check Kindlebrew's repository index, package sources and KPM artifacts."""
from __future__ import annotations

import json
import pathlib
import sys
import tarfile

ROOT = pathlib.Path(__file__).resolve().parents[1]


def die(message: str) -> "None":
    raise SystemExit(message)


def load_json(path: pathlib.Path) -> dict:
    try:
        return json.loads(path.read_text(encoding="utf-8"))
    except Exception as exc:
        die(f"{path.relative_to(ROOT)}: invalid JSON: {exc}")


def main() -> None:
    repo_path = ROOT / "manifest.json"
    repo = load_json(repo_path)
    if repo.get("manifest_version") != 2:
        die("manifest.json: expected repository manifest_version 2")

    packages = repo.get("packages")
    if not isinstance(packages, dict) or not packages:
        die("manifest.json: packages must be a non-empty object")

    referenced: set[pathlib.Path] = set()

    for package_id, package in sorted(packages.items()):
        if not isinstance(package, dict):
            die(f"{package_id}: repository package entry must be an object")
        artifacts = package.get("artifacts")
        if not isinstance(artifacts, list) or not artifacts:
            die(f"{package_id}: artifacts must be a non-empty array")

        source_manifest_path = ROOT / "package-src" / package_id / "manifest.json"
        source_manifest = load_json(source_manifest_path) if source_manifest_path.is_file() else None
        current_inner: dict | None = None
        current_version: tuple[int, int, int] | None = None

        for artifact in artifacts:
            if not isinstance(artifact, dict):
                die(f"{package_id}: artifact must be an object")
            url = artifact.get("url")
            if not isinstance(url, str) or not url.startswith("repo/"):
                die(f"{package_id}: Kindlebrew artifact URL must be repo-relative: {url!r}")

            rel = pathlib.PurePosixPath(url[len("repo/"):])
            if rel.is_absolute() or ".." in rel.parts:
                die(f"{package_id}: unsafe artifact URL: {url}")
            artifact_path = ROOT.joinpath(*rel.parts)
            if not artifact_path.is_file():
                die(f"{package_id}: referenced artifact missing: {artifact_path.relative_to(ROOT)}")
            referenced.add(artifact_path.resolve())

            try:
                with tarfile.open(artifact_path, "r:gz") as archive:
                    names = set(archive.getnames())
                    if "manifest.json" not in names:
                        die(f"{artifact_path.relative_to(ROOT)}: root manifest.json missing")
                    inner = json.load(archive.extractfile("manifest.json"))
            except SystemExit:
                raise
            except Exception as exc:
                die(f"{artifact_path.relative_to(ROOT)}: cannot read KPM manifest: {exc}")

            if inner.get("manifest_version") != 2:
                die(f"{artifact_path.relative_to(ROOT)}: expected KPM manifest v2")
            if inner.get("id") != package_id:
                die(
                    f"{artifact_path.relative_to(ROOT)}: id {inner.get('id')!r} "
                    f"does not match repository key {package_id!r}"
                )
            if inner.get("version") != artifact.get("version"):
                die(
                    f"{artifact_path.relative_to(ROOT)}: version mismatch: "
                    f"package={inner.get('version')} index={artifact.get('version')}"
                )
            if inner.get("supported_platforms") != artifact.get("supported_platforms"):
                die(
                    f"{artifact_path.relative_to(ROOT)}: platform mismatch: "
                    f"package={inner.get('supported_platforms')} "
                    f"index={artifact.get('supported_platforms')}"
                )
            if inner.get("dependencies", []) != artifact.get("dependencies", []):
                die(f"{artifact_path.relative_to(ROOT)}: dependency list mismatch")

            version = inner.get("version")
            if (
                isinstance(version, list)
                and len(version) == 3
                and all(isinstance(v, int) and not isinstance(v, bool) and v >= 0 for v in version)
            ):
                version_key = tuple(version)
                if current_version is None or version_key > current_version:
                    current_version = version_key
                    current_inner = inner

        if source_manifest is not None:
            if current_inner is None:
                die(f"{package_id}: no valid current artifact version")
            for key in ("id", "version", "supported_platforms", "dependencies"):
                source_value = source_manifest.get(key, [] if key == "dependencies" else None)
                inner_value = current_inner.get(key, [] if key == "dependencies" else None)
                if source_value != inner_value:
                    die(
                        f"{package_id}: package-src manifest {key}={source_value!r} "
                        f"does not match highest artifact {inner_value!r}"
                    )

    committed = {
        p.resolve()
        for p in ROOT.glob("packages/*/artifacts/*.kpkg")
        if p.is_file()
    }
    orphans = sorted(committed - referenced)
    if orphans:
        die(
            "unreferenced KPM artifacts: "
            + ", ".join(str(p.relative_to(ROOT)) for p in orphans)
        )

    missing_files = sorted(referenced - committed)
    if missing_files:
        die(
            "referenced artifacts outside canonical packages/*/artifacts layout: "
            + ", ".join(str(p.relative_to(ROOT)) for p in missing_files)
        )

    print(
        f"repository graph OK: {len(packages)} packages, "
        f"{len(referenced)} referenced artifacts, no orphans"
    )


if __name__ == "__main__":
    main()
