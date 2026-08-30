#!/bin/sh
set -eu

TARGET="/mnt/us/kindlebrew/ssh"
STATE="/var/local/kindlebrew-ssh"
MARKER="$TARGET/.kindlebrew-managed"
STATE_MARKER="$STATE/.kindlebrew-managed"

if [ -e "$TARGET" ] && [ ! -f "$MARKER" ]; then
  echo "Existing $TARGET is not managed by Kindlebrew; refusing to overwrite it."
  exit 1
fi
if [ -e "$STATE" ] && [ ! -f "$STATE_MARKER" ]; then
  echo "Existing $STATE is not managed by Kindlebrew; refusing to overwrite it."
  exit 1
fi

for required in bin/dropbear bin/dropbearkey bin/scp server.sh; do
  [ -f "$required" ] || { echo "Package is missing $required"; exit 1; }
done

mkdir -p "$TARGET/bin" "$STATE"
cp bin/dropbear "$TARGET/bin/dropbear"
cp bin/dropbearkey "$TARGET/bin/dropbearkey"
cp bin/scp "$TARGET/bin/scp"
cp server.sh "$TARGET/server.sh"
chmod 755 "$TARGET/bin/dropbear" "$TARGET/bin/dropbearkey" "$TARGET/bin/scp" "$TARGET/server.sh"
chmod 700 "$STATE" 2>/dev/null || true
printf '%s\n' 'managed-by=kindlebrew' > "$MARKER"
printf '%s\n' 'managed-by=kindlebrew' > "$STATE_MARKER"

if [ ! -e "$TARGET/config" ]; then
  cat > "$TARGET/config" <<'EOF'
# Kindlebrew SSH server configuration
PORT=2222
EOF
fi

if [ ! -s "$TARGET/authorized_keys" ]; then
  for candidate in \
    /mnt/us/usbnetlite/etc/dropbear/authorized_keys \
    /mnt/us/koreader/settings/SSH/authorized_keys
  do
    if [ -s "$candidate" ]; then
      cp "$candidate" "$TARGET/authorized_keys"
      echo "Imported authorized_keys from $candidate"
      break
    fi
  done
fi

cat > "$TARGET/README.txt" <<'EOF'
Kindlebrew SSH Server
=====================

Authentication is public-key only. Password login is disabled.

1. On your computer, create a key if you do not already have one:
   ssh-keygen -t ed25519

2. While the Kindle is mounted over USB, copy the PUBLIC key contents
   (for example ~/.ssh/id_ed25519.pub) into:
   kindlebrew/ssh/authorized_keys

   The file may contain multiple authorized_keys lines.

3. Safely eject the Kindle and run:
   ;kpm launch ssh-server

4. Connect over Wi-Fi:
   ssh -p 2222 root@KINDLE_IP

The default port is 2222. Change kindlebrew/ssh/config to use another port.

The trusted copy of authorized_keys and the SSH host key live under
/var/local/kindlebrew-ssh with normal Unix permissions. The USB-visible
authorized_keys file is only a staging copy.

Legacy SCP is available with modern OpenSSH clients using:
   scp -O -P 2222 FILE root@KINDLE_IP:/mnt/us/

To stop the server from an SSH session:
   /mnt/us/kindlebrew/ssh/server.sh stop
EOF

"$TARGET/bin/dropbear" -V >/dev/null 2>&1 || {
  echo "Bundled Dropbear binary cannot run on this Kindle."
  exit 1
}

"$TARGET/server.sh" prepare

if [ -s "$TARGET/authorized_keys" ]; then
  echo "SSH server installed and an authorized key is configured."
else
  echo "SSH server installed. Add your public key to kindlebrew/ssh/authorized_keys over USB before starting it."
fi
echo "Start it with ;kpm launch ssh-server"
