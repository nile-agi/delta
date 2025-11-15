#!/bin/bash
# Quick start script for Delta CLI

echo "╔══════════════════════════════════════════════════════════════╗"
echo "║    🚀 DELTA CLI QUICK START                                    ║"
echo "╚══════════════════════════════════════════════════════════════╝"
echo ""

# Stop any running server
echo "1. Stopping any running servers..."
pkill -f llama-server || echo "   No server running"
sleep 1

# Check if web UI is built
echo ""
echo "2. Checking web UI..."
if [ ! -f "public/index.html.gz" ]; then
    echo "   ⚠️  Web UI not built, building now..."
    if [ ! -d "assets/node_modules" ]; then
        echo "   Installing npm dependencies..."
        cd assets
        npm install > /dev/null 2>&1
        cd ..
    fi
    cd assets
    npm run build > /dev/null 2>&1
    cd ..
    echo "   ✅ Web UI built"
else
    echo "   ✅ Web UI already built"
fi

# Start Delta
echo ""
echo "3. Starting Delta CLI..."
echo "   Opening http://localhost:8080 in your browser..."
echo ""

cd "$(dirname "$0")"

# Check if delta is installed, otherwise use build directory
if command -v delta >/dev/null 2>&1; then
delta server
elif [ -f "build_macos/delta" ]; then
    ./build_macos/delta server
else
    echo "❌ Error: delta binary not found!"
    echo "   Please run: ./temp-setup.sh first"
    exit 1
fi

