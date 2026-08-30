#!/bin/sh
set -u

BASE="/mnt/us/kindlebrew/ssh"
STATE="/var/local/kindlebrew-ssh"
PIDFILE="/tmp/kindlebrew-ssh.pid"
FWSTATE="/tmp/kindlebrew-ssh-firewall"
DROPBEAR="$BASE/bin/dropbear"
KEYGEN="$BASE/bin/dropbearkey"
HOSTKEY="$STATE/dropbear_ed25519_host_key"
TRUSTED_KEYS="$STATE/authorized_keys"
STAGING_KEYS="$BASE/authorized_keys"
PORT=2222

if [ -r "$BASE/config" ]; then
  # shellcheck disable=SC1091
  . "$BASE/config"
fi

case "$PORT" in
  ''|*[!0-9]*)
    echo "Invalid SSH port in $BASE/config: $PORT"
    exit 1
    ;;
esac
if [ "$PORT" -lt 1 ] || [ "$PORT" -gt 65535 ]; then
  echo "SSH port must be between 1 and 65535."
  exit 1
fi

is_running() {
  [ -s "$PIDFILE" ] || return 1
  pid="$(cat "$PIDFILE" 2>/dev/null || true)"
  [ -n "$pid" ] && [ -d "/proc/$pid" ]
}

sync_keys() {
  mkdir -p "$STATE"
  chmod 700 "$STATE" 2>/dev/null || true

  if [ -s "$STAGING_KEYS" ]; then
    tmp="$STATE/authorized_keys.new"
    tr -d '\r' < "$STAGING_KEYS" > "$tmp"
    chmod 600 "$tmp" 2>/dev/null || true
    mv "$tmp" "$TRUSTED_KEYS"
  fi
}

ensure_hostkey() {
  if [ ! -s "$HOSTKEY" ]; then
    "$KEYGEN" -t ed25519 -f "$HOSTKEY"
    chmod 600 "$HOSTKEY" 2>/dev/null || true
  fi
}

prepare() {
  [ -x "$DROPBEAR" ] || { echo "Missing executable: $DROPBEAR"; return 1; }
  [ -x "$KEYGEN" ] || { echo "Missing executable: $KEYGEN"; return 1; }
  sync_keys
  ensure_hostkey
}

firewall_open() {
  command -v iptables >/dev/null 2>&1 || {
    echo "Warning: iptables not found; the Kindle firewall may block port $PORT."
    return 0
  }

  if [ -f "$FWSTATE" ]; then
    return 0
  fi

  if iptables -I INPUT 1 -p tcp --dport "$PORT" -m conntrack --ctstate NEW,ESTABLISHED -j ACCEPT 2>/dev/null; then
    if iptables -I OUTPUT 1 -p tcp --sport "$PORT" -m conntrack --ctstate ESTABLISHED -j ACCEPT 2>/dev/null; then
      printf '%s\n' "$PORT" > "$FWSTATE"
      return 0
    fi
    iptables -D INPUT -p tcp --dport "$PORT" -m conntrack --ctstate NEW,ESTABLISHED -j ACCEPT 2>/dev/null || true
  fi

  echo "Could not open the Kindle firewall for TCP port $PORT."
  return 1
}

firewall_close() {
  command -v iptables >/dev/null 2>&1 || { rm -f "$FWSTATE"; return 0; }

  fwport="$PORT"
  if [ -s "$FWSTATE" ]; then
    saved="$(cat "$FWSTATE" 2>/dev/null || true)"
    case "$saved" in
      ''|*[!0-9]*) ;;
      *) fwport="$saved" ;;
    esac
  fi

  iptables -D INPUT -p tcp --dport "$fwport" -m conntrack --ctstate NEW,ESTABLISHED -j ACCEPT 2>/dev/null || true
  iptables -D OUTPUT -p tcp --sport "$fwport" -m conntrack --ctstate ESTABLISHED -j ACCEPT 2>/dev/null || true
  rm -f "$FWSTATE"
}

wifi_ip() {
  if command -v ip >/dev/null 2>&1; then
    ip -4 addr show wlan0 2>/dev/null | awk '/inet / { sub(/\/.*/, "", $2); print $2; exit }'
    return
  fi
  if command -v ifconfig >/dev/null 2>&1; then
    ifconfig wlan0 2>/dev/null | sed -n 's/.*inet addr:\([0-9.]*\).*/\1/p; s/.*inet \([0-9.]*\).*/\1/p' | sed -n '1p'
  fi
}

start_server() {
  if is_running; then
    pid="$(cat "$PIDFILE")"
    echo "Kindlebrew SSH server is already running (PID $pid, port $PORT)."
    ipaddr="$(wifi_ip || true)"
    [ -n "$ipaddr" ] && echo "Connect: ssh -p $PORT root@$ipaddr"
    return 0
  fi

  rm -f "$PIDFILE"
  prepare || return 1

  if [ ! -s "$TRUSTED_KEYS" ]; then
    echo "SSH server not started: no authorized key is configured."
    echo "Put your public key in $STAGING_KEYS, eject USB storage, then run ;kpm launch ssh-server again."
    return 1
  fi

  firewall_open || return 1

  if ! env -i \
      PATH="$BASE/bin:/usr/local/bin:/usr/bin:/bin:/usr/sbin:/sbin" \
      HOME="/root" USER="root" LOGNAME="root" \
      "$DROPBEAR" \
      -e \
      -r "$HOSTKEY" \
      -D "$STATE" \
      -s -j -k -m \
      -p "0.0.0.0:$PORT" \
      -P "$PIDFILE"; then
    firewall_close
    echo "Dropbear failed to start."
    return 1
  fi

  sleep 1
  if ! is_running; then
    firewall_close
    echo "Dropbear exited before becoming ready."
    return 1
  fi

  pid="$(cat "$PIDFILE")"
  echo "Kindlebrew SSH server started (PID $pid, port $PORT, public-key auth only)."
  ipaddr="$(wifi_ip || true)"
  if [ -n "$ipaddr" ]; then
    echo "Connect: ssh -p $PORT root@$ipaddr"
  else
    echo "Wi-Fi address not found yet. The server is listening on IPv4 port $PORT."
  fi
}

stop_server() {
  if is_running; then
    pid="$(cat "$PIDFILE")"
    kill -TERM "$pid" 2>/dev/null || true
    n=0
    while [ "$n" -lt 5 ] && [ -d "/proc/$pid" ]; do
      sleep 1
      n=$((n + 1))
    done
    if [ -d "/proc/$pid" ]; then
      kill -KILL "$pid" 2>/dev/null || true
    fi
  fi

  rm -f "$PIDFILE"
  firewall_close
  echo "Kindlebrew SSH server stopped."
}

status_server() {
  if is_running; then
    pid="$(cat "$PIDFILE")"
    echo "running pid=$pid port=$PORT"
    ipaddr="$(wifi_ip || true)"
    [ -n "$ipaddr" ] && echo "ssh -p $PORT root@$ipaddr"
    return 0
  fi
  echo "stopped"
  return 1
}

case "${1:-start}" in
  prepare) prepare ;;
  start) start_server ;;
  stop) stop_server ;;
  restart) stop_server; start_server ;;
  status) status_server ;;
  *)
    echo "Usage: $0 {prepare|start|stop|restart|status}"
    exit 2
    ;;
esac
