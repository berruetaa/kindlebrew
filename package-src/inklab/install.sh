#!/bin/sh
set -eu
TARGET="/mnt/us/extensions/kindlebrew-inklab"
DOC="/mnt/us/documents/Kindlebrew-InkLab.sh"

if [ ! -f "payload/inklab" ]; then
  echo "InkLab payload is missing."
  exit 1
fi

mkdir -p "$TARGET" /mnt/us/documents
cp "payload/inklab" "$TARGET/inklab"
chmod 755 "$TARGET/inklab" 2>/dev/null || true

cat > "$DOC" <<'EOF'
#!/bin/sh
# Name: Kindlebrew InkLab
# Author: Kindlebrew
# Icon: data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAGAAAABgCAAAAADH8yjkAAAB8klEQVR42u1a25HCMAyUPKkjqolKqCWVUJNoRPfBACFRbD3s4+Ym+QIHdq2nVwFkeF/z6vW903qBwddJcBKcBP+BAEXrJxQDE6UvTcrnKLxbkN3a1A9epyhd8QEAGwSUj2rVRU98jhwyqOSNGmTgaBJhqw4og//KU2wUGicCIHULqFP1UjUGbgOuAADL0wRsFloEHuAKt4Os5a1p4krNy/vdssPo0E0ve1u6tGtHNpQYPu0NODBham+UU/3QbwGRi6RE3U+DCNBbjyWGzw/8z+Ja3EFOb99pgYJ/axngIVD3f2vgf55vuPr6/IKUR/+hfcu/r7rprGB4YkDHR8rSIwYUPi3KYHybiyhxWk9jsj+UpjG1UQbjm9M0LJbKYHwjQULslcH4MN23K7NLF6nr6xsmC3D4GIujCQBxMEHcCHsl42iCoJsMBMwZI0wWSILB5iKJu8kYA5HooGgOsnyqa3tqmHWRIl20vrTVRR7pGMoml7rmQKx98j1QEt4BxM3gHqG8JeEfAsVX15E5WQ6Fdq9BXOz4deElxzeGZdGbQsYSWPH/iC7KXF8gwBQeVQmk066l7iLsacCGQJIMyiOySRdx0mf7e4Lng9uMm7iaRfk4cyNNpS++EgPJ+Ifruui3fkcD4NwfBb7ci06Ck+Ak6H79AMR1eU0+eUrxAAAAAElFTkSuQmCC
# DontUseFBInk
/var/local/kmc/bin/kpm launch inklab
EOF
chmod 755 "$DOC" 2>/dev/null || true

echo "InkLab installed. Open Kindlebrew InkLab from the library or run ;kpm launch inklab."
