#!/usr/bin/env bash
#
# Packages the CLI release archive from an engine build: delta, delta-server, llama-server, the
# web UI and (on macOS) the Metal library, plus a .sha256 next to the archive.
#
# Usage: scripts/package-cli.sh <build-dir> <target-triple> <archive> [tag]
#   e.g. scripts/package-cli.sh build_tauri_release aarch64-apple-darwin out/delta-cli-macos-arm64.tar.gz v1.0.17
set -euo pipefail

BUILD_DIR="$1"
TARGET="$2"
ARCHIVE="$3"
TAG="${4:-}"
REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
VERSION="$(tr -d '[:space:]' < "$REPO_ROOT/version.txt")"

EXT=""
[[ "$TARGET" == *windows* ]] && EXT=".exe"

# Multi-config generators (MSVC) put binaries under Release/.
BIN_ROOT="$BUILD_DIR"
[[ -f "$BUILD_DIR/Release/delta${EXT}" ]] && BIN_ROOT="$BUILD_DIR/Release"

WORK="$(mktemp -d "${TMPDIR:-/tmp}/delta-cli.XXXXXX")"
trap 'rm -rf "$WORK"' EXIT
STAGE="$WORK/delta-cli-${VERSION}"
mkdir -p "$STAGE/bin"

cp "$BIN_ROOT/delta${EXT}" "$BIN_ROOT/delta-server${EXT}" "$STAGE/bin/"

LLAMA_SERVER=""
for candidate in "$BIN_ROOT/llama-server${EXT}" "$BUILD_DIR/bin/llama-server${EXT}" \
  "$BUILD_DIR/bin/Release/llama-server${EXT}" "$BUILD_DIR/engine/vendor/llama.cpp/bin/llama-server${EXT}" \
  "$BUILD_DIR/bin/server${EXT}"; do
  if [[ -f "$candidate" ]]; then
    LLAMA_SERVER="$candidate"
    break
  fi
done
[[ -n "$LLAMA_SERVER" ]] || { echo "ERROR: llama-server not found in $BUILD_DIR" >&2; exit 1; }
cp "$LLAMA_SERVER" "$STAGE/bin/llama-server${EXT}"
if [[ -f "$BUILD_DIR/release-build-info.json" ]]; then
  cp "$BUILD_DIR/release-build-info.json" "$STAGE/BUILD_INFO.json"
fi

[[ -f "$REPO_ROOT/public/index.html" ]] || { echo "ERROR: public/index.html not found; build the web UI first" >&2; exit 1; }
cp -r "$REPO_ROOT/public" "$STAGE/webui"

if [[ "$TARGET" == *apple-darwin ]]; then
  METALLIB="$(find "$BUILD_DIR" -name '*.metallib' 2>/dev/null | head -1)"
  [[ -n "$METALLIB" ]] && cp "$METALLIB" "$STAGE/bin/"
fi

{
  echo "version: ${VERSION}"
  echo "target: ${TARGET}"
  echo "commit: $(git -C "$REPO_ROOT" rev-parse HEAD)"
  echo "tag: ${TAG}"
  echo "date: $(date -u +%Y-%m-%dT%H:%M:%SZ)"
} > "$STAGE/BUILD_INFO"

mkdir -p "$(dirname "$ARCHIVE")"
ARCHIVE="$(cd "$(dirname "$ARCHIVE")" && pwd)/$(basename "$ARCHIVE")"
NAME="$(basename "$ARCHIVE")"
case "$NAME" in
  *.zip)
    if command -v 7z >/dev/null; then
      (cd "$WORK" && 7z a -bd "$ARCHIVE" "$(basename "$STAGE")" >/dev/null)
    else
      (cd "$WORK" && powershell.exe -NoProfile -Command "Compress-Archive -Path '$(basename "$STAGE")' -DestinationPath '$(cygpath -w "$ARCHIVE" 2>/dev/null || echo "$ARCHIVE")'")
    fi
    ;;
  *.tar.gz) tar -C "$WORK" -czf "$ARCHIVE" "$(basename "$STAGE")" ;;
  *) echo "ERROR: archive must end in .zip or .tar.gz: $NAME" >&2; exit 1 ;;
esac

# The checksum file names the archive alone, so `sha256sum -c` works next to it.
(
  cd "$(dirname "$ARCHIVE")"
  if command -v sha256sum >/dev/null; then sha256sum "$NAME"; else shasum -a 256 "$NAME"; fi
) > "$ARCHIVE.sha256"
echo "Packaged $ARCHIVE"
