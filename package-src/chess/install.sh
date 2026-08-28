#!/bin/sh
set -eu

URL="https://ve.uy/repo/upstream/gnomegames-1.1.zip"
SHA256="3ac019bcca2634d0cc68ca141462eaa26cf357ee1247c0099fea939f74969448"
TMP="/mnt/us/.kindlebrew-gnomegames"
TARGET="/mnt/us/extensions/gnomegames"
DOC="/mnt/us/documents/GnomeChess.sh"


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
if [ ! -f "$SRC/bin/gnomegames.sh" ] || [ ! -f "$SRC/shortcut_gnomechess.sh" ]; then
  echo "Unexpected GnomeGames archive layout."
  rm -rf "$TMP"
  exit 1
fi

rm -rf "$TARGET.kpm-new"
mkdir -p "$TARGET.kpm-new"
cp -R "$SRC"/. "$TARGET.kpm-new/"
chmod 755 "$TARGET.kpm-new/bin/gnomegames.sh" 2>/dev/null || true
chmod 755 "$TARGET.kpm-new/shortcut_gnomechess.sh" 2>/dev/null || true
printf '%s\n' 'managed-by=kindlebrew' > "$TARGET.kpm-new/.kindlebrew-managed"

rm -rf "$TARGET"
mv "$TARGET.kpm-new" "$TARGET"
cp "$TARGET/shortcut_gnomechess.sh" "$DOC"
chmod 755 "$DOC" 2>/dev/null || true

rm -rf "$TMP"
echo "GNOME Chess installed. Open Gnome Chess from the Kindle library or run ;kpm launch chess."
