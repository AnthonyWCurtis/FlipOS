#!/bin/sh
set -u
ROOT=/mnt/SDCARD/flipos-dev
BIN="$ROOT/bin/flip-perf-capture"
LOGDIR="$ROOT/logs/playtest"
PIDFILE="$ROOT/state/playtest-logger.pid"
mkdir -p "$LOGDIR" "$ROOT/state"
if [ -f "$PIDFILE" ]; then
    oldpid=$(cat "$PIDFILE" 2>/dev/null || true)
    if [ -n "${oldpid:-}" ] && kill -0 "$oldpid" 2>/dev/null; then
        exit 0
    fi
fi
stamp=$(date +%Y%m%d-%H%M%S)
out="$LOGDIR/playtest-$stamp.csv"
nohup "$BIN" playtest 172800 60 "$out" </dev/null >/dev/null 2>&1 &
pid=$!
echo "$pid" > "$PIDFILE"
echo "PLAYTEST_LOGGER_START pid=$pid csv=$out interval=60s $(date)" >> "$ROOT/logs/frontend.log"
exit 0
