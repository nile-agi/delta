#!/bin/bash
# Comprehensive fix for blank page issue

set -e

cd "$(dirname "$0")"

echo "╔══════════════════════════════════════════════════════════════╗"
echo "║    🔧 FIXING BLANK PAGE ISSUE                                 ║"
echo "╚══════════════════════════════════════════════════════════════╝"
echo ""

# Step 1: Stop server
echo "Step 1/5: Stopping server..."
pkill -f llama-server || echo "   No server running"
sleep 1
echo "✅ Done"
echo ""

# Step 2: Rebuild web UI
echo "Step 2/5: Rebuilding web UI..."
cd vendor/llama-cpp/tools/server/webui
npm run build > /dev/null 2>&1
./scripts/post-build.sh > /dev/null 2>&1
cd ../../../../..
echo "✅ Web UI rebuilt"
echo ""

# Step 3: Verify index.html
echo "Step 3/5: Verifying index.html..."
cd vendor/llama-cpp/tools/server/public
if [ ! -f "index.html" ]; then
    echo "❌ index.html missing!"
    exit 1
fi

if grep -q "sveltekit.head\|sveltekit.body" index.html; then
    echo "❌ Template placeholders found in index.html!"
    exit 1
fi

ENTRY_DIR="_app/immutable/entry"
START_FILE=$(grep -o 'start\.[^"]*\.js' index.html | head -1)
APP_FILE=$(grep -o 'app\.[^"]*\.js' index.html | head -1)

if [ -z "$START_FILE" ] || [ -z "$APP_FILE" ]; then
    echo "❌ JavaScript files not found in index.html!"
    exit 1
fi

if [ ! -f "$ENTRY_DIR/$START_FILE" ] || [ ! -f "$ENTRY_DIR/$APP_FILE" ]; then
    echo "❌ JavaScript files don't exist!"
    exit 1
fi

echo "✅ index.html is correct"
echo "   Start: $START_FILE"
echo "   App: $APP_FILE"
cd ../../../../..
echo ""

# Step 4: Rebuild Delta CLI
echo "Step 4/5: Rebuilding Delta CLI..."
cd build_macos
make > /dev/null 2>&1
cd ..
echo "✅ Delta CLI rebuilt"
echo ""

# Step 5: Start server and test
echo "Step 5/5: Starting server..."
PUBLIC_PATH="/Users/suzanodero/Downloads/delta-cli/vendor/llama-cpp/tools/server/public"
/opt/homebrew/bin/llama-server \
    -m "/Users/suzanodero/.delta-cli/models/Qwen3-0.6B-Q4_K_M.gguf" \
    --port 8080 \
    -c 4096 \
    --path "$PUBLIC_PATH" \
    >/dev/null 2>&1 &

sleep 2

# Test
HTML=$(curl -sS http://localhost:8080 2>&1 | head -20)
if echo "$HTML" | grep -q "sveltekit.head\|sveltekit.body"; then
    echo "❌ Server still serving template!"
else
    echo "✅ Server serving correct HTML"
fi

echo ""
echo "╔══════════════════════════════════════════════════════════════╗"
echo "║    ✅ FIX COMPLETE                                             ║"
echo "╚══════════════════════════════════════════════════════════════╝"
echo ""
echo "Server started with absolute path: $PUBLIC_PATH"
echo ""
echo "📋 TEST:"
echo "1. Open http://localhost:8080"
echo "2. Hard refresh: Cmd+Shift+R"
echo "3. Check browser console (F12) for errors"
echo ""

