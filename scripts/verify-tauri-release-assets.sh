#!/usr/bin/env bash
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
TARGET_TRIPLE="${1:-}"
WEB_ONLY=0

# shellcheck source=appimage-graphics-libs.sh
source "$REPO_ROOT/scripts/appimage-graphics-libs.sh"

fail() {
  echo "ERROR: $*" >&2
  exit 1
}

if [[ "$TARGET_TRIPLE" == "--appimage" ]]; then
  [[ $# -eq 2 ]] || fail "usage: $0 --appimage <AppImage>"
  APPIMAGE="$2"
  [[ -f "$APPIMAGE" ]] || fail "AppImage not found: $APPIMAGE"
  [[ -x "$APPIMAGE" ]] || fail "AppImage is not executable: $APPIMAGE"

  APPIMAGE_DIR="$(cd "$(dirname "$APPIMAGE")" && pwd)"
  APPIMAGE="$APPIMAGE_DIR/$(basename "$APPIMAGE")"
  EXTRACT_DIR="$(mktemp -d "${TMPDIR:-/tmp}/delta-appimage-verify.XXXXXX")"
  trap 'rm -rf "$EXTRACT_DIR"' EXIT
  (
    cd "$EXTRACT_DIR"
    "$APPIMAGE" --appimage-extract >/dev/null
  )
  assert_valid_appimage_appdir "$EXTRACT_DIR/squashfs-root" ||
    fail "AppImage validation failed: $APPIMAGE"
  echo "Linux AppImage verified: $APPIMAGE"
  exit 0
fi

if [[ "$TARGET_TRIPLE" == "--web-only" ]]; then
  WEB_ONLY=1
  TARGET_TRIPLE=""
fi

WEB_INDEX="$REPO_ROOT/public/index.html"
[[ -f "$WEB_INDEX" ]] || fail "public/index.html not found. Run the web UI build before building Tauri."

[[ -d "$REPO_ROOT/public/_app" ]] || fail "public/_app not found. The SvelteKit assets are missing; run the web UI build before building Tauri."
find "$REPO_ROOT/public/_app" -type f -print -quit | grep -q . ||
  fail "public/_app is empty. The SvelteKit assets are missing; run the web UI build before building Tauri."
grep -q 'id="app-loading"' "$WEB_INDEX" || fail "public/index.html is missing the startup loading surface. Rebuild web/app before packaging."

# Startup and reporting run before Svelte mounts and are copied as static assets.
# Check both the HTML references and their deployed files, rather than expecting inline code.
for helper in startup-guard.js failure-report.js; do
  grep -Eq "<script[^>]*src=[\"'][^\"']*/${helper//./\\.}[\"']" "$WEB_INDEX" ||
    fail "public/index.html does not reference '$helper'. Rebuild web/app before packaging."
  [[ -s "$REPO_ROOT/public/$helper" ]] ||
    fail "public/$helper is missing or empty. Rebuild web/app before packaging."
done

for marker in "__DELTA_PORT__" "__DELTA_MODEL_API_PORT__" "delta-server-ready" "delta-server-error" "__DELTA_STARTUP__"; do
  grep -q "$marker" "$REPO_ROOT/public/startup-guard.js" || fail "public/startup-guard.js is missing '$marker'. Rebuild web/app before packaging."
done
grep -q '__DELTA_DIAGNOSTICS__' "$REPO_ROOT/public/failure-report.js" ||
  fail "public/failure-report.js is missing diagnostic reporting. Rebuild web/app before packaging."

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
  if [[ "$TARGET_TRIPLE" != *apple-darwin ]]; then
    [[ -f "$BINDIR/llama-server-v3-${TARGET_TRIPLE}${EXE_SUFFIX}" ]] ||
      fail "missing AVX2 llama-server-v3 sidecar for $TARGET_TRIPLE"
  fi
else
  find "$BINDIR" -maxdepth 1 -type f -name 'delta-server-*' | grep -q . ||
    fail "missing delta-server sidecar in src-tauri/binaries"
  find "$BINDIR" -maxdepth 1 -type f -name 'llama-server-*' | grep -q . ||
    fail "missing llama-server sidecar in src-tauri/binaries"
fi

echo "Tauri release assets verified."
