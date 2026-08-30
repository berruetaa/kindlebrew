#!/bin/sh
set -eu

TARGET="/mnt/us/extensions/kindlebrew-chess"
NEW="${TARGET}.kpm-new.$$"
OLD="${TARGET}.kpm-old.$$"

for required in payload/inkchess payload/stockfish library-install.sh scriptlet.sh; do
  if [ ! -f "$required" ]; then
    echo "Ink Chess package is incomplete: missing $required"
    exit 1
  fi
done

if [ ! -d "payload/assets" ]; then
  echo "Ink Chess piece assets are missing."
  exit 1
fi

count=0
for f in payload/assets/*.r8a8; do
  [ -f "$f" ] || continue
  [ "$(wc -c < "$f")" -eq 32768 ] || {
    echo "Invalid Ink Chess piece asset: $f"
    exit 1
  }
  count=$((count + 1))
done
[ "$count" -eq 12 ] || {
  echo "Expected 12 Ink Chess piece assets, found $count."
  exit 1
}

sh -n "library-install.sh"
sh -n "scriptlet.sh"

rm -rf "$NEW" "$OLD"
mkdir -p "$NEW/assets"
cp "payload/inkchess" "$NEW/inkchess"
cp "payload/stockfish" "$NEW/stockfish"
cp payload/assets/*.r8a8 "$NEW/assets/"
chmod 755 "$NEW/inkchess" "$NEW/stockfish" 2>/dev/null || true

had_old=0
if [ -e "$TARGET" ]; then
  mv "$TARGET" "$OLD"
  had_old=1
fi

if ! mv "$NEW" "$TARGET"; then
  [ "$had_old" -eq 0 ] || mv "$OLD" "$TARGET" 2>/dev/null || true
  exit 1
fi

if ! sh "library-install.sh" install; then
  rm -rf "$TARGET"
  if [ "$had_old" -ne 0 ]; then
    mv "$OLD" "$TARGET" 2>/dev/null || true
  fi
  exit 1
fi

rm -rf "$OLD" 2>/dev/null || true
echo "Ink Chess installed. Open it from the Kindle library or run ;kpm launch chess."
