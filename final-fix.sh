#!/bin/bash
# Final fix to ensure web UI works correctly

set -e

cd "$(dirname "$0")"

echo "╔══════════════════════════════════════════════════════════════╗"
echo "║    🔧 FINAL FIX FOR WEB UI                                    ║"
echo "╚══════════════════════════════════════════════════════════════╝"
echo ""

# Step 1: Stop any running server
echo "Step 1/4: Stopping any running servers..."
pkill -f llama-server || echo "   No server running"
sleep 1
echo "✅ Done"
echo ""

# Step 2: Rebuild web UI to ensure latest files
echo "Step 2/4: Rebuilding web UI..."
cd vendor/llama-cpp/tools/server/webui
npm run build > /dev/null 2>&1
./scripts/post-build.sh > /dev/null 2>&1
cd ../../../../..
echo "✅ Web UI rebuilt"
echo ""

# Step 3: Update index.html with latest files
echo "Step 3/4: Updating index.html with latest JavaScript files..."
cd vendor/llama-cpp/tools/server/public
ENTRY_DIR="_app/immutable/entry"
START_FILE=$(ls -t "$ENTRY_DIR"/start.*.js 2>/dev/null | head -1 | xargs basename)
APP_FILE=$(ls -t "$ENTRY_DIR"/app.*.js 2>/dev/null | head -1 | xargs basename)

if [ -z "$START_FILE" ] || [ -z "$APP_FILE" ]; then
    echo "❌ Error: JavaScript files not found"
    exit 1
fi

cat > index.html << HTML
<!DOCTYPE html>
<html lang="en">
  <head>
    <meta charset="utf-8" />
    <link rel="icon" href="/_app/immutable/assets/favicon.svg" />
    <meta name="viewport" content="width=device-width, initial-scale=1" />
    <title>Delta</title>
  </head>
  <body data-sveltekit-preload-data="hover">
    <div style="display: contents" id="app"></div>
    <script type="module">
      import { start } from '/_app/immutable/entry/$START_FILE';
      import * as app from '/_app/immutable/entry/$APP_FILE';
      
      start({
        target: document.getElementById('app'),
        paths: {"base":"","assets":""},
        session: {},
        route: true,
        spa: false,
        trailing_slash: "never",
        hydrate: {
          node_ids: [0, 2],
          data: [null, null],
          form: null
        },
        nodes: app.nodes,
        root: app.root
      });
    </script>
  </body>
</html>
HTML

echo "✅ index.html updated with: $START_FILE, $APP_FILE"
cd ../../../../..
echo ""

# Step 4: Verify files
echo "Step 4/4: Verifying files..."
if [ -f "vendor/llama-cpp/tools/server/public/index.html" ]; then
    echo "✅ index.html exists"
else
    echo "❌ index.html missing"
    exit 1
fi

if [ -f "vendor/llama-cpp/tools/server/public/_app/immutable/entry/$START_FILE" ]; then
    echo "✅ $START_FILE exists"
else
    echo "❌ $START_FILE missing"
    exit 1
fi

if [ -f "vendor/llama-cpp/tools/server/public/_app/immutable/entry/$APP_FILE" ]; then
    echo "✅ $APP_FILE exists"
else
    echo "❌ $APP_FILE missing"
    exit 1
fi

echo ""
echo "╔══════════════════════════════════════════════════════════════╗"
echo "║    ✅ ALL FIXES APPLIED                                       ║"
echo "╚══════════════════════════════════════════════════════════════╝"
echo ""
echo "📋 NOW START DELTA:"
echo ""
echo "   delta server"
echo ""
echo "Then open http://localhost:8080 in your browser"
echo ""
echo "If you still see a blank page:"
echo "1. Open Developer Tools (F12 or Cmd+Option+I)"
echo "2. Check Console tab for JavaScript errors"
echo "3. Check Network tab for 404 errors"
echo "4. Try hard refresh: Cmd+Shift+R"
echo ""

