#!/bin/bash
# Update installation with llama.cpp web UI - run with sudo

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PUBLIC_DIR="$SCRIPT_DIR/vendor/llama.cpp/tools/server/public"
INSTALL_DIR="/usr/local/share/delta-cli/webui/public"

if [ ! -d "$PUBLIC_DIR" ] || [ ! -f "$PUBLIC_DIR/index.html" ]; then
    echo "❌ Public directory not found! Rebuilding..."
    cd "$SCRIPT_DIR/vendor/llama.cpp/tools/server/webui"
    npm run build 2>&1 | tail -3
    cd "$SCRIPT_DIR"
fi

echo "Updating installation directory with llama.cpp web UI..."
sudo rm -rf "$INSTALL_DIR"/*
sudo cp -r "$PUBLIC_DIR"/* "$INSTALL_DIR"/

# Fix title if needed
sudo sed -i '' 's/Delta CLI - Offline AI Assistant/llama.cpp/g' "$INSTALL_DIR/index.html" 2>/dev/null || sudo sed -i 's/Delta CLI - Offline AI Assistant/llama.cpp/g' "$INSTALL_DIR/index.html"

echo "✅ Installation directory updated with llama.cpp web UI!"
