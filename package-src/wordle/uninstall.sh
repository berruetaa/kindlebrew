#!/bin/sh
set -eu
TARGET="/mnt/us/documents/kwordle"
SCRIPT="/mnt/us/documents/kwordle.sh"
MARKER="$TARGET/.kindlebrew-managed"

if [ -f "$MARKER" ]; then
  rm -f "$SCRIPT"
  rm -rf "$TARGET"
  echo "KWordle removed."
else
  echo "KWordle files are not managed by Kindlebrew; leaving them untouched."
fi
rm -rf /mnt/us/.kindlebrew-wordle
