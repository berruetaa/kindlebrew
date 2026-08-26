#!/bin/sh
set -eu

URL="https://github.com/crazy-electron/GnomeGames4Kindle/releases/download/v1.1/gnomegames.zip"
SHA256="3ac019bcca2634d0cc68ca141462eaa26cf357ee1247c0099fea939f74969448"
TMP="/mnt/us/.kindlebrew-gnomegames"
TARGET="/mnt/us/extensions/gnomegames"
DOC="/mnt/us/documents/GnomeMines.sh"

if [ "${KPM_PLATFORM:-}" != "kindlehf" ]; then
  echo "GNOME Mines package supports kindlehf only (got: ${KPM_PLATFORM:-unknown})."
  exit 1
fi

if [ -e "$TARGET" ] && [ ! -f "$TARGET/.kindlebrew-managed" ]; then
  echo "Existing GnomeGames installation is not managed by Kindlebrew; refusing to overwrite it."
  exit 1
fi

command -v curl >/dev/null 2>&1 || { echo "curl is required."; exit 1; }
command -v unzip >/dev/null 2>&1 || { echo "unzip is required."; exit 1; }
command -v sha256sum >/dev/null 2>&1 || { echo "sha256sum is required."; exit 1; }

rm -rf "$TMP"
mkdir -p "$TMP" /mnt/us/extensions /mnt/us/documents

curl -fL --retry 3 -o "$TMP/gnomegames.zip" "$URL"
echo "$SHA256  $TMP/gnomegames.zip" | sha256sum -c -
unzip -q "$TMP/gnomegames.zip" -d "$TMP/unpacked"

SRC="$TMP/unpacked/gnomegames"
if [ ! -f "$SRC/bin/gnomegames.sh" ] || [ ! -f "$SRC/shortcut_gnomine.sh" ]; then
  echo "Unexpected GnomeGames archive layout."
  rm -rf "$TMP"
  exit 1
fi

rm -rf "$TARGET.kpm-new"
mkdir -p "$TARGET.kpm-new"
cp -R "$SRC"/. "$TARGET.kpm-new/"
chmod 755 "$TARGET.kpm-new/bin/gnomegames.sh" 2>/dev/null || true
chmod 755 "$TARGET.kpm-new/shortcut_gnomine.sh" 2>/dev/null || true
printf '%s\n' 'managed-by=kindlebrew' > "$TARGET.kpm-new/.kindlebrew-managed"

rm -rf "$TARGET"
mv "$TARGET.kpm-new" "$TARGET"
cp "$TARGET/shortcut_gnomine.sh" "$DOC"
chmod 755 "$DOC" 2>/dev/null || true

rm -rf "$TMP"
echo "GNOME Mines installed. Open Gnome Mines from the Kindle library or run ;kpm launch mines."
