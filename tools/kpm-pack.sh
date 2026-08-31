#!/usr/bin/env bash
set -euo pipefail

# The Vera/KPM 0.2.2 helper remains the format authority. Its tar output is
# canonicalized afterward so owner, order and timestamps are reproducible.
KPM_HELPER_COMMIT='799adf431223d2cfa782a6a4ad07d809f120100b'
KPM_HELPER_SHA256='10b6d550accba7f8c7daa28f01ab9f44bd1198cd3b6cf407df4590a8e20c330b'
KPM_HELPER_URL="https://raw.githubusercontent.com/KindleModding/KPM/${KPM_HELPER_COMMIT}/kpm-helper.py"

if [ "$#" -ne 2 ]; then
    echo "usage: $0 <package-directory> <output.kpkg>" >&2
    exit 2
fi

src="$1"
out="$2"
if [ ! -d "$src" ] || [ ! -f "$src/manifest.json" ]; then
    echo "package directory or manifest is missing: $src" >&2
    exit 2
fi

script_dir="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
out_dir="$(dirname -- "$out")"
mkdir -p "$out_dir"
raw="$(mktemp "$out_dir/.kpm-raw.XXXXXX")"
canonical="$(mktemp "$out_dir/.kpm-canonical.XXXXXX")"
manifest_backup="$(mktemp "$out_dir/.kpm-manifest.XXXXXX")"
cp "$src/manifest.json" "$manifest_backup"
cleanup() {
    if [ -f "$manifest_backup" ]; then
        cp "$manifest_backup" "$src/manifest.json"
    fi
    rm -f "$raw" "$canonical" "$manifest_backup"
}
trap cleanup EXIT
trap 'exit 130' INT
trap 'exit 143' TERM

helper="${RUNNER_TEMP:-/tmp}/kindlebrew-kpm-helper-${KPM_HELPER_COMMIT}.py"
if [ -f "$helper" ] &&
   ! printf '%s  %s\n' "$KPM_HELPER_SHA256" "$helper" | sha256sum -c - >/dev/null 2>&1; then
    rm -f "$helper"
fi
if [ ! -f "$helper" ]; then
    curl -fL --retry 3 "$KPM_HELPER_URL" -o "$helper"
fi
printf '%s  %s\n' "$KPM_HELPER_SHA256" "$helper" | sha256sum -c -

manifest_before="$("${PYTHON:-python3}" -c \
    'import json,sys; print(json.dumps(json.load(open(sys.argv[1], encoding="utf-8")), sort_keys=True, separators=(",", ":")))' \
    "$src/manifest.json")"
"${PYTHON:-python3}" "$helper" package pack "$src" "$raw" --compression 5
manifest_after="$("${PYTHON:-python3}" -c \
    'import json,sys; print(json.dumps(json.load(open(sys.argv[1], encoding="utf-8")), sort_keys=True, separators=(",", ":")))' \
    "$src/manifest.json")"
if [ "$manifest_before" != "$manifest_after" ]; then
    echo 'KPM helper unexpectedly changed manifest.json' >&2
    exit 1
fi

"${PYTHON:-python3}" "$script_dir/kpm-canonicalize.py" "$raw" "$canonical"
bash "$script_dir/kpm-validate.sh" "$canonical"

# Same-directory rename is the only operation that replaces a prior artifact.
mv -f "$canonical" "$out"
cleanup
trap - EXIT INT TERM
sha256sum "$out"
