#!/bin/sh
set -eu

if [ "${1:-}" = "upgrade" ]; then
  echo 'chess: preserving installed state for KPM upgrade.'
  exit 0
fi

TARGET='/mnt/us/extensions/kindlebrew-chess'
MARKER='.kindlebrew-managed'
MARKER_VALUE='kindlebrew-package=chess'
DOC='/mnt/us/documents/Ink-Chess.sh'
LEGACY_DOC='/mnt/us/documents/GnomeChess.sh'

is_owned_tree() {
  [ -d "$TARGET" ] && [ -f "$TARGET/$MARKER" ] &&
    [ "$(cat "$TARGET/$MARKER" 2>/dev/null || true)" = "$MARKER_VALUE" ]
}

is_current_launcher() {
  [ -f "$DOC" ] &&
    grep -Fqx '# DontUseFBInk' "$DOC" &&
    grep -Fqx 'exec /var/local/kmc/bin/kpm launch chess "$@"' "$DOC"
}

is_legacy_chess_launcher() {
  [ -f "$LEGACY_DOC" ] &&
    grep -Fqx '# Name: GNOME Chess' "$LEGACY_DOC" &&
    grep -Fqx 'TARGET="/mnt/us/extensions/gnomegames"' "$LEGACY_DOC" &&
    grep -Fqx 'exec sh "$TARGET/bin/gnomegames.sh" glchess' "$LEGACY_DOC"
}

if [ -e "$TARGET" ] && ! is_owned_tree; then
  echo "Refusing to remove unmanaged path: $TARGET" >&2
  exit 1
fi

if [ -f library-install.sh ]; then
  sh library-install.sh uninstall
elif is_current_launcher; then
  rm -f "$DOC"
  rm -rf "$DOC.sdr"
else
  echo "Preserving unmanaged launcher: $DOC" >&2
fi

if is_legacy_chess_launcher; then
  rm -f "$LEGACY_DOC"
  rm -rf "$LEGACY_DOC.sdr"
elif [ -e "$LEGACY_DOC" ]; then
  echo "Preserving unmanaged legacy launcher: $LEGACY_DOC" >&2
fi

if [ -e "$TARGET" ]; then
  rm -rf "$TARGET"
fi

echo 'Ink Chess removed. Your save in /mnt/us/kindlebrew-data/chess is intentionally kept.'
