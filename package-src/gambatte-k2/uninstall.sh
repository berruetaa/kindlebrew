#!/bin/sh
set -eu

if [ "${1:-}" = "upgrade" ]; then
  echo "gambatte-k2: preserving installed state for KPM upgrade."
  exit 0
fi

rm -f /mnt/us/documents/Gambatte-K2.sh
rm -rf /mnt/us/extensions/gambatte-k2 /mnt/us/extensions/gambatte-k2.kpm-new
echo "Gambatte-K2 removed. ROMs stored elsewhere are untouched."
