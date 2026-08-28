#!/usr/bin/env bash
set -euo pipefail

if [ "$#" -ne 1 ]; then
    echo "usage: $0 <package.kpkg>" >&2
    exit 2
fi

pkg="$1"
if [ ! -f "$pkg" ]; then
    echo "package does not exist: $pkg" >&2
    exit 2
fi

# First parser: Python tarfile, matching the implementation family used by the
# KPM 0.2.2 helper shipped by Vera/jb.sh.
python - "$pkg" <<'PY'
import json
import pathlib
import sys
import tarfile

path = pathlib.Path(sys.argv[1])
with path.open("rb") as fh:
    assert fh.read(2) == b"\x1f\x8b", "KPM manifest-v2 package must be gzip-compressed"

with tarfile.open(path, "r:gz") as archive:
    members = archive.getmembers()
    names = [m.name for m in members]

    assert names, "empty KPM package"
    assert "manifest.json" in names, f"manifest.json must be at archive root: {names}"
    assert len(names) == len(set(names)), "duplicate archive entry"

    # Official KPM helper archives root items by their bare names. Historically
    # Kindlebrew's failing GNU-tar artifacts contained a synthetic ./ root and
    # ./foo members. We deliberately reject that producer shape so it cannot
    # silently return.
    for name in names:
        assert name not in (".", "./"), f"synthetic archive root is forbidden: {name!r}"
        assert not name.startswith("./"), f"non-canonical archive member: {name!r}"

    for member in members:
        p = pathlib.PurePosixPath(member.name)
        assert not p.is_absolute(), f"absolute archive path: {member.name}"
        assert ".." not in p.parts, f"path traversal in archive: {member.name}"
        assert member.name not in ("rootfs", "startup.sh"), (
            f"reserved KPM package path: {member.name}"
        )
        if member.issym() or member.islnk():
            target = pathlib.PurePosixPath(member.linkname)
            assert not target.is_absolute(), (
                f"absolute link target: {member.name} -> {member.linkname}"
            )
            assert ".." not in target.parts, (
                f"escaping link target: {member.name} -> {member.linkname}"
            )

    manifest = json.load(archive.extractfile("manifest.json"))
    assert manifest["manifest_version"] == 2, manifest
    assert isinstance(manifest["id"], str) and manifest["id"], manifest
    assert isinstance(manifest["version"], list) and len(manifest["version"]) == 3, manifest
    assert all(isinstance(v, int) and not isinstance(v, bool) and v >= 0 for v in manifest["version"]), manifest

print(f"python tar validation OK: {path}")
PY

# Second parser: libarchive. KPM 0.2.2 itself embeds libarchive 3.8.1.
BSDTAR_BIN="${BSDTAR:-bsdtar}"
if ! command -v "$BSDTAR_BIN" >/dev/null 2>&1 && [ ! -x "$BSDTAR_BIN" ]; then
    echo "bsdtar is required: install libarchive-tools or set BSDTAR" >&2
    exit 3
fi

"$BSDTAR_BIN" -tf "$pkg" >/dev/null

extract_dir="$(mktemp -d)"
trap 'rm -rf "$extract_dir"' EXIT INT TERM
"$BSDTAR_BIN" -xf "$pkg" -C "$extract_dir"
test -f "$extract_dir/manifest.json"

# KPM executes lifecycle hooks with sh. CRLF is needless ambiguity on the
# Kindle BusyBox shell and is rejected at release time.
for script in install.sh launch.sh uninstall.sh; do
    if [ -f "$extract_dir/$script" ] && grep -q "$(printf '\r')" "$extract_dir/$script"; then
        echo "CRLF/CR detected in $script; refusing Kindle package" >&2
        exit 4
    fi
done

rm -rf "$extract_dir"
trap - EXIT INT TERM

echo "libarchive list+extract OK ($("$BSDTAR_BIN" --version | head -n 1)): $pkg"
