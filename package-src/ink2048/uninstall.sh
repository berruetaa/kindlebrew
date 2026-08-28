#!/bin/sh
set -eu

if [ -f "library-install.sh" ]; then
  sh "library-install.sh" uninstall
else
  # Compatibility fallback for packages installed before library-install.sh.
  rm -f "/mnt/us/documents/Ink-2048.sh"
  rm -rf "/mnt/us/documents/Ink-2048.sh.sdr"
fi

rm -rf "/mnt/us/extensions/kindlebrew-ink2048"

echo "Ink 2048 removed. Your save in /mnt/us/kindlebrew-data/ink2048 is intentionally kept."
