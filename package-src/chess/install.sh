#!/bin/sh
set -eu

TARGET='/mnt/us/extensions/kindlebrew-chess'
MARKER='.kindlebrew-managed'
MARKER_VALUE='kindlebrew-package=chess'
NEW="${TARGET}.kpm-new.$$"
OLD="${TARGET}.kpm-old.$$"
LEGACY_DOC='/mnt/us/documents/GnomeChess.sh'

is_owned_tree() {
  [ -d "$1" ] && [ -f "$1/$MARKER" ] &&
    [ "$(cat "$1/$MARKER" 2>/dev/null || true)" = "$MARKER_VALUE" ]
}

remove_owned_tree() {
  path="$1"
  [ ! -e "$path" ] && return 0
  if ! is_owned_tree "$path"; then
    echo "Refusing to remove unmanaged path: $path" >&2
    return 1
  fi
  rm -rf "$path"
}

is_legacy_chess_launcher() {
  [ -f "$LEGACY_DOC" ] &&
    grep -Fqx '# Name: GNOME Chess' "$LEGACY_DOC" &&
    grep -Fqx 'TARGET="/mnt/us/extensions/gnomegames"' "$LEGACY_DOC" &&
    grep -Fqx 'exec sh "$TARGET/bin/gnomegames.sh" glchess' "$LEGACY_DOC"
}

remove_legacy_chess_launcher() {
  [ ! -e "$LEGACY_DOC" ] && return 0
  if is_legacy_chess_launcher; then
    rm -f "$LEGACY_DOC"
    rm -rf "$LEGACY_DOC.sdr"
  else
    echo "Preserving unmanaged legacy launcher: $LEGACY_DOC" >&2
  fi
}

cleanup_new() {
  if is_owned_tree "$NEW"; then
    rm -rf "$NEW"
  fi
}
trap cleanup_new EXIT
trap 'exit 130' INT
trap 'exit 143' TERM

for required in payload/inkchess payload/stockfish library-install.sh scriptlet.sh; do
  if [ ! -f "$required" ]; then
    echo "Ink Chess package is incomplete: missing $required" >&2
    exit 1
  fi
done

ASSETS='whitePawn whiteKnight whiteBishop whiteRook whiteQueen whiteKing blackPawn blackKnight blackBishop blackRook blackQueen blackKing'
if [ ! -d payload/assets ]; then
  echo 'Ink Chess piece assets are missing.' >&2
  exit 1
fi
for name in $ASSETS; do
  asset="payload/assets/$name.r8a8"
  if [ ! -f "$asset" ] || [ "$(wc -c < "$asset")" -ne 32768 ]; then
    echo "Missing or invalid Ink Chess piece asset: $asset" >&2
    exit 1
  fi
done
asset_count=0
for asset in payload/assets/*.r8a8; do
  [ -f "$asset" ] || continue
  asset_count=$((asset_count + 1))
done
if [ "$asset_count" -ne 12 ]; then
  echo "Expected exactly 12 Ink Chess piece assets, found $asset_count." >&2
  exit 1
fi

sh -n library-install.sh
sh -n scriptlet.sh

if [ -e "$TARGET" ] && ! is_owned_tree "$TARGET"; then
  echo 'Existing Ink Chess directory is not managed by this package; refusing to overwrite it.' >&2
  exit 1
fi
if [ -e "$NEW" ]; then
  remove_owned_tree "$NEW"
fi
if [ -e "$OLD" ]; then
  remove_owned_tree "$OLD"
fi

mkdir -p "$NEW/assets"
printf '%s\n' "$MARKER_VALUE" > "$NEW/$MARKER"
cp payload/inkchess "$NEW/inkchess"
cp payload/stockfish "$NEW/stockfish"
for name in $ASSETS; do
  cp "payload/assets/$name.r8a8" "$NEW/assets/$name.r8a8"
done
chmod 755 "$NEW/inkchess" "$NEW/stockfish"

had_old=0
if [ -e "$TARGET" ]; then
  mv "$TARGET" "$OLD"
  had_old=1
fi

if ! mv "$NEW" "$TARGET"; then
  if [ "$had_old" -ne 0 ]; then
    mv "$OLD" "$TARGET" 2>/dev/null || true
  fi
  exit 1
fi

if ! sh library-install.sh install; then
  remove_owned_tree "$TARGET" || true
  if [ "$had_old" -ne 0 ]; then
    mv "$OLD" "$TARGET" 2>/dev/null || true
  fi
  exit 1
fi

remove_legacy_chess_launcher
if [ "$had_old" -ne 0 ]; then
  remove_owned_tree "$OLD"
fi
trap - EXIT INT TERM
echo 'Ink Chess installed. Open it from the Kindle library or run ;kpm launch chess.'
