#!/usr/bin/env bash
# Starts a packaged delta-server with an empty models folder and fails unless its API answers.
# llama-server's own check never runs delta-server, so a crash on start would otherwise ship.
#
# Usage: scripts/smoke-delta-server.sh <path-to-delta-server>
set -euo pipefail

SERVER="$1"
PORT=18180
WORK="$(mktemp -d "${TMPDIR:-/tmp}/delta-smoke.XXXXXX")"
mkdir -p "$WORK/models"

"$SERVER" --models-dir "$WORK/models" --port "$PORT" --model-api-port $((PORT + 1)) >"$WORK/log" 2>&1 &
PID=$!
# shellcheck disable=SC2329 # run by the EXIT trap
cleanup() {
  kill "$PID" 2>/dev/null || true
  wait "$PID" 2>/dev/null || true
  pkill -f "llama-server.*--port $PORT" 2>/dev/null || true
  rm -rf "$WORK"
}
trap cleanup EXIT

for _ in $(seq 60); do
  if ! kill -0 "$PID" 2>/dev/null; then
    wait "$PID" && code=0 || code=$?
    echo "ERROR: delta-server exited during startup (code $code)" >&2
    cat "$WORK/log" >&2
    exit 1
  fi
  if curl -fs -o /dev/null "http://127.0.0.1:$((PORT + 1))/v1/models"; then
    echo "delta-server started and answered: $(basename "$SERVER")"
    exit 0
  fi
  sleep 1
done
echo "ERROR: delta-server did not answer within 60s" >&2
cat "$WORK/log" >&2
exit 1
