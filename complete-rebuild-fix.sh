#!/bin/bash
# Complete rebuild and fix for blank page issue

set -e

cd "$(dirname "$0")"

echo "╔══════════════════════════════════════════════════════════════╗"
echo "║    🔧 COMPLETE REBUILD AND FIX                                ║"
echo "╚══════════════════════════════════════════════════════════════╝"
echo ""

# Step 1: Stop server
echo "Step 1/6: Stopping server..."
pkill -f llama-server || echo "   No server running"
sleep 2
echo "✅ Done"
echo ""

# Step 2: Clean web UI build
echo "Step 2/6: Cleaning web UI build..."
cd vendor/llama.cpp/tools/server/webui
rm -rf .svelte-kit node_modules/.vite build 2>/dev/null || true
cd ../../../../..
echo "✅ Cleaned"
echo ""

# Step 3: Rebuild web UI from scratch
echo "Step 3/6: Rebuilding web UI from scratch..."
cd vendor/llama.cpp/tools/server/webui
if [ ! -d "node_modules" ]; then
    echo "   Installing dependencies..."
    npm install
fi
echo "   Building SvelteKit app..."
npm run build
echo "   Deploying to public..."
./scripts/post-build.sh
cd ../../../../..
echo "✅ Web UI rebuilt"
echo ""

# Step 4: Verify index.html
echo "Step 4/6: Verifying index.html..."
cd vendor/llama.cpp/tools/server/public
if [ ! -f "index.html" ]; then
    echo "❌ index.html missing after build!"
    exit 1
fi

# Check for template placeholders
if grep -q "sveltekit.head\|sveltekit.body\|%sveltekit" index.html; then
    echo "❌ Template placeholders found in index.html!"
    echo "   This means post-build.sh didn't work correctly"
    exit 1
fi

# Verify JavaScript files exist
ENTRY_DIR="_app/immutable/entry"
START_FILE=$(grep -o 'start\.[^"]*\.js' index.html | head -1)
APP_FILE=$(grep -o 'app\.[^"]*\.js' index.html | head -1)

if [ -z "$START_FILE" ] || [ -z "$APP_FILE" ]; then
    echo "❌ JavaScript files not found in index.html!"
    exit 1
fi

if [ ! -f "$ENTRY_DIR/$START_FILE" ] || [ ! -f "$ENTRY_DIR/$APP_FILE" ]; then
    echo "❌ JavaScript files don't exist!"
    echo "   Looking for: $ENTRY_DIR/$START_FILE and $ENTRY_DIR/$APP_FILE"
    exit 1
fi

echo "✅ index.html verified"
echo "   Start: $START_FILE"
echo "   App: $APP_FILE"
cd ../../../../..
echo ""

# Step 5: Clean and rebuild Delta CLI
echo "Step 5/6: Rebuilding Delta CLI..."
cd build_macos
echo "   Cleaning..."
make clean > /dev/null 2>&1 || true
echo "   Configuring..."
cmake .. -DCMAKE_BUILD_TYPE=Release > /dev/null 2>&1
echo "   Compiling..."
make > /dev/null 2>&1
cd ..
echo "✅ Delta CLI rebuilt"
echo ""

# Step 6: Reinstall Delta CLI
echo "Step 6/6: Reinstalling Delta CLI..."
cd build_macos
if [ "$EUID" -ne 0 ]; then
    echo "   Using sudo for system-wide installation..."
    sudo make install > /dev/null 2>&1
else
    make install > /dev/null 2>&1
fi
cd ..
echo "✅ Delta CLI reinstalled"
echo ""

# Start server with absolute path for testing
echo "Starting server for verification..."
PUBLIC_PATH="$(pwd)/vendor/llama.cpp/tools/server/public"
MODEL_PATH="$HOME/.delta-cli/models/Qwen3-0.6B-Q4_K_M.gguf"

if [ ! -f "$MODEL_PATH" ]; then
    echo "⚠️  Model not found at $MODEL_PATH"
    echo "   Server will start but may not work without a model"
fi

/opt/homebrew/bin/llama-server \
    -m "$MODEL_PATH" \
    --port 8080 \
    -c 4096 \
    --path "$PUBLIC_PATH" \
    >/dev/null 2>&1 &

sleep 3

# Verify server is running
if ps aux | grep -q "[l]lama-server.*--path.*$PUBLIC_PATH"; then
    echo "✅ Server started with correct path"
else
    echo "⚠️  Server may not have started correctly"
fi

echo ""
echo "╔══════════════════════════════════════════════════════════════╗"
echo "║    ✅ COMPLETE REBUILD FINISHED                                ║"
echo "╚══════════════════════════════════════════════════════════════╝"
echo ""
echo "🌐 Open: http://localhost:8080"
echo ""
echo "📋 Verification commands:"
echo "   ps aux | grep llama-server | grep -- '--path'"
echo "   curl -sS http://localhost:8080 | head -20"
echo ""
echo "💡 If still blank page:"
echo "   1. Hard refresh: Cmd+Shift+R"
echo "   2. Open browser console (F12)"
echo "   3. Check Console tab for errors"
echo "   4. Check Network tab for 404s"
echo "   5. Share the errors with me"
echo ""

