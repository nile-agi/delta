#!/bin/bash
# Update index.html with the latest JavaScript files

cd "$(dirname "$0")/vendor/llama.cpp/tools/server/public"

ENTRY_DIR="_app/immutable/entry"
START_FILE=$(ls -t "$ENTRY_DIR"/start.*.js 2>/dev/null | head -1 | xargs basename)
APP_FILE=$(ls -t "$ENTRY_DIR"/app.*.js 2>/dev/null | head -1 | xargs basename)

if [ -z "$START_FILE" ] || [ -z "$APP_FILE" ]; then
    echo "❌ Error: JavaScript entry files not found"
    exit 1
fi

echo "Found latest files: $START_FILE, $APP_FILE"

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

echo "✅ Updated index.html with: $START_FILE, $APP_FILE"

