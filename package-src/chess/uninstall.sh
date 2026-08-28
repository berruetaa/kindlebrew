#!/bin/sh
set -eu

if [ "${1:-}" = "upgrade" ]; then
  echo "chess: preserving installed state for KPM upgrade."
  exit 0
fi

TARGET="/mnt/us/extensions/gnomegames"
DOC="/mnt/us/documents/GnomeChess.sh"
OTHER="/mnt/us/documents/GnomeMines.sh"

rm -f "$DOC"
if [ ! -e "$OTHER" ] && [ -f "$TARGET/.kindlebrew-managed" ]; then
  rm -rf "$TARGET" "$TARGET.kpm-new"
fi
echo "GNOME Chess removed."
