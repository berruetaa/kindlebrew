#!/bin/sh
set -u

TARGET="/mnt/us/kindlebrew/ssh"
STATE="/var/local/kindlebrew-ssh"

if [ -x "$TARGET/server.sh" ]; then
  "$TARGET/server.sh" stop || true
fi

if [ -f "$TARGET/.kindlebrew-managed" ]; then
  rm -rf "$TARGET/bin"
  rm -f "$TARGET/server.sh" "$TARGET/README.txt" "$TARGET/.kindlebrew-managed"
  echo "Preserved user files in $TARGET (authorized_keys/config, if present)."
fi

if [ -f "$STATE/.kindlebrew-managed" ]; then
  rm -rf "$STATE"
fi

echo "Kindlebrew SSH server uninstalled."
