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

# Validate the optional cover before mutating an existing installation.
cover_name=""
for cover in cover.svg cover.png cover.jpg cover.jpeg; do
  if [ -f "$cover" ]; then
    if [ -n "$cover_name" ]; then
      echo "Ink 2048 package contains more than one cover asset."
      exit 1
    fi
    cover_name="$cover"
  fi
done

mkdir -p "$TARGET" /mnt/us/documents
cp "payload/ink2048" "$TARGET/ink2048"
chmod 755 "$TARGET/ink2048" 2>/dev/null || true

# Remove stale artwork when an upgrade changes format or drops its custom cover.
rm -f "$TARGET/cover.svg" "$TARGET/cover.png" "$TARGET/cover.jpg" "$TARGET/cover.jpeg"
if [ -n "$cover_name" ]; then
  cp "$cover_name" "$TARGET/$cover_name"
  # sh_integration currently validates path icons with access(R_OK|W_OK).
  chmod 666 "$TARGET/$cover_name" 2>/dev/null || true
fi

# Install artwork first, then the Scriptlet. Rewriting the .sh is what makes
# the Kindle scanner/sh_integration re-read the current thumbnail metadata.
cp "scriptlet.sh" "$DOC"
chmod 755 "$DOC" 2>/dev/null || true
touch "$DOC" 2>/dev/null || true

echo "Ink 2048 installed. Open it from the Kindle library or run ;kpm launch ink2048."
