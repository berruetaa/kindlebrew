#!/bin/sh
set -eu

TARGET="/mnt/us/extensions/kindlebrew-ink2048"

if [ ! -f "payload/ink2048" ]; then
  echo "Ink 2048 payload is missing."
  exit 1
fi
if [ ! -f "library-install.sh" ]; then
  echo "Ink 2048 library integration helper is missing."
  exit 1
fi

mkdir -p "$TARGET"
cp "payload/ink2048" "$TARGET/ink2048"
chmod 755 "$TARGET/ink2048" 2>/dev/null || true

sh "library-install.sh" install

echo "Ink 2048 installed. Open it from the Kindle library or run ;kpm launch ink2048."
