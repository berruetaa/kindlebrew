#!/usr/bin/env bash
set -euo pipefail

# Pack KPM v2 artifacts with the exact upstream helper shipped by Vera/jb.sh (KPM 0.2.2).
# This deliberately avoids GNU tar/gzip: a previous Kindlebrew build produced
# KPM error 8 (KPM_LIBARCHIVE_ERROR) even though the archive looked valid on
# the build host.
KPM_HELPER_COMMIT="799adf431223d2cfa782a6a4ad07d809f120100b"
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

previous=''
if [ -f "$out" ]; then
    previous="$(mktemp)"
    cp "$out" "$previous"
fi
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

if [ -n "$previous" ]; then
    if python - "$previous" "$out" <<'PY'
import hashlib
import sys
import tarfile

def fingerprint(path):
    result = []
    with tarfile.open(path, "r:*") as archive:
        for member in archive.getmembers():
            if member.isfile():
                f = archive.extractfile(member)
                digest = hashlib.sha256(f.read()).hexdigest()
                kind = "file"
                target = ""
            elif member.isdir():
                digest = ""
                kind = "dir"
                target = ""
            elif member.issym():
                digest = ""
                kind = "symlink"
                target = member.linkname
            elif member.islnk():
                digest = ""
                kind = "hardlink"
                target = member.linkname
            else:
                digest = ""
                kind = f"type:{member.type!r}"
                target = member.linkname or ""
            result.append((member.name, kind, member.mode & 0o7777, target, digest))
    return sorted(result)

old, new = sys.argv[1:3]
if fingerprint(old) != fingerprint(new):
    raise SystemExit(1)
print("semantic package content unchanged")
PY
    then
        mv "$previous" "$out"
        previous=''
        echo "preserved existing artifact bytes to avoid timestamp-only churn"
    fi
fi
if [ -n "$previous" ]; then
    rm -f "$previous"
fi

sha256sum "$out"
