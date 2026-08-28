#!/bin/sh
set -eu

TARGET="/mnt/us/extensions/kindlebrew-ink2048"
DOC="/mnt/us/documents/Ink-2048.sh"

if [ ! -f "payload/ink2048" ]; then
  echo "Ink 2048 payload is missing."
  exit 1
fi

mkdir -p "$TARGET" /mnt/us/documents
cp "payload/ink2048" "$TARGET/ink2048"
chmod 755 "$TARGET/ink2048" 2>/dev/null || true

cat > "$DOC" <<'EOF'
#!/bin/sh
# Name: Ink 2048
# Author: Kindlebrew
# DontUseFBInk
/var/local/kmc/bin/kpm launch ink2048
EOF
chmod 755 "$DOC" 2>/dev/null || true

echo "Ink 2048 installed. Open it from the Kindle library or run ;kpm launch ink2048."
