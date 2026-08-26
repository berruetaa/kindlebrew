#!/bin/sh
set -eu

TARGET="/mnt/us/extensions/gambatte-k2"
DOC="/mnt/us/documents/Gambatte-K2.sh"
TMP="${TARGET}.kpm-new"

if [ "${KPM_PLATFORM:-}" != "kindlehf" ]; then
  echo "Gambatte-K2 KPM package supports kindlehf only (got: ${KPM_PLATFORM:-unknown})."
  exit 1
fi

if [ ! -d "payload" ] || [ ! -f "payload/shortcut_gambatte-k2.sh" ]; then
  echo "Package payload is incomplete: shortcut_gambatte-k2.sh not found."
  exit 1
fi

mkdir -p /mnt/us/extensions /mnt/us/documents
rm -rf "$TMP"
mkdir -p "$TMP"
cp -R payload/. "$TMP/"
chmod 755 "$TMP/shortcut_gambatte-k2.sh" 2>/dev/null || true

rm -rf "$TARGET"
mv "$TMP" "$TARGET"

cp "$TARGET/shortcut_gambatte-k2.sh" "$DOC"
chmod 755 "$DOC" 2>/dev/null || true

echo "Gambatte-K2 installed. Open 'Gambatte-K2' from the Kindle library or run ;kpm launch gambatte-k2."
