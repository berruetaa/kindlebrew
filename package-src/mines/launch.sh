#!/bin/sh
set -eu
ENTRY="/mnt/us/extensions/gnomegames/bin/gnomegames.sh"
if [ ! -f "$ENTRY" ]; then
  echo "GNOME Mines runtime is not installed correctly."
  exit 1
fi
exec sh "$ENTRY" gnomine "$@"
