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

SCRIPT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
bash "$SCRIPT_DIR/kpm-validate.sh" "$out"

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
