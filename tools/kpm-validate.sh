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

python3 - "$pkg" <<'PY'
import hashlib
import json
import pathlib
import sys
import tarfile


def require(condition, message):
    if not condition:
        raise SystemExit(message)


path = pathlib.Path(sys.argv[1])
require(path.stat().st_size <= 512 * 1024 * 1024, "compressed package is unreasonably large")
with path.open("rb") as stream:
    require(stream.read(2) == b"\x1f\x8b", "KPM v2 package must be gzip-compressed")

with tarfile.open(path, "r:gz") as archive:
    members = archive.getmembers()
    names = [member.name for member in members]
    require(0 < len(members) <= 1024, "empty package or too many archive entries")
    require(len(names) == len(set(names)), "duplicate archive entry")

    total = 0
    by_name = {}
    metadata_issues = []
    for member in members:
        name = member.name
        require(name not in (".", "./") and not name.startswith("./"),
                f"non-canonical archive member: {name!r}")
        require("\\" not in name and "\x00" not in name,
                f"invalid archive member name: {name!r}")
        pure = pathlib.PurePosixPath(name)
        require(not pure.is_absolute() and ".." not in pure.parts and str(pure) == name,
                f"unsafe archive path: {name!r}")
        require(name not in ("rootfs", "startup.sh"), f"reserved KPM path: {name}")
        require(member.isfile() or member.isdir(), f"special archive entry forbidden: {name}")
        if member.uid != 0 or member.gid != 0 or member.uname or member.gname:
            metadata_issues.append(f"ownership:{name}")
        if member.mtime != 0:
            metadata_issues.append(f"mtime:{name}")
        if member.isfile():
            require(member.size <= 256 * 1024 * 1024, f"archive member too large: {name}")
            total += member.size
        by_name[name] = member
    require(total <= 512 * 1024 * 1024, "uncompressed package is unreasonably large")
    require("manifest.json" in by_name and by_name["manifest.json"].isfile(),
            "manifest.json must be a regular file at archive root")

    manifest_stream = archive.extractfile(by_name["manifest.json"])
    require(manifest_stream is not None, "manifest.json is unreadable")
    manifest = json.load(manifest_stream)
    require(manifest.get("manifest_version") == 2, "expected manifest_version 2")
    package_id = manifest.get("id")
    require(isinstance(package_id, str) and package_id, "manifest id is invalid")
    version = manifest.get("version")
    require(isinstance(version, list) and len(version) == 3 and
            all(isinstance(value, int) and not isinstance(value, bool) and value >= 0
                for value in version), "manifest version is invalid")
    platforms = manifest.get("supported_platforms")
    require(isinstance(platforms, list) and platforms and
            all(isinstance(value, str) and value for value in platforms),
            "supported_platforms is invalid")
    require(isinstance(manifest.get("dependencies", []), list), "dependencies is invalid")

    if package_id == "chess" and version[0] >= 2:
        require(not metadata_issues,
                f"Ink Chess package metadata is not reproducible: {metadata_issues[:8]}")
        required = {
            "manifest.json", "library.json", "install.sh", "launch.sh", "uninstall.sh",
            "scriptlet.sh", "library-install.sh", "LICENSE", "SOURCE.txt",
            "CHESS-LIBRARY-LICENSE.txt", "GNOME-PIECES-LICENSE.html",
            "STOCKFISH-LICENSE.txt", "payload/inkchess", "payload/stockfish",
        }
        missing = sorted(required - set(names))
        require(not missing, f"Ink Chess package entries missing: {missing}")
        require(not any(name.lower().endswith(".svg") for name in names),
                "runtime package must not contain SVG files")

        expected_assets = {
            "blackBishop.r8a8": "67d15e45b60f15227449f4682c38875c141fba55bbfcdf3ec6c9aaf4e8400103",
            "blackKing.r8a8": "03fe57a3b42accd77d344052bd646cf17072de0bdb10756db95173e98b2ca328",
            "blackKnight.r8a8": "cc8a13324770622c86d124cfe7d4b5179182ca9aa2f2c41eacb07ac7c1bff783",
            "blackPawn.r8a8": "5fbd08a524c9d3c7a0d598f6c7141b79dd908e5cca60fd54ad81a1aaee34eddc",
            "blackQueen.r8a8": "7c5085c3e757720c4a4fab7226494495aced4ae865ddfebbaf24b3342b775ee2",
            "blackRook.r8a8": "75eff39f058dd1f6d13082ffe50e3389634c1ed0da603d7ec2bcaff93c6c7461",
            "whiteBishop.r8a8": "9578151e16b3989fe0595bd6de29c00a65f9245dce83740d404633738bf284ae",
            "whiteKing.r8a8": "a3ad561da3084df8d873b1d966c5420db700e5c2909142573f4f637761011469",
            "whiteKnight.r8a8": "2a571c95e282aef56ef54a4b1cd767941ce79607f240146c8db9c14a46906e15",
            "whitePawn.r8a8": "a5733efd188e4654e5f652c3136f5b7fef482f59f9e06bbb322773cc0e28b8a6",
            "whiteQueen.r8a8": "9cfa80a6b38d1b655fa027e9d2abefcbdaa671c1eef13633a8821adbfacec963",
            "whiteRook.r8a8": "7a0c6881038f549758c541e2ff2a8f2533d40e2c1fb012dda133077b35c8761e",
        }
        asset_names = {
            name.removeprefix("payload/assets/")
            for name in names
            if name.startswith("payload/assets/") and name.endswith(".r8a8")
        }
        require(asset_names == set(expected_assets),
                f"Ink Chess asset set mismatch: {sorted(asset_names)}")
        for filename, expected_hash in expected_assets.items():
            member = by_name[f"payload/assets/{filename}"]
            require(member.isfile() and member.size == 32768,
                    f"invalid Ink Chess asset size: {filename}")
            payload = archive.extractfile(member)
            require(payload is not None, f"unreadable Ink Chess asset: {filename}")
            require(hashlib.sha256(payload.read()).hexdigest() == expected_hash,
                    f"Ink Chess asset checksum mismatch: {filename}")

        for executable in ("install.sh", "launch.sh", "uninstall.sh", "scriptlet.sh",
                           "library-install.sh", "payload/inkchess", "payload/stockfish"):
            require(by_name[executable].mode & 0o111,
                    f"required executable bit missing: {executable}")

print(f"python tar validation OK: {path}")
PY

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

for script in install.sh launch.sh uninstall.sh scriptlet.sh library-install.sh; do
    if [ -f "$extract_dir/$script" ]; then
        if grep -q "$(printf '\r')" "$extract_dir/$script"; then
            echo "CRLF/CR detected in $script; refusing Kindle package" >&2
            exit 4
        fi
        sh -n "$extract_dir/$script"
    fi
done

rm -rf "$extract_dir"
trap - EXIT INT TERM
echo "libarchive list+extract OK ($("$BSDTAR_BIN" --version | head -n 1)): $pkg"
