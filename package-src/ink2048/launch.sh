#!/bin/sh
set -eu

BIN="/mnt/us/extensions/kindlebrew-ink2048/ink2048"
if [ ! -x "$BIN" ]; then
  echo "Ink 2048 is not installed."
  exit 1
fi

exec "$BIN"
