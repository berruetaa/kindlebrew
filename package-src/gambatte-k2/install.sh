#!/bin/sh
set -eu

TARGET="/mnt/us/extensions/gambatte-k2"
DOC="/mnt/us/documents/Gambatte-K2.sh"
NEW="${TARGET}.kpm-new.$$"
OLD="${TARGET}.kpm-old.$$"
DOC_NEW="${DOC}.kpm-new.$$"

if [ ! -d "payload" ] || [ ! -f "payload/shortcut_gambatte-k2.sh" ]; then
  echo "Package payload is incomplete: shortcut_gambatte-k2.sh not found."
  exit 1
fi

mkdir -p /mnt/us/extensions /mnt/us/documents
rm -rf "$NEW" "$OLD"
rm -f "$DOC_NEW"
mkdir -p "$NEW"
if ! cp -R payload/. "$NEW/"; then
  rm -rf "$NEW"
  exit 1
fi
chmod 755 "$NEW/shortcut_gambatte-k2.sh" 2>/dev/null || true

if ! cp "$NEW/shortcut_gambatte-k2.sh" "$DOC_NEW"; then
  rm -rf "$NEW"
  exit 1
fi
chmod 755 "$DOC_NEW" 2>/dev/null || true

had_old=0
if [ -e "$TARGET" ]; then
  if ! mv "$TARGET" "$OLD"; then
    rm -rf "$NEW"
    rm -f "$DOC_NEW"
    exit 1
  fi
  had_old=1
fi

if ! mv "$NEW" "$TARGET"; then
  [ "$had_old" -eq 0 ] || mv "$OLD" "$TARGET" 2>/dev/null || true
  rm -rf "$NEW"
  rm -f "$DOC_NEW"
  exit 1
fi

if ! mv -f "$DOC_NEW" "$DOC"; then
  rm -rf "$TARGET"
  [ "$had_old" -eq 0 ] || mv "$OLD" "$TARGET" 2>/dev/null || true
  rm -f "$DOC_NEW"
  exit 1
fi

rm -rf "$OLD" 2>/dev/null || true
echo "Gambatte-K2 installed. Open 'Gambatte-K2' from the Kindle library or run ;kpm launch gambatte-k2."
