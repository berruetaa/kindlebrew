#!/bin/sh
set -eu

URL="https://ve.uy/repo/upstream/gnomegames-1.1.zip"
SHA256="3ac019bcca2634d0cc68ca141462eaa26cf357ee1247c0099fea939f74969448"
TMP="/mnt/us/.kindlebrew-gnomegames"
TARGET="/mnt/us/extensions/gnomegames"
DOC="/mnt/us/documents/GnomeMines.sh"
NEW="${TARGET}.kpm-new.$"
OLD="${TARGET}.kpm-old.$"
DOC_NEW="${DOC}.kpm-new.$"


if [ -e "$TARGET" ] && [ ! -f "$TARGET/.kindlebrew-managed" ]; then
  echo "Existing GnomeGames installation is not managed by Kindlebrew; refusing to overwrite it."
  exit 1
fi

command -v curl >/dev/null 2>&1 || { echo "curl is required."; exit 1; }
command -v unzip >/dev/null 2>&1 || { echo "unzip is required."; exit 1; }
command -v sha256sum >/dev/null 2>&1 || { echo "sha256sum is required."; exit 1; }

rm -rf "$TMP" "$NEW" "$OLD"
rm -f "$DOC_NEW"
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

rm -rf "$NEW"
mkdir -p "$NEW"
cp -R "$SRC"/. "$NEW/"
chmod 755 "$NEW/bin/gnomegames.sh" 2>/dev/null || true
chmod 755 "$NEW/shortcut_gnomine.sh" 2>/dev/null || true
printf '%s\n' 'managed-by=kindlebrew' > "$NEW/.kindlebrew-managed"

if ! cp "$NEW/shortcut_gnomine.sh" "$DOC_NEW"; then
  rm -rf "$TMP" "$NEW"
  exit 1
fi
chmod 755 "$DOC_NEW" 2>/dev/null || true

had_old=0
if [ -e "$TARGET" ]; then
  if ! mv "$TARGET" "$OLD"; then
    rm -rf "$TMP" "$NEW"
    rm -f "$DOC_NEW"
    exit 1
  fi
  had_old=1
fi

if ! mv "$NEW" "$TARGET"; then
  [ "$had_old" -eq 0 ] || mv "$OLD" "$TARGET" 2>/dev/null || true
  rm -rf "$TMP" "$NEW"
  rm -f "$DOC_NEW"
  exit 1
fi

if ! mv -f "$DOC_NEW" "$DOC"; then
  rm -rf "$TARGET"
  [ "$had_old" -eq 0 ] || mv "$OLD" "$TARGET" 2>/dev/null || true
  rm -rf "$TMP"
  rm -f "$DOC_NEW"
  exit 1
fi

rm -rf "$OLD" "$TMP" 2>/dev/null || true
echo "GNOME Mines installed. Open Gnome Mines from the Kindle library or run ;kpm launch mines."
