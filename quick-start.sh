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
if [ ! -f "vendor/llama.cpp/tools/server/public/index.html" ]; then
    echo "   ⚠️  Web UI not built, building now..."
    cd vendor/llama.cpp/tools/server/webui
    npm run build > /dev/null 2>&1
    ./scripts/post-build.sh > /dev/null 2>&1
    cd ../../../../..
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
delta server

