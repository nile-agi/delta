#!/bin/bash
# Rebuild web UI and fix index.html

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$SCRIPT_DIR"

echo "╔══════════════════════════════════════════════════════════════╗"
echo "║    🔧 REBUILDING WEB UI                                       ║"
echo "╚══════════════════════════════════════════════════════════════╝"
echo ""

# Step 1: Rebuild web UI
echo "Step 1/3: Building web UI..."
cd vendor/llama-cpp/tools/server/webui

if [ ! -f "package.json" ]; then
    echo "❌ Error: package.json not found in webui directory"
    exit 1
fi

npm run build 2>&1 | tail -10
echo "✅ Build complete"
echo ""

# Step 2: Copy to public directory
echo "Step 2/3: Copying build output to public directory..."
cd "$SCRIPT_DIR"
./vendor/llama-cpp/tools/server/webui/scripts/post-build.sh
echo "✅ Files copied"
echo ""

# Step 3: Verify files
echo "Step 3/3: Verifying files..."
cd vendor/llama-cpp/tools/server/public

if [ ! -f "index.html" ]; then
    echo "❌ Error: index.html not found"
    exit 1
fi

ENTRY_DIR="_app/immutable/entry"
START_FILE=$(ls "$ENTRY_DIR"/start.*.js 2>/dev/null | head -1 | xargs basename)
APP_FILE=$(ls "$ENTRY_DIR"/app.*.js 2>/dev/null | head -1 | xargs basename)

if [ -z "$START_FILE" ] || [ -z "$APP_FILE" ]; then
    echo "❌ Error: JavaScript entry files not found"
    exit 1
fi

echo "✅ Found entry files: $START_FILE, $APP_FILE"

# Verify files referenced in index.html exist
if grep -q "$START_FILE" index.html && grep -q "$APP_FILE" index.html; then
    echo "✅ index.html references correct files"
else
    echo "⚠️  Warning: index.html may reference wrong files"
    echo "   Regenerating index.html..."
    
    cat > index.html << HTML
<!DOCTYPE html>
<html lang="en">
  <head>
    <meta charset="utf-8" />
    <link rel="icon" href="/_app/immutable/assets/favicon.svg" />
    <meta name="viewport" content="width=device-width, initial-scale=1" />
    <title>llama.cpp</title>
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
    echo "✅ index.html regenerated"
fi

echo ""
echo "╔══════════════════════════════════════════════════════════════╗"
echo "║    ✅ WEB UI REBUILD COMPLETE                                 ║"
echo "╚══════════════════════════════════════════════════════════════╝"
echo ""
echo "📋 NEXT STEPS:"
echo ""
echo "1. Stop any running server:"
echo "   pkill -f llama-server"
echo ""
echo "2. Restart Delta:"
echo "   delta server"
echo ""
echo "3. Open http://localhost:8080 in your browser"
echo ""
echo "4. If you still see a blank page:"
echo "   - Open Developer Tools (F12 or Cmd+Option+I)"
echo "   - Check Console tab for JavaScript errors"
echo "   - Check Network tab for 404 errors (red entries)"
echo "   - Try hard refresh: Cmd+Shift+R (Mac) or Ctrl+Shift+R"
echo ""

