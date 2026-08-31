#!/bin/sh
set -eu

TARGET='/mnt/us/extensions/kindlebrew-chess'
TARGET_MARKER='kindlebrew-package=chess'
QA_PREFIX='/mnt/us/documents/kindlebrew-qa/'

die() {
  echo "kindle-lab: $*" >&2
  exit 1
}

validate_run_root() {
  run_root="$1"
  case "$run_root" in
    "$QA_PREFIX"*) ;;
    *) die "run root is outside $QA_PREFIX" ;;
  esac
  run_name="${run_root#"$QA_PREFIX"}"
  case "$run_name" in
    ''|*/*|*[!A-Za-z0-9._-]*) die 'unsafe run id' ;;
  esac
}

validate_installed() {
  [ -x "$TARGET/inkchess" ] || die 'InkChess binary is not installed'
  [ -x "$TARGET/stockfish" ] || die 'Stockfish binary is not installed'
  [ -f "$TARGET/.kindlebrew-managed" ] || die 'installation marker is missing'
  [ "$(cat "$TARGET/.kindlebrew-managed")" = "$TARGET_MARKER" ] ||
    die 'installation marker does not belong to chess'
}

process_check() {
  found=0
  for proc in /proc/[0-9]*; do
    [ -e "$proc/exe" ] || continue
    exe="$(readlink "$proc/exe" 2>/dev/null || true)"
    case "$exe" in
      "$TARGET/inkchess"|"$TARGET/stockfish"|"$QA_PREFIX"*/tools/uinput-touch)
        pid="${proc#/proc/}"
        ppid="$(awk '/^PPid:/ {print $2}' "$proc/status" 2>/dev/null || true)"
        state="$(awk '/^State:/ {print $2}' "$proc/status" 2>/dev/null || true)"
        rss="$(awk '/^VmRSS:/ {print $2 " " $3}' "$proc/status" 2>/dev/null || true)"
        fd_count="$(ls -1 "$proc/fd" 2>/dev/null | wc -l | tr -d ' ')"
        cpu_ticks="$(awk '{print $14 + $15}' "$proc/stat" 2>/dev/null || true)"
        printf 'pid=%s ppid=%s state=%s rss=%s fds=%s cpu_ticks=%s exe=%s\n' \
          "$pid" "$ppid" "$state" "$rss" "$fd_count" "$cpu_ticks" "$exe"
        found=1
        ;;
    esac
  done
  [ "$found" -eq 0 ] && echo 'no InkChess/Stockfish processes'
}

ensure_no_app() {
  for proc in /proc/[0-9]*; do
    [ -e "$proc/exe" ] || continue
    [ "$(readlink "$proc/exe" 2>/dev/null || true)" = "$TARGET/inkchess" ] || continue
    die "InkChess is already running as ${proc#/proc/}"
  done
}

latest_run_dir() {
  [ -f "$1/run/latest" ] || die 'no launched run exists'
  number="$(cat "$1/run/latest")"
  case "$number" in ''|*[!0-9]*) die 'invalid latest run metadata' ;; esac
  printf '%s/run/%s\n' "$1" "$number"
}

verify_app_pid() {
  run_dir="$1"
  [ -f "$run_dir/app.pid" ] && [ -f "$run_dir/app.start" ] || die 'app metadata is incomplete'
  app_pid="$(cat "$run_dir/app.pid")"
  case "$app_pid" in ''|*[!0-9]*) die 'invalid app pid metadata' ;; esac
  [ -e "/proc/$app_pid/exe" ] || die 'recorded InkChess process is not alive'
  [ "$(readlink "/proc/$app_pid/exe")" = "$TARGET/inkchess" ] || die 'PID no longer belongs to InkChess'
  current_start="$(awk '{print $22}' "/proc/$app_pid/stat")"
  [ "$current_start" = "$(cat "$run_dir/app.start")" ] || die 'PID start time changed'
}

verify_input_pid() {
  run_root="$1"
  input_dir="$run_root/input"
  [ -f "$input_dir/uinput.pid" ] && [ -f "$input_dir/uinput.start" ] ||
    die 'input metadata is incomplete'
  input_pid="$(cat "$input_dir/uinput.pid")"
  case "$input_pid" in ''|*[!0-9]*) die 'invalid input pid metadata' ;; esac
  [ -e "/proc/$input_pid/exe" ] || die 'recorded input process is not alive'
  [ "$(readlink "/proc/$input_pid/exe")" = "$run_root/tools/uinput-touch" ] ||
    die 'PID no longer belongs to this run input helper'
  current_start="$(awk '{print $22}' "/proc/$input_pid/stat")"
  [ "$current_start" = "$(cat "$input_dir/uinput.start")" ] || die 'input PID start time changed'
  [ -p "$input_dir/commands.fifo" ] || die 'input command FIFO is missing'
}

command="${1:-}"
case "$command" in
  inventory)
    date
    uname -a
    cat /etc/prettyversion.txt 2>/dev/null || true
    grep -E '^(Hardware|Processor|Features)' /proc/cpuinfo | head -20
    mount | grep -E ' on / |/mnt/us'
    df -h /mnt/us /tmp
    /var/local/kmc/bin/kpm version 2>/dev/null || true
    process_check
    ;;

  install-hooks)
    [ "$#" -eq 4 ] || die 'usage: install-hooks RUN_ROOT PACKAGE SHA256'
    validate_run_root "$2"
    package="$3"
    case "$package" in "$2/stage/"*.kpkg) ;; *) die 'package path is outside this run stage' ;; esac
    [ -f "$package" ] || die 'package is missing'
    expected_sha="$4"
    case "$expected_sha" in *[!0-9a-f]*|'') die 'invalid expected SHA256' ;; esac
    [ "${#expected_sha}" -eq 64 ] || die 'invalid expected SHA256 length'
    actual_sha="$(sha256sum "$package" | awk '{print $1}')"
    [ "$actual_sha" = "$expected_sha" ] || die 'uploaded package SHA256 mismatch'
    extract="$2/stage/extracted.$$.d"
    [ ! -e "$extract" ] || die 'unique extraction directory already exists'
    mkdir -p "$extract"
    printf '%s\n' 'kindlebrew-lab-stage' > "$extract/.kindlebrew-lab-stage"
    tar -xzf "$package" -C "$extract"
    [ -f "$extract/manifest.json" ] && [ -f "$extract/install.sh" ] || die 'extracted package is incomplete'
    (cd "$extract" && sh install.sh)
    mkdir -p "$2/results"
    printf 'package=%s\nsha256=%s\ninstalled_at=%s\n' \
      "$package" "$actual_sha" "$(date -Iseconds 2>/dev/null || date)" \
      > "$2/results/install.txt"
    ;;

  run)
    [ "$#" -eq 2 ] || die 'usage: run RUN_ROOT'
    validate_run_root "$2"
    validate_installed
    ensure_no_app
    [ -x "$2/tools/close-fds-exec" ] || die 'close-fds helper is missing'
    mkdir -p "$2/run" "$2/results" "$2/frames"
    counter=1
    if [ -f "$2/run/latest" ]; then
      previous="$(cat "$2/run/latest")"
      case "$previous" in ''|*[!0-9]*) die 'invalid run counter' ;; esac
      counter=$((previous + 1))
    fi
    run_dir="$2/run/$counter"
    [ ! -e "$run_dir" ] || die 'run directory already exists'
    mkdir -p "$run_dir"
    printf '%s\n' "$counter" > "$2/run/latest"
    nohup "$0" supervise "$2" "$run_dir" </dev/null >"$run_dir/supervisor.log" 2>&1 &
    printf 'supervisor_pid=%s run=%s\n' "$!" "$counter"
    ;;

  supervise)
    [ "$#" -eq 3 ] || die 'usage: supervise RUN_ROOT RUN_DIR'
    validate_run_root "$2"
    case "$3" in "$2/run/"[0-9]*) ;; *) die 'unsafe run directory' ;; esac
    validate_installed
    run_dir="$3"
    date > "$run_dir/started.txt"
    INKCHESS_ASSET_DIR="$TARGET/assets" "$2/tools/close-fds-exec" \
      "$TARGET/inkchess" "$TARGET/stockfish" \
      >"$run_dir/stdout.log" 2>"$run_dir/stderr.log" &
    app_pid="$!"
    printf '%s\n' "$app_pid" > "$run_dir/app.pid"
    awk '{print $22}' "/proc/$app_pid/stat" > "$run_dir/app.start"
    set +e
    wait "$app_pid"
    rc="$?"
    set -e
    printf '%s\n' "$rc" > "$run_dir/exit-code.txt"
    date > "$run_dir/finished.txt"
    process_check > "$run_dir/processes-after.txt"
    ;;

  status)
    [ "$#" -eq 2 ] || die 'usage: status RUN_ROOT'
    validate_run_root "$2"
    run_dir="$(latest_run_dir "$2")"
    echo "run_dir=$run_dir"
    if [ -f "$run_dir/app.pid" ]; then
      app_pid="$(cat "$run_dir/app.pid")"
      if [ -e "/proc/$app_pid/status" ]; then
        sed -n '/^Name:/p;/^State:/p;/^Pid:/p;/^PPid:/p;/^VmRSS:/p;/^Threads:/p' "/proc/$app_pid/status"
      fi
    fi
    [ ! -f "$run_dir/exit-code.txt" ] || echo "exit_code=$(cat "$run_dir/exit-code.txt")"
    process_check
    ;;

  stop)
    [ "$#" -ge 2 ] && [ "$#" -le 3 ] || die 'usage: stop RUN_ROOT [TERM|KILL]'
    validate_run_root "$2"
    run_dir="$(latest_run_dir "$2")"
    verify_app_pid "$run_dir"
    signal="${3:-TERM}"
    case "$signal" in TERM|KILL) ;; *) die 'signal must be TERM or KILL' ;; esac
    kill "-$signal" "$app_pid"
    echo "signal=$signal pid=$app_pid"
    ;;

  kill-stockfish)
    [ "$#" -eq 2 ] || die 'usage: kill-stockfish RUN_ROOT'
    validate_run_root "$2"
    run_dir="$(latest_run_dir "$2")"
    verify_app_pid "$run_dir"
    killed=0
    for proc in /proc/[0-9]*; do
      [ -e "$proc/exe" ] || continue
      [ "$(readlink "$proc/exe" 2>/dev/null || true)" = "$TARGET/stockfish" ] || continue
      ppid="$(awk '/^PPid:/ {print $2}' "$proc/status")"
      [ "$ppid" = "$app_pid" ] || continue
      stock_pid="${proc#/proc/}"
      kill -KILL "$stock_pid"
      echo "stockfish_killed=$stock_pid parent=$app_pid"
      killed=1
    done
    [ "$killed" -eq 1 ] || die 'no Stockfish child of current InkChess run found'
    ;;

  framebuffer)
    [ "$#" -eq 2 ] || die 'usage: framebuffer RUN_ROOT'
    validate_run_root "$2"
    [ -x "$2/tools/framebuffer-dump" ] || die 'framebuffer helper is missing'
    mkdir -p "$2/frames"
    stamp="$(date +%Y%m%dT%H%M%S)"
    suffix=0
    output="$2/frames/frame-$stamp.raw"
    while [ -e "$output" ]; do
      suffix=$((suffix + 1))
      [ "$suffix" -le 99 ] || die 'could not allocate a unique framebuffer name'
      output="$2/frames/frame-$stamp-$suffix.raw"
    done
    report="${output%.raw}.txt"
    "$2/tools/framebuffer-dump" "$output" > "$report"
    if [ -x "$2/tools/fbink-state" ]; then
      "$2/tools/fbink-state" >> "$report"
    fi
    sha256sum "$output" >> "$report"
    echo "$output"
    ;;

  input-start)
    [ "$#" -eq 2 ] || die 'usage: input-start RUN_ROOT'
    validate_run_root "$2"
    [ -x "$2/tools/uinput-touch" ] || die 'uinput helper is missing'
    input_dir="$2/input"
    mkdir -p "$input_dir"
    if [ -f "$input_dir/uinput.pid" ]; then
      old_pid="$(cat "$input_dir/uinput.pid")"
      case "$old_pid" in ''|*[!0-9]*) old_pid=0 ;; esac
      if [ "$old_pid" -gt 0 ] && [ -e "/proc/$old_pid/exe" ]; then
        die 'input helper metadata points to a live process'
      fi
    fi
    fifo="$input_dir/commands.fifo"
    if [ -e "$fifo" ]; then
      [ -p "$fifo" ] || die 'refusing to replace a non-FIFO input command path'
      rm -f "$fifo"
    fi
    mkfifo -m 600 "$fifo"
    nohup "$2/tools/uinput-touch" "$fifo" 1072 1448 \
      </dev/null >"$input_dir/uinput.log" 2>"$input_dir/uinput.err" &
    input_pid="$!"
    printf '%s\n' "$input_pid" > "$input_dir/uinput.pid"
    awk '{print $22}' "/proc/$input_pid/stat" > "$input_dir/uinput.start"
    ready=0
    attempt=0
    while [ "$attempt" -lt 30 ]; do
      if grep -q '^READY ' "$input_dir/uinput.log" 2>/dev/null; then ready=1; break; fi
      [ -e "/proc/$input_pid/exe" ] || break
      sleep 0.1
      attempt=$((attempt + 1))
    done
    [ "$ready" -eq 1 ] || die 'input helper did not become ready'
    verify_input_pid "$2"
    echo "input_pid=$input_pid fifo=$fifo"
    ;;

  input-send)
    [ "$#" -ge 3 ] || die 'usage: input-send RUN_ROOT tap|down|move|up ARGS...'
    validate_run_root "$2"
    verify_input_pid "$2"
    input_command="$3"
    case "$input_command" in
      tap)
        [ "$#" -eq 6 ] || die 'usage: input-send RUN_ROOT tap X Y DURATION_MS'
        case "$4" in ''|*[!0-9]*) die 'tap X must be a non-negative integer' ;; esac
        case "$5" in ''|*[!0-9]*) die 'tap Y must be a non-negative integer' ;; esac
        case "$6" in ''|*[!0-9]*) die 'tap duration must be a non-negative integer' ;; esac
        [ "$4" -lt 1072 ] && [ "$5" -lt 1448 ] || die 'tap is outside the visible screen'
        [ "$6" -ge 1 ] && [ "$6" -le 10000 ] || die 'tap duration is outside 1..10000 ms'
        printf 'tap %s %s %s\n' "$4" "$5" "$6" > "$2/input/commands.fifo"
        ;;
      down|move)
        [ "$#" -eq 5 ] || die "usage: input-send RUN_ROOT $input_command X Y"
        case "$4" in ''|*[!0-9]*) die 'X must be a non-negative integer' ;; esac
        case "$5" in ''|*[!0-9]*) die 'Y must be a non-negative integer' ;; esac
        [ "$4" -lt 1072 ] && [ "$5" -lt 1448 ] || die 'coordinates are outside the visible screen'
        printf '%s %s %s\n' "$input_command" "$4" "$5" > "$2/input/commands.fifo"
        ;;
      up)
        [ "$#" -eq 3 ] || die 'usage: input-send RUN_ROOT up'
        printf 'up\n' > "$2/input/commands.fifo"
        ;;
      *) die 'input command must be tap, down, move, or up' ;;
    esac
    ;;

  input-stop)
    [ "$#" -eq 2 ] || die 'usage: input-stop RUN_ROOT'
    validate_run_root "$2"
    verify_input_pid "$2"
    printf 'quit\n' > "$2/input/commands.fifo"
    attempt=0
    while [ "$attempt" -lt 30 ] && [ -e "/proc/$input_pid/exe" ]; do
      sleep 0.1
      attempt=$((attempt + 1))
    done
    [ ! -e "/proc/$input_pid/exe" ] || die 'input helper did not stop after quit'
    echo "input_stopped=$input_pid"
    ;;

  collect)
    [ "$#" -eq 2 ] || die 'usage: collect RUN_ROOT'
    validate_run_root "$2"
    mkdir -p "$2/results"
    stamp="$(date +%Y%m%dT%H%M%S)"
    out="$2/results/collect-$stamp"
    mkdir -p "$out"
    date > "$out/date.txt"
    uname -a > "$out/uname.txt"
    df -h /mnt/us /tmp > "$out/df.txt"
    mount > "$out/mounts.txt"
    ps w > "$out/ps.txt"
    process_check > "$out/kindlebrew-processes.txt"
    dmesg 2>/dev/null | tail -200 > "$out/dmesg-tail.txt" || true
    lipc-get-prop com.lab126.powerd preventScreenSaver > "$out/preventScreenSaver.txt" 2>/dev/null || true
    if [ -x "$TARGET/inkchess" ] && [ -x "$TARGET/stockfish" ]; then
      sha256sum "$TARGET/inkchess" "$TARGET/stockfish" > "$out/installed-binaries.sha256"
    fi
    for proc in /proc/[0-9]*; do
      [ -e "$proc/exe" ] || continue
      exe="$(readlink "$proc/exe" 2>/dev/null || true)"
      case "$exe" in "$TARGET/inkchess"|"$TARGET/stockfish") ;; *) continue ;; esac
      pid="${proc#/proc/}"
      cp "$proc/status" "$out/process-$pid.status" 2>/dev/null || true
      tr '\000' ' ' < "$proc/cmdline" > "$out/process-$pid.cmdline" 2>/dev/null || true
      ls -l "$proc/fd" > "$out/process-$pid.fds" 2>/dev/null || true
    done
    if [ -f /mnt/us/kindlebrew-data/chess/save-v1.txt ]; then
      cp /mnt/us/kindlebrew-data/chess/save-v1.txt "$out/save-v1.txt"
      sha256sum "$out/save-v1.txt" > "$out/save-v1.sha256"
    fi
    echo "$out"
    ;;

  process-check)
    process_check
    ;;

  *) die 'commands: inventory install-hooks run status stop kill-stockfish framebuffer input-start input-send input-stop collect process-check' ;;
esac
