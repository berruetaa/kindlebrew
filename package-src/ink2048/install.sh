#!/bin/sh
set -eu

TARGET="/mnt/us/extensions/kindlebrew-ink2048"
DOC="/mnt/us/documents/Ink-2048.sh"

if [ ! -f "payload/ink2048" ]; then
  echo "Ink 2048 payload is missing."
  exit 1
fi
if [ ! -f "scriptlet.sh" ]; then
  echo "Ink 2048 library Scriptlet is missing."
  exit 1
fi

mkdir -p "$TARGET" /mnt/us/documents
cp "payload/ink2048" "$TARGET/ink2048"
chmod 755 "$TARGET/ink2048" 2>/dev/null || true

# A game may opt into its own Kindle-library cover simply by shipping one
# supported cover.* file in the KPM package. The generated Scriptlet already
# points at the final absolute path.
cover_count=0
for cover in cover.svg cover.png cover.jpg cover.jpeg; do
  if [ -f "$cover" ]; then
    cover_count=$((cover_count + 1))
    if [ "$cover_count" -gt 1 ]; then
      echo "Ink 2048 package contains more than one cover asset."
      exit 1
    fi
    cp "$cover" "$TARGET/$cover"
    chmod 666 "$TARGET/$cover" 2>/dev/null || true
  fi
done

cp "scriptlet.sh" "$DOC"
chmod 755 "$DOC" 2>/dev/null || true

echo "Ink 2048 installed. Open it from the Kindle library or run ;kpm launch ink2048."
