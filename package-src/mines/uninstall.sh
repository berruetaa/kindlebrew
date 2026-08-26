#!/bin/sh
set -eu
TARGET="/mnt/us/extensions/gnomegames"
DOC="/mnt/us/documents/GnomeMines.sh"
OTHER="/mnt/us/documents/GnomeChess.sh"

rm -f "$DOC"
if [ ! -e "$OTHER" ] && [ -f "$TARGET/.kindlebrew-managed" ]; then
  rm -rf "$TARGET" "$TARGET.kpm-new"
fi
echo "GNOME Mines removed."
