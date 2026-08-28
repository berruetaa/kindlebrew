#!/bin/sh
set -eu

TARGET="/mnt/us/extensions/gnomegames"
ENTRY="$TARGET/bin/gnomegames.sh"
SHIM="$TARGET/lib/libGL.so.1"

if [ ! -f "$ENTRY" ]; then
  echo "GNOME Chess runtime is not installed correctly."
  exit 1
fi

if [ ! -f "$SHIM" ]; then
  echo "GNOME Chess libGL compatibility shim is missing."
  exit 1
fi

export LD_LIBRARY_PATH="$TARGET/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
exec sh "$ENTRY" glchess "$@"
