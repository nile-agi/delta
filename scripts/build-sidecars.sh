#!/usr/bin/env bash
#
# Build the C++ sidecar binaries (delta-server, llama-server) and copy them
# into src-tauri/binaries/ with the target-triple suffix that Tauri expects.
#
# Usage:
#   ./scripts/build-sidecars.sh            # auto-detect platform
#   ./scripts/build-sidecars.sh --release  # Release build (default)
#   ./scripts/build-sidecars.sh --debug    # Debug build

set -euo pipefail

PROJECT_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BINDIR="$PROJECT_ROOT/src-tauri/binaries"
BUILD_TYPE="${1:-Release}"

if [[ "$BUILD_TYPE" == "--debug" ]]; then
    BUILD_TYPE="Debug"
elif [[ "$BUILD_TYPE" == "--release" ]]; then
    BUILD_TYPE="Release"
fi

# Detect target triple
if command -v rustc &>/dev/null; then
    TRIPLE=$(rustc -vV | grep host | cut -d' ' -f2)
else
    # Fallback detection
    ARCH=$(uname -m)
    case "$(uname -s)" in
        Darwin)
            case "$ARCH" in
                arm64) TRIPLE="aarch64-apple-darwin" ;;
                x86_64) TRIPLE="x86_64-apple-darwin" ;;
            esac
            ;;
        Linux)
            TRIPLE="${ARCH}-unknown-linux-gnu"
            ;;
        MINGW*|MSYS*|CYGWIN*)
            TRIPLE="${ARCH}-pc-windows-msvc"
            ;;
    esac
fi

echo "=== Building Delta sidecars ==="
echo "  Platform triple: $TRIPLE"
echo "  Build type: $BUILD_TYPE"
echo ""

# Determine build directory
BUILDDIR="$PROJECT_ROOT/build_tauri_${BUILD_TYPE,,}"
mkdir -p "$BUILDDIR"

# Configure CMake
CMAKE_ARGS=(
    -DCMAKE_BUILD_TYPE="$BUILD_TYPE"
    -DBUILD_TESTS=OFF
)

# Platform-specific flags
case "$(uname -s)" in
    Darwin)
        CMAKE_ARGS+=(-DGGML_METAL=ON)
        ;;
    Linux)
        CMAKE_ARGS+=(-DGGML_METAL=OFF)
        ;;
esac

echo "Configuring CMake..."
cmake -S "$PROJECT_ROOT" -B "$BUILDDIR" "${CMAKE_ARGS[@]}" 2>&1 | tail -20

echo ""
echo "Building..."
NPROC=$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 4)
cmake --build "$BUILDDIR" --config "$BUILD_TYPE" -j "$NPROC" -- delta-server 2>&1 | tail -10

# Also build llama-server from the llama.cpp submodule
cmake --build "$BUILDDIR" --config "$BUILD_TYPE" -j "$NPROC" -- llama-server 2>&1 | tail -10 || \
cmake --build "$BUILDDIR" --config "$BUILD_TYPE" -j "$NPROC" -- server 2>&1 | tail -10 || true

echo ""
echo "Copying binaries to $BINDIR ..."
mkdir -p "$BINDIR"

# Determine exe suffix
EXE_SUFFIX=""
if [[ "$TRIPLE" == *"windows"* ]]; then
    EXE_SUFFIX=".exe"
fi

# Copy delta-server
if [[ -f "$BUILDDIR/delta-server${EXE_SUFFIX}" ]]; then
    cp "$BUILDDIR/delta-server${EXE_SUFFIX}" "$BINDIR/delta-server-${TRIPLE}${EXE_SUFFIX}"
    echo "  delta-server -> delta-server-${TRIPLE}${EXE_SUFFIX}"
else
    echo "  ERROR: delta-server not found in $BUILDDIR"
    exit 1
fi

# Copy llama-server (may be named 'server' or 'llama-server')
LLAMA_SERVER=""
for candidate in \
    "$BUILDDIR/bin/llama-server${EXE_SUFFIX}" \
    "$BUILDDIR/engine/vendor/llama.cpp/bin/llama-server${EXE_SUFFIX}" \
    "$BUILDDIR/engine/vendor/llama.cpp/tools/server/llama-server${EXE_SUFFIX}" \
    "$BUILDDIR/llama-server${EXE_SUFFIX}" \
    "$BUILDDIR/bin/server${EXE_SUFFIX}" \
    "$BUILDDIR/server${EXE_SUFFIX}"; do
    if [[ -f "$candidate" ]]; then
        LLAMA_SERVER="$candidate"
        break
    fi
done

if [[ -n "$LLAMA_SERVER" ]]; then
    cp "$LLAMA_SERVER" "$BINDIR/llama-server-${TRIPLE}${EXE_SUFFIX}"
    echo "  llama-server -> llama-server-${TRIPLE}${EXE_SUFFIX}"
else
    echo "  WARNING: llama-server not found, searching..."
    FOUND=$(find "$BUILDDIR" -name "llama-server${EXE_SUFFIX}" -o -name "server${EXE_SUFFIX}" 2>/dev/null | head -1)
    if [[ -n "$FOUND" ]]; then
        cp "$FOUND" "$BINDIR/llama-server-${TRIPLE}${EXE_SUFFIX}"
        echo "  llama-server -> llama-server-${TRIPLE}${EXE_SUFFIX} (from $FOUND)"
    else
        echo "  ERROR: llama-server binary not found anywhere in build tree"
        exit 1
    fi
fi

# Copy Metal shader library on macOS
if [[ "$(uname -s)" == "Darwin" ]]; then
    METALLIB=$(find "$BUILDDIR" -name "*.metallib" 2>/dev/null | head -1)
    if [[ -n "$METALLIB" ]]; then
        cp "$METALLIB" "$BINDIR/"
        echo "  $(basename "$METALLIB") -> binaries/"
    fi
fi

echo ""
echo "=== Sidecar build complete ==="
ls -lh "$BINDIR/"
