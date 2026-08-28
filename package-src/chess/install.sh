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

if [ ! -f "lib/libGL.so.1" ]; then
  echo "Kindlebrew Chess compatibility shim is missing."
  rm -rf "$TMP"
  exit 1
fi

mkdir -p "$TARGET/lib"
cp "lib/libGL.so.1" "$TARGET/lib/libGL.so.1"
chmod 755 "$TARGET/lib/libGL.so.1" 2>/dev/null || true

cat > "$DOC" <<'EOF'
#!/bin/sh
# DontUseFBInk
TARGET="/mnt/us/extensions/gnomegames"
export LD_LIBRARY_PATH="$TARGET/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
exec sh "$TARGET/bin/gnomegames.sh" glchess
EOF
chmod 755 "$DOC" 2>/dev/null || true

rm -rf "$TMP"
echo "GNOME Chess installed. Open Gnome Chess from the Kindle library or run ;kpm launch chess."
