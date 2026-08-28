#!/bin/sh
set -eu

TARGET="/mnt/us/extensions/kindlebrew-ink2048"
NEW="${TARGET}.kpm-new.$$"
OLD="${TARGET}.kpm-old.$$"

if [ ! -f "payload/ink2048" ]; then
  echo "Ink 2048 payload is missing."
  exit 1
fi
if [ ! -f "library-install.sh" ] || [ ! -f "scriptlet.sh" ]; then
  echo "Ink 2048 library integration is incomplete."
  exit 1
fi
sh -n "library-install.sh"
sh -n "scriptlet.sh"

rm -rf "$NEW" "$OLD"
mkdir -p "$NEW"
if ! cp "payload/ink2048" "$NEW/ink2048"; then
  rm -rf "$NEW"
  exit 1
fi
chmod 755 "$NEW/ink2048" 2>/dev/null || true

had_old=0
if [ -e "$TARGET" ]; then
  if ! mv "$TARGET" "$OLD"; then
    rm -rf "$NEW"
    exit 1
  fi
  had_old=1
fi

if ! mv "$NEW" "$TARGET"; then
  [ "$had_old" -eq 0 ] || mv "$OLD" "$TARGET" 2>/dev/null || true
  rm -rf "$NEW"
  exit 1
fi

if ! sh "library-install.sh" install; then
  rm -rf "$TARGET"
  if [ "$had_old" -ne 0 ]; then
    mv "$OLD" "$TARGET" 2>/dev/null || true
  fi
  exit 1
fi

rm -rf "$OLD" 2>/dev/null || true
echo "Ink 2048 installed. Open it from the Kindle library or run ;kpm launch ink2048."
