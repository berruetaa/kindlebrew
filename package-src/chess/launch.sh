#!/bin/sh
set -eu

TARGET="/mnt/us/extensions/kindlebrew-chess"
BIN="$TARGET/inkchess"
ENGINE="$TARGET/stockfish"
ASSETS="$TARGET/assets"

if [ ! -x "$BIN" ]; then
  echo "Ink Chess is not installed."
  exit 1
fi
if [ ! -x "$ENGINE" ]; then
  echo "Ink Chess Stockfish engine is missing."
  exit 1
fi
if [ ! -d "$ASSETS" ]; then
  echo "Ink Chess piece assets are missing."
  exit 1
fi

export INKCHESS_ASSET_DIR="$ASSETS"
exec "$BIN" "$ENGINE" "$@"
