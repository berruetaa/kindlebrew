#!/bin/sh
set -eu

URL="https://ve.uy/repo/upstream/kwordle-1.5.0.zip"
SHA256="1755b33c5d0724bacb025ffd44256c7116463e985e82cc45a5f8025c29ad563f"
TMP="/mnt/us/.kindlebrew-wordle"
DOCS="/mnt/us/documents"
TARGET="$DOCS/kwordle"
SCRIPT="$DOCS/kwordle.sh"
MARKER="$TARGET/.kindlebrew-managed"


if { [ -e "$TARGET" ] || [ -e "$SCRIPT" ]; } && [ ! -f "$MARKER" ]; then
  echo "Existing KWordle installation is not managed by Kindlebrew; refusing to overwrite it."
  exit 1
fi

command -v curl >/dev/null 2>&1 || { echo "curl is required."; exit 1; }
command -v unzip >/dev/null 2>&1 || { echo "unzip is required."; exit 1; }
command -v sha256sum >/dev/null 2>&1 || { echo "sha256sum is required."; exit 1; }

rm -rf "$TMP"
mkdir -p "$TMP" "$DOCS"

curl -fL --retry 3 -o "$TMP/kwordle.zip" "$URL"
echo "$SHA256  $TMP/kwordle.zip" | sha256sum -c -
unzip -q "$TMP/kwordle.zip" -d "$TMP/unpacked"

KWORDLE_DIR="$(find "$TMP/unpacked" -type d -name kwordle -print | sed -n '1p')"
KWORDLE_SCRIPT="$(find "$TMP/unpacked" -type f -name kwordle.sh -print | sed -n '1p')"

if [ -z "$KWORDLE_DIR" ] || [ -z "$KWORDLE_SCRIPT" ]; then
  echo "Unexpected KWordle archive layout."
  rm -rf "$TMP"
  exit 1
fi

rm -rf "$TARGET"
cp -R "$KWORDLE_DIR" "$TARGET"
printf '%s\n' 'managed-by=kindlebrew' > "$MARKER"
cp "$KWORDLE_SCRIPT" "$SCRIPT"
chmod 755 "$SCRIPT" 2>/dev/null || true

rm -rf "$TMP"
echo "KWordle installed. Open KWordle from the Kindle library or run ;kpm launch wordle."
