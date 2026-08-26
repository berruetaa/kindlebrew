#!/bin/sh
set -eu
TARGET="/mnt/us/extensions/gambatte-k2"
if [ ! -f "$TARGET/shortcut_gambatte-k2.sh" ]; then
  echo "Gambatte-K2 is not installed correctly."
  exit 1
fi
cd "$TARGET"
exec sh ./shortcut_gambatte-k2.sh "$@"
