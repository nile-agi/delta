#!/bin/bash
# Quick rebuild and run script for Delta CLI

set -e

echo "╔══════════════════════════════════════════════════════════════╗"
echo "║    🔧 REBUILDING AND STARTING DELTA CLI                      ║"
echo "╚══════════════════════════════════════════════════════════════╝"
echo ""

# Step 1: Rebuild Delta CLI (if needed)
echo "Step 1/3: Rebuilding Delta CLI..."
cd /Users/suzanodero/Downloads/delta-cli
if [ -d "build_macos" ]; then
    cd build_macos
    cmake .. > /dev/null 2>&1
    make > /dev/null 2>&1
    echo "✅ Delta CLI rebuilt"
else
    echo "⚠️  build_macos directory not found, skipping rebuild"
fi
echo ""

# Step 2: Kill existing server
echo "Step 2/3: Stopping existing server..."
pkill -f llama-server || echo "   No existing server found"
echo "✅ Server stopped"
echo ""

# Step 3: Start Delta server
echo "Step 3/3: Starting Delta server..."
cd /Users/suzanodero/Downloads/delta-cli
delta server

echo ""
echo "╔══════════════════════════════════════════════════════════════╗"
echo "║    ✅ DELTA SERVER STARTED                                     ║"
echo "╚══════════════════════════════════════════════════════════════╝"
echo ""
echo "Open http://localhost:8080 in your browser"
echo ""
echo "If you see a blank page:"
echo "1. Hard refresh: Cmd+Shift+R (Mac) or Ctrl+Shift+R (Windows/Linux)"
echo "2. Check browser console (F12) for errors"
echo "3. Verify server path: ps aux | grep llama-server | grep -v grep"
