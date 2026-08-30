#!/bin/sh
set -eu

if [ "${1:-}" = "upgrade" ]; then
  echo "chess: preserving installed state for KPM upgrade."
  exit 0
fi

if [ -f "library-install.sh" ]; then
  sh "library-install.sh" uninstall
else
  rm -f "/mnt/us/documents/Ink-Chess.sh"
  rm -rf "/mnt/us/documents/Ink-Chess.sh.sdr"
fi

rm -rf "/mnt/us/extensions/kindlebrew-chess"

echo "Ink Chess removed. Your save in /mnt/us/kindlebrew-data/chess is intentionally kept."
