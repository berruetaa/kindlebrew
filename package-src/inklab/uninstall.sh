#!/bin/sh
set -eu

if [ "${1:-}" = "upgrade" ]; then
  echo "inklab: preserving installed state for KPM upgrade."
  exit 0
fi

rm -rf "/mnt/us/extensions/kindlebrew-inklab"
rm -f "/mnt/us/documents/Kindlebrew-InkLab.sh"
rm -rf "/mnt/us/documents/Kindlebrew-InkLab.sh.sdr"
echo "InkLab removed. Diagnostic text files are intentionally kept."
