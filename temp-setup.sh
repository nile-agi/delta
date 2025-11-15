#!/bin/bash
# Temporary setup script for Delta CLI - no system installation required
# Usage: ./temp-setup.sh

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

echo "╔══════════════════════════════════════════════════════════════╗"
echo "║    🚀 DELTA CLI - TEMPORARY SETUP (NO INSTALL)               ║"
echo "╚══════════════════════════════════════════════════════════════╝"
echo ""

# Step 1: Update submodules
echo "Step 1/5: Updating submodules..."
git submodule update --init --recursive
echo "✅ Submodules updated"
echo ""

# Step 2: Build Web UI
echo "Step 2/5: Building Web UI..."
if [ ! -d "assets/node_modules" ]; then
    echo "   Installing npm dependencies..."
    cd assets
    npm install > /dev/null 2>&1
    cd ..
fi

cd assets
echo "   Building SvelteKit app..."
npm run build > /dev/null 2>&1
cd ..
echo "✅ Web UI built (output in public/)"
echo ""

# Step 3: Clean and create build directory
echo "Step 3/5: Setting up build directory..."
rm -rf build_macos
mkdir -p build_macos
cd build_macos
echo "✅ Build directory ready"
echo ""

# Step 4: Configure and build Delta CLI
echo "Step 4/5: Building Delta CLI..."
CC=/usr/bin/clang CXX=/usr/bin/clang++ cmake .. \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=/usr/local \
    -DGGML_METAL=ON \
    -DBUILD_SERVER=ON

echo "   Compiling (this may take a few minutes)..."
cmake --build . --config Release -j$(sysctl -n hw.ncpu) > /dev/null 2>&1
cd ..
echo "✅ Delta CLI built"
echo ""

# Step 5: Verify binaries
echo "Step 5/5: Verifying build..."
if [ ! -f "build_macos/delta" ]; then
    echo "❌ Error: delta binary not found!"
    exit 1
fi
if [ ! -f "build_macos/delta-server" ]; then
    echo "❌ Error: delta-server binary not found!"
    exit 1
fi
echo "✅ Binaries ready"
echo ""

echo "╔══════════════════════════════════════════════════════════════╗"
echo "║    ✅ SETUP COMPLETE!                                         ║"
echo "╚══════════════════════════════════════════════════════════════╝"
echo ""
echo "To use Delta CLI temporarily (no installation):"
echo ""
echo "  # Run Delta CLI"
echo "  ./build_macos/delta"
echo ""
echo "  # Start the server with web UI"
echo "  ./build_macos/delta server"
echo ""
echo "  # Or use the quick start script"
echo "  ./quick-start.sh"
echo ""
echo "The web UI will be available at: http://localhost:8080"
echo ""


