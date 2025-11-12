#!/bin/bash
# Comprehensive diagnosis of blank page issue

echo "╔══════════════════════════════════════════════════════════════╗"
echo "║    🔍 DIAGNOSING BLANK PAGE ISSUE                             ║"
echo "╚══════════════════════════════════════════════════════════════╝"
echo ""

GPT_UI_DIR="/Users/suzanodero/Downloads/gpt-ui-main"

# Step 1: Check if gpt-ui-main is built
echo "Step 1/5: Checking gpt-ui-main build..."
if [ -d "$GPT_UI_DIR/build" ]; then
    echo "   ✅ build/ directory exists"
    BUILD_DIR="$GPT_UI_DIR/build"
elif [ -d "$GPT_UI_DIR/public" ]; then
    echo "   ✅ public/ directory exists"
    BUILD_DIR="$GPT_UI_DIR/public"
else
    echo "   ❌ Neither build/ nor public/ directory found"
    echo "   Run: ./build-gpt-ui.sh"
    exit 1
fi

# Step 2: Check index.html
echo ""
echo "Step 2/5: Checking index.html..."
if [ -f "$BUILD_DIR/index.html" ]; then
    echo "   ✅ index.html exists"
    if grep -q "<script" "$BUILD_DIR/index.html"; then
        echo "   ✅ Contains script tags"
    else
        echo "   ❌ Missing script tags"
    fi
else
    echo "   ❌ index.html not found"
    exit 1
fi

# Step 3: Check _app directory
echo ""
echo "Step 3/5: Checking _app directory..."
if [ -d "$BUILD_DIR/_app" ]; then
    echo "   ✅ _app/ directory exists"
    ENTRY_COUNT=$(find "$BUILD_DIR/_app/immutable/entry" -name "*.js" 2>/dev/null | wc -l | tr -d ' ')
    echo "   ✅ Found $ENTRY_COUNT entry JavaScript files"
else
    echo "   ❌ _app/ directory not found"
    exit 1
fi

# Step 4: Verify file paths in index.html match actual files
echo ""
echo "Step 4/5: Verifying file paths..."
START_FILE=$(grep -o 'start\.[^"]*\.js' "$BUILD_DIR/index.html" | head -1)
APP_FILE=$(grep -o 'app\.[^"]*\.js' "$BUILD_DIR/index.html" | head -1)

if [ -n "$START_FILE" ]; then
    if [ -f "$BUILD_DIR/_app/immutable/entry/$START_FILE" ]; then
        echo "   ✅ $START_FILE exists"
    else
        echo "   ❌ $START_FILE NOT FOUND"
    fi
fi

if [ -n "$APP_FILE" ]; then
    if [ -f "$BUILD_DIR/_app/immutable/entry/$APP_FILE" ]; then
        echo "   ✅ $APP_FILE exists"
    else
        echo "   ❌ $APP_FILE NOT FOUND"
    fi
fi

# Step 5: Check if server is running and using correct path
echo ""
echo "Step 5/5: Checking server configuration..."
if pgrep -f "llama-server" > /dev/null; then
    echo "   ✅ llama-server is running"
    echo "   Checking command line arguments..."
    ps aux | grep "llama-server" | grep -v grep | grep -o "--path [^ ]*" | head -1
else
    echo "   ⚠️  llama-server is not running"
    echo "   Start it with: delta server"
fi

echo ""
echo "╔══════════════════════════════════════════════════════════════╗"
echo "║    📋 DIAGNOSIS COMPLETE                                      ║"
echo "╚══════════════════════════════════════════════════════════════╝"
echo ""
echo "If all checks passed but you still see a blank page:"
echo "1. Open browser Developer Tools (F12)"
echo "2. Check Console tab for errors"
echo "3. Check Network tab for 404 errors"
echo "4. Try hard refresh: Cmd+Shift+R (Mac) or Ctrl+Shift+R (Windows/Linux)"
echo "5. Try incognito/private window"
