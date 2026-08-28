#!/usr/bin/env bash
set -euo pipefail

# Pack KPM v2 artifacts with the exact upstream helper that KPM documents.
# This deliberately avoids GNU tar/gzip: a previous Kindlebrew build produced
# KPM error 8 (KPM_LIBARCHIVE_ERROR) even though the archive looked valid on
# the build host.
KPM_HELPER_COMMIT="ffa767fffadd731bd59f2bca8c83231f4fc0ab2d"
KPM_HELPER_URL="https://raw.githubusercontent.com/KindleModding/KPM/${KPM_HELPER_COMMIT}/kpm-helper.py"

if [ "$#" -ne 2 ]; then
    echo "usage: $0 <package-directory> <output.kpkg>" >&2
    exit 2
fi

src="$1"
out="$2"

if [ ! -d "$src" ]; then
    echo "package directory does not exist: $src" >&2
    exit 2
fi
if [ ! -f "$src/manifest.json" ]; then
    echo "package is missing manifest.json: $src" >&2
    exit 2
fi

mkdir -p "$(dirname "$out")"
rm -f "$out"

helper="${RUNNER_TEMP:-/tmp}/kindlebrew-kpm-helper-${KPM_HELPER_COMMIT}.py"
if [ ! -f "$helper" ]; then
    curl -fL --retry 3 "$KPM_HELPER_URL" -o "$helper"
fi

python "$helper" package pack "$src" "$out" --compression 5

# Structural checks intentionally mirror assumptions in KPM's libarchive path.
python - "$out" <<'PY'
import json
import pathlib
import sys
import tarfile

path = pathlib.Path(sys.argv[1])
with path.open("rb") as fh:
    assert fh.read(2) == b"\x1f\x8b", "KPM v2 package must be gzip-compressed"

with tarfile.open(path, "r:gz") as archive:
    members = archive.getmembers()
    names = [m.name for m in members]

    assert names, "empty KPM package"
    assert "manifest.json" in names, f"manifest.json must be at archive root: {names}"
    assert len(names) == len(set(names)), "duplicate archive entry"

    for member in members:
        p = pathlib.PurePosixPath(member.name)
        assert not p.is_absolute(), f"absolute archive path: {member.name}"
        assert ".." not in p.parts, f"path traversal in archive: {member.name}"
        assert member.name not in ("rootfs", "startup.sh"), (
            f"reserved KPM package path: {member.name}"
        )
        if member.issym() or member.islnk():
            target = pathlib.PurePosixPath(member.linkname)
            assert not target.is_absolute(), f"absolute link target: {member.name} -> {member.linkname}"
            assert ".." not in target.parts, f"escaping link target: {member.name} -> {member.linkname}"

    manifest = json.load(archive.extractfile("manifest.json"))
    assert manifest["manifest_version"] == 2, manifest
    assert isinstance(manifest["id"], str) and manifest["id"], manifest
    assert isinstance(manifest["version"], list) and len(manifest["version"]) == 3, manifest

print(f"validated KPM structure: {path}")
PY

# KPM 0.2.x extracts .kpkg files with libarchive. CI installs bsdtar from
# libarchive-tools, so this is a second parser independent from Python tarfile.
if ! command -v bsdtar >/dev/null 2>&1; then
    echo "bsdtar is required: install libarchive-tools" >&2
    exit 3
fi
bsdtar -tf "$out" >/dev/null

extract_dir="$(mktemp -d)"
trap 'rm -rf "$extract_dir"' EXIT INT TERM
bsdtar -xf "$out" -C "$extract_dir"
test -f "$extract_dir/manifest.json"
for script in install.sh launch.sh uninstall.sh; do
    if [ -f "$extract_dir/$script" ]; then
        if grep -q "$(printf '\r')" "$extract_dir/$script"; then
            echo "CRLF/CR detected in $script; refusing Kindle package" >&2
            exit 4
        fi
    fi
done
rm -rf "$extract_dir"
trap - EXIT INT TERM

echo "libarchive list+extract OK: $out"
sha256sum "$out"
