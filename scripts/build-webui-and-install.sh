#!/usr/bin/env bash
# Build the Delta web UI from app/ and then build + install Delta CLI.
# Use this when you have local changes in app/ so that reinstalling Delta
# uses YOUR built web UI (not an old or upstream copy).
#
# Usage: from repo root, run:
#   ./scripts/build-webui-and-install.sh [install-prefix]
# Example:
#   ./scripts/build-webui-and-install.sh              # install to /usr/local
#   ./scripts/build-webui-and-install.sh /opt/homebrew # install to Homebrew prefix
set -e

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$REPO_ROOT"

PREFIX="${1:-/usr/local}"
BUILD_DIR="${BUILD_DIR:-build}"

echo "=== 1. Building web UI from web/app/ (output -> public/) ==="
if [[ ! -f web/app/package.json ]]; then
  echo "Error: web/app/package.json not found. Run from Delta repo root." >&2
  exit 1
fi
if ! command -v pnpm &>/dev/null; then
  echo "Error: pnpm not found. Install pnpm: https://pnpm.io/installation" >&2
  exit 1
fi
cd web/app
if [[ ! -d node_modules ]]; then
  echo "Installing dependencies..."
  pnpm install
fi
pnpm run build
cd "$REPO_ROOT"

if [[ ! -f public/index.html ]]; then
  echo "Error: Web UI build did not produce public/index.html" >&2
  exit 1
fi
echo "Web UI built successfully."

echo ""
echo "=== 2. Configuring and building Delta CLI (prefix=$PREFIX) ==="
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"
# Force web UI rebuild is not needed here since we just built it above
cmake .. \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX="$PREFIX" \
  -DGGML_METAL="${GGML_METAL:-ON}" \
  -DBUILD_SERVER=ON \
  -DUSE_CURL=ON
cmake --build . --config Release -j"$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)"

echo ""
echo "=== 3. Installing to $PREFIX ==="
echo "This may ask for your password (sudo)."
cmake --install . --prefix "$PREFIX"

echo ""
echo "Done. Delta CLI and the web UI (from your web/app/) are installed to $PREFIX."
echo "  Binaries: $PREFIX/bin/delta, $PREFIX/bin/delta-server"
echo "  Web UI:   $PREFIX/share/delta-cli/webui"
echo "Run: delta --version  (or $PREFIX/bin/delta --version)"
