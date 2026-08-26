#!/bin/sh
set -eu
SCRIPT="/mnt/us/documents/kwordle.sh"
if [ ! -f "$SCRIPT" ]; then
  echo "KWordle is not installed correctly."
  exit 1
fi
exec sh "$SCRIPT" "$@"
