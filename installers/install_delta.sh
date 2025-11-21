#!/bin/bash
# Delta CLI - Automated Installer
# This script installs Delta CLI and handles Gatekeeper trust automatically

set -e

APP_NAME="Delta CLI"
APP_SOURCE="Delta CLI.app"
INSTALL_DIR="/Applications"
APP_DEST="$INSTALL_DIR/$APP_NAME.app"

echo "╔══════════════════════════════════════════════════════════════╗"
echo "║         Delta CLI - Automated Installer                     ║"
echo "╚══════════════════════════════════════════════════════════════╝"
echo ""

# Check if app exists in current directory
if [ ! -d "$APP_SOURCE" ]; then
    echo "❌ Error: $APP_SOURCE not found in current directory"
    echo ""
    echo "Please run this script from the DMG after mounting it."
    exit 1
fi

echo "Found: $APP_SOURCE"
echo ""

# Remove old installation if it exists
if [ -d "$APP_DEST" ]; then
    echo "Removing existing installation..."
    rm -rf "$APP_DEST"
    echo "✓ Removed old installation"
    echo ""
fi

# Copy app to Applications
echo "Installing to $INSTALL_DIR..."
cp -R "$APP_SOURCE" "$APP_DEST"
echo "✓ App copied to Applications"
echo ""

# Remove quarantine attributes (critical for auto-trust)
echo "Removing security quarantine..."
find "$APP_DEST" -exec xattr -c {} \; 2>/dev/null || true
echo "✓ Quarantine removed"
echo ""

# Try to ad-hoc sign (helps with Gatekeeper)
echo "Signing app..."
if command -v codesign >/dev/null 2>&1; then
    codesign --force --deep --sign - "$APP_DEST" 2>/dev/null && \
    echo "✓ App signed" || \
    echo "⚠ Could not sign (app will auto-trust on first run)"
else
    echo "⚠ codesign not available (app will auto-trust on first run)"
fi
echo ""

# Note: The app's launcher script will also auto-trust on first run
# This provides a double layer of protection

# Create symlink for easy terminal access
echo "Creating terminal symlink..."
SYMLINK_PATH="/usr/local/bin/delta"
if [ -w "$(dirname "$SYMLINK_PATH")" ] 2>/dev/null || sudo -n true 2>/dev/null; then
    if [ -L "$SYMLINK_PATH" ] || [ -f "$SYMLINK_PATH" ]; then
        sudo rm -f "$SYMLINK_PATH" 2>/dev/null || rm -f "$SYMLINK_PATH" 2>/dev/null
    fi
    sudo ln -s "$APP_DEST/Contents/MacOS/delta" "$SYMLINK_PATH" 2>/dev/null || \
    ln -s "$APP_DEST/Contents/MacOS/delta" "$SYMLINK_PATH" 2>/dev/null && \
    echo "✓ Symlink created at $SYMLINK_PATH" || \
    echo "⚠ Could not create symlink (you can add to PATH manually)"
else
    echo "⚠ Need sudo to create symlink. You can create it manually:"
    echo "   sudo ln -s $APP_DEST/Contents/MacOS/delta $SYMLINK_PATH"
fi
echo ""

# Verify installation
echo "Verifying installation..."
if [ -d "$APP_DEST" ] && [ -x "$APP_DEST/Contents/MacOS/delta" ]; then
    echo "✓ Installation verified"
    echo ""
    
    # Test the app
    if "$APP_DEST/Contents/MacOS/delta" --version >/dev/null 2>&1; then
        VERSION=$("$APP_DEST/Contents/MacOS/delta" --version 2>&1 | head -1)
        echo "✓ App is working: $VERSION"
    else
        echo "⚠ App installed but could not verify execution"
    fi
else
    echo "❌ Installation verification failed"
    exit 1
fi

echo ""
echo "╔══════════════════════════════════════════════════════════════╗"
echo "║              Installation Complete!                        ║"
echo "╚══════════════════════════════════════════════════════════════╝"
echo ""
echo "✅ Delta CLI has been installed successfully!"
echo ""
echo "To use Delta CLI:"
echo "  1. Open Terminal"
echo "  2. Run: delta --version"
echo ""
echo "If 'delta' command is not found:"
echo "  export PATH=\"/Applications/$APP_NAME.app/Contents/MacOS:\$PATH\""
echo ""
echo "Or restart your terminal."
echo ""
echo "To start using Delta CLI:"
echo "  delta pull qwen3:0.6b    # Download a model"
echo "  delta                    # Start chatting"
echo ""

