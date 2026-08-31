#!/bin/sh
set -eu

TARGET='/mnt/us/extensions/kindlebrew-chess'
BIN="$TARGET/inkchess"
ENGINE="$TARGET/stockfish"
ASSETS="$TARGET/assets"
MARKER="$TARGET/.kindlebrew-managed"

if [ ! -f "$MARKER" ] ||
   [ "$(cat "$MARKER" 2>/dev/null || true)" != 'kindlebrew-package=chess' ]; then
  echo 'Ink Chess installation ownership marker is missing or invalid.'
  exit 1
fi

if [ ! -x "$BIN" ]; then
  echo 'Ink Chess is not installed.'
  exit 1
fi
if [ ! -x "$ENGINE" ]; then
  echo 'Ink Chess Stockfish engine is missing.'
  exit 1
fi
if [ ! -d "$ASSETS" ]; then
  echo 'Ink Chess piece assets are missing.'
  exit 1
fi

export INKCHESS_ASSET_DIR="$ASSETS"
exec "$BIN" "$ENGINE" "$@"
