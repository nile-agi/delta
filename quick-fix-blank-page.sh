#!/bin/bash
# Quick fix for blank page - rebuilds web UI and restarts server

set -e

cd "$(dirname "$0")"

echo "🔧 Quick fix for blank page..."
echo ""

# Stop server
pkill -f llama-server || true
sleep 1

# Rebuild web UI
echo "Rebuilding web UI..."
cd vendor/llama.cpp/tools/server/webui
npm run build > /dev/null 2>&1
./scripts/post-build.sh > /dev/null 2>&1
cd ../../../../..

# Start server
echo "Starting server..."
delta server > /dev/null 2>&1 &
sleep 2

echo ""
echo "✅ Done! Open http://localhost:8080"
echo "   Hard refresh: Cmd+Shift+R"
echo ""

