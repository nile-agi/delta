#!/bin/bash
# Test script to diagnose blank page issue

echo "╔══════════════════════════════════════════════════════════════╗"
echo "║    🔍 TESTING WEB UI                                          ║"
echo "╚══════════════════════════════════════════════════════════════╝"
echo ""

# Stop any running server
pkill -f llama-server
sleep 1

# Check if files exist
echo "1. Checking if index.html exists..."
if [ -f "vendor/llama-cpp/tools/server/public/index.html" ]; then
    echo "   ✅ index.html exists"
else
    echo "   ❌ index.html MISSING"
    exit 1
fi

# Get JavaScript file names from index.html
START_FILE=$(grep -o 'start\.[^"]*\.js' vendor/llama-cpp/tools/server/public/index.html | head -1)
APP_FILE=$(grep -o 'app\.[^"]*\.js' vendor/llama-cpp/tools/server/public/index.html | head -1)

echo ""
echo "2. Checking JavaScript files referenced in index.html..."
echo "   Start file: $START_FILE"
echo "   App file: $APP_FILE"

if [ -f "vendor/llama-cpp/tools/server/public/_app/immutable/entry/$START_FILE" ]; then
    echo "   ✅ $START_FILE exists"
else
    echo "   ❌ $START_FILE MISSING"
fi

if [ -f "vendor/llama-cpp/tools/server/public/_app/immutable/entry/$APP_FILE" ]; then
    echo "   ✅ $APP_FILE exists"
else
    echo "   ❌ $APP_FILE MISSING"
fi

# Start server with absolute path
PUBLIC_PATH="/Users/suzanodero/Downloads/delta-cli/vendor/llama-cpp/tools/server/public"
echo ""
echo "3. Starting server with absolute path..."
echo "   Path: $PUBLIC_PATH"

/opt/homebrew/bin/llama-server \
    -m "/Users/suzanodero/.delta-cli/models/Qwen3-0.6B-Q4_K_M.gguf" \
    --port 8080 \
    -c 4096 \
    --path "$PUBLIC_PATH" \
    >/dev/null 2>&1 &

sleep 2

# Test if server is running
if pgrep -f llama-server > /dev/null; then
    echo "   ✅ Server started"
else
    echo "   ❌ Server failed to start"
    exit 1
fi

# Test index.html
echo ""
echo "4. Testing index.html..."
HTML=$(curl -sS http://localhost:8080 2>&1 | head -20)
if echo "$HTML" | grep -q "Delta"; then
    echo "   ✅ index.html served correctly (contains 'Delta')"
else
    echo "   ❌ index.html issue:"
    echo "$HTML" | head -10
fi

# Test JavaScript files
echo ""
echo "5. Testing JavaScript files..."
START_STATUS=$(curl -sI http://localhost:8080/_app/immutable/entry/$START_FILE 2>&1 | head -1)
APP_STATUS=$(curl -sI http://localhost:8080/_app/immutable/entry/$APP_FILE 2>&1 | head -1)

if echo "$START_STATUS" | grep -q "200"; then
    echo "   ✅ $START_FILE accessible (200 OK)"
else
    echo "   ❌ $START_FILE NOT accessible: $START_STATUS"
fi

if echo "$APP_STATUS" | grep -q "200"; then
    echo "   ✅ $APP_FILE accessible (200 OK)"
else
    echo "   ❌ $APP_FILE NOT accessible: $APP_STATUS"
fi

echo ""
echo "╔══════════════════════════════════════════════════════════════╗"
echo "║    📋 NEXT STEPS                                              ║"
echo "╚══════════════════════════════════════════════════════════════╝"
echo ""
echo "1. Open http://localhost:8080 in your browser"
echo "2. Open Developer Tools (F12 or Cmd+Option+I)"
echo "3. Check Console tab for JavaScript errors"
echo "4. Check Network tab for 404 errors"
echo "5. Try hard refresh: Cmd+Shift+R"
echo ""
echo "If you see errors, share them with me!"

