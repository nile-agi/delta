#!/usr/bin/env bash
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
TARGET_TRIPLE="${1:-}"
WEB_ONLY=0

if [[ "$TARGET_TRIPLE" == "--web-only" ]]; then
  WEB_ONLY=1
  TARGET_TRIPLE=""
fi

fail() {
  echo "ERROR: $*" >&2
  exit 1
}

WEB_INDEX="$REPO_ROOT/public/index.html"
[[ -f "$WEB_INDEX" ]] || fail "public/index.html not found. Run the web UI build before building Tauri."

for marker in "__DELTA_PORT__" "__DELTA_MODEL_API_PORT__" "delta-server-ready" "delta-server-error"; do
  grep -q "$marker" "$WEB_INDEX" || fail "public/index.html is missing '$marker'. Rebuild web/app before packaging."
done

if [[ "$WEB_ONLY" == "1" ]]; then
  echo "Tauri web assets verified."
  exit 0
fi

BINDIR="$REPO_ROOT/src-tauri/binaries"
[[ -d "$BINDIR" ]] || fail "src-tauri/binaries not found. Run scripts/build-sidecars.sh before packaging."

if [[ -n "$TARGET_TRIPLE" ]]; then
  EXE_SUFFIX=""
  if [[ "$TARGET_TRIPLE" == *"windows"* ]]; then
    EXE_SUFFIX=".exe"
  fi

  [[ -f "$BINDIR/delta-server-${TARGET_TRIPLE}${EXE_SUFFIX}" ]] ||
    fail "missing delta-server sidecar for $TARGET_TRIPLE"
  [[ -f "$BINDIR/llama-server-${TARGET_TRIPLE}${EXE_SUFFIX}" ]] ||
    fail "missing llama-server sidecar for $TARGET_TRIPLE"
else
  find "$BINDIR" -maxdepth 1 -type f -name 'delta-server-*' | grep -q . ||
    fail "missing delta-server sidecar in src-tauri/binaries"
  find "$BINDIR" -maxdepth 1 -type f -name 'llama-server-*' | grep -q . ||
    fail "missing llama-server sidecar in src-tauri/binaries"
fi

echo "Tauri release assets verified."
