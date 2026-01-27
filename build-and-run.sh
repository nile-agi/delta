#!/bin/bash
# Complete build and run script for Delta CLI

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

echo "╔══════════════════════════════════════════════════════════════╗"
echo "║    🚀 DELTA CLI - BUILD AND RUN                              ║"
echo "╚══════════════════════════════════════════════════════════════╝"
echo ""

# Step 1: Stop any running server
echo "Step 1/5: Stopping any existing server..."
pkill -f llama-server || echo "   No server running"
sleep 1
echo "✅ Done"
echo ""

# Step 2: Rebuild web UI
echo "Step 2/5: Rebuilding web UI..."
cd vendor/llama.cpp/tools/server/webui
if [ ! -d "node_modules" ]; then
    echo "   Installing dependencies..."
    npm install > /dev/null 2>&1
fi
echo "   Building SvelteKit app..."
npm run build > /dev/null 2>&1
echo "   Deploying to public directory..."
./scripts/post-build.sh > /dev/null 2>&1
cd "$SCRIPT_DIR"
echo "✅ Web UI rebuilt"
echo ""

# Step 3: Build Delta CLI
echo "Step 3/5: Building Delta CLI..."
if [ ! -d "build_macos" ]; then
    echo "   Creating build directory..."
    mkdir -p build_macos
    cd build_macos
    cmake .. -DCMAKE_BUILD_TYPE=Release > /dev/null 2>&1
    cd ..
fi
cd build_macos
echo "   Compiling..."
make > /dev/null 2>&1
cd ..
echo "✅ Delta CLI built"
echo ""

# Step 4: Install Delta CLI (system-wide)
echo "Step 4/5: Installing Delta CLI system-wide..."
cd build_macos
if [ "$EUID" -ne 0 ]; then
    echo "   Using sudo for system-wide installation..."
    sudo make install > /dev/null 2>&1
else
    make install > /dev/null 2>&1
fi
cd ..
echo "✅ Delta CLI installed"
echo ""

# Step 5: Start Delta server
echo "Step 5/5: Starting Delta server..."
cd "$SCRIPT_DIR"
pkill -f llama-server || true
sleep 1
delta server > /dev/null 2>&1 &
sleep 2
echo "✅ Delta server started"
echo ""

echo "╔══════════════════════════════════════════════════════════════╗"
echo "║    ✅ DELTA CLI IS READY!                                     ║"
echo "╚══════════════════════════════════════════════════════════════╝"
echo ""
echo "🌐 Web UI: http://localhost:2275"
echo ""
echo "📋 Verification:"
echo "   Server path:"
ps aux | grep llama-server | grep -- '--path' | grep -v grep || echo "   (Server starting...)"
echo ""
echo "💡 Tips:"
echo "   - If you see a blank page, hard refresh: Cmd+Shift+R"
echo "   - Check browser console (F12) for any errors"
echo "   - To stop server: pkill -f llama-server"
echo ""

