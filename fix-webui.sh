#!/bin/bash
# Fix web UI index.html with correct JavaScript file names

set -e

cd "$(dirname "$0")/vendor/llama-cpp/tools/server/public"

echo "🔍 Finding JavaScript entry files..."

ENTRY_DIR="_app/immutable/entry"
START_FILE=$(ls "$ENTRY_DIR"/start.*.js 2>/dev/null | head -1 | xargs basename)
APP_FILE=$(ls "$ENTRY_DIR"/app.*.js 2>/dev/null | head -1 | xargs basename)

if [ -z "$START_FILE" ] || [ -z "$APP_FILE" ]; then
    echo "❌ Error: JavaScript entry files not found in $ENTRY_DIR"
    echo "Available files:"
    ls "$ENTRY_DIR"/*.js 2>/dev/null || echo "No .js files found"
    exit 1
fi

echo "✅ Found: $START_FILE, $APP_FILE"

# Create proper index.html
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

echo "✅ Fixed index.html with correct file names"
echo "📋 Files referenced:"
echo "   - $START_FILE"
echo "   - $APP_FILE"

