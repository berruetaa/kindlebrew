#!/bin/sh
set -eu
BIN="/mnt/us/extensions/kindlebrew-inklab/inklab"
if [ ! -x "$BIN" ]; then
  echo "InkLab binary is not installed."
  exit 1
fi
exec "$BIN"
