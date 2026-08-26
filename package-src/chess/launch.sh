#!/bin/sh
set -eu
ENTRY="/mnt/us/extensions/gnomegames/bin/gnomegames.sh"
if [ ! -f "$ENTRY" ]; then
  echo "GNOME Chess runtime is not installed correctly."
  exit 1
fi
exec sh "$ENTRY" glchess "$@"
