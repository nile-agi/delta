#!/usr/bin/env bash
#
# Copies a Tauri build's installers into <out-dir>, named as the release has always published them
# (the macOS app also as Delta_<arch>.app.tar.gz).
#
# Usage: scripts/collect-tauri-bundles.sh <target-triple> <out-dir>
set -euo pipefail

TARGET="$1"
OUT="$2"
REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
ROOT="$REPO_ROOT/src-tauri/target/$TARGET/release/bundle"
[[ -d "$ROOT" ]] || ROOT="$REPO_ROOT/src-tauri/target/release/bundle"
[[ -d "$ROOT" ]] || { echo "ERROR: no Tauri bundle directory for $TARGET" >&2; exit 1; }
mkdir -p "$OUT"

shopt -s nullglob
copy() {
  local found=("$@")
  [[ ${#found[@]} -gt 0 ]] || { echo "ERROR: missing bundle: $*" >&2; exit 1; }
  cp "${found[@]}" "$OUT/"
}

case "$TARGET" in
  *apple-darwin)
    copy "$ROOT"/dmg/*.dmg
    apps=("$ROOT"/macos/*.app)
    [[ ${#apps[@]} -eq 1 ]] || { echo "ERROR: expected one .app in $ROOT/macos" >&2; exit 1; }
    arch=x64
    [[ "$TARGET" == aarch64-* ]] && arch=aarch64
    tar -C "$ROOT/macos" -czf "$OUT/$(basename "${apps[0]}" .app)_${arch}.app.tar.gz" "$(basename "${apps[0]}")"
    ;;
  *linux*)
    copy "$ROOT"/deb/*.deb
    copy "$ROOT"/rpm/*.rpm
    copy "$ROOT"/appimage/*.AppImage
    ;;
  *windows*)
    copy "$ROOT"/msi/*.msi
    copy "$ROOT"/nsis/*-setup.exe
    ;;
  *) echo "ERROR: unknown target $TARGET" >&2; exit 1 ;;
esac
ls -l "$OUT"
