#!/bin/bash
# Quick rebuild and run script for Delta CLI
# Usage: ./rebuild-and-run.sh [build_dir]

set -e

# Get project root (directory containing this script)
PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

echo "╔══════════════════════════════════════════════════════════════╗"
echo "║    🔧 REBUILDING AND STARTING DELTA CLI                      ║"
echo "╚══════════════════════════════════════════════════════════════╝"
echo ""

# Step 1: Update submodules
echo "Step 1/4: Updating llama.cpp submodule..."
cd "$PROJECT_ROOT"
git submodule update --init --recursive
echo "✅ Submodules updated"
echo ""

# Step 2: Rebuild Delta CLI
echo "Step 2/4: Rebuilding Delta CLI..."
BUILD_DIR="${1:-build_macos}"
if [ -d "$BUILD_DIR" ]; then
    cd "$BUILD_DIR"
    cmake .. > /dev/null 2>&1
    make -j$(sysctl -n hw.ncpu 2>/dev/null || nproc 2>/dev/null || echo 4) > /dev/null 2>&1
    echo "✅ Delta CLI rebuilt"
else
    echo "⚠️  $BUILD_DIR directory not found, creating build..."
    mkdir -p "$BUILD_DIR"
    cd "$BUILD_DIR"
    cmake .. > /dev/null 2>&1
    make -j$(sysctl -n hw.ncpu 2>/dev/null || nproc 2>/dev/null || echo 4) > /dev/null 2>&1
    echo "✅ Delta CLI built"
fi
echo ""

# Step 3: Kill existing server
echo "Step 3/4: Stopping existing server..."
pkill -f delta-server || pkill -f llama-server || echo "   No existing server found"
echo "✅ Server stopped"
echo ""

# Step 4: Start Delta server
echo "Step 4/4: Starting Delta server..."
cd "$PROJECT_ROOT"
if [ -f "$BUILD_DIR/delta" ]; then
    "$BUILD_DIR/delta" server
else
    echo "❌ Delta binary not found in $BUILD_DIR"
    exit 1
fi

echo ""
echo "╔══════════════════════════════════════════════════════════════╗"
echo "║    ✅ DELTA SERVER STARTED                                     ║"
echo "╚══════════════════════════════════════════════════════════════╝"
echo ""
echo "Open http://localhost:8080 in your browser"
