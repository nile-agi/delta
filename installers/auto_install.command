#!/bin/bash
# Auto-Install Script for Delta CLI
# This script runs automatically when the DMG is opened
# It installs the app and handles Gatekeeper trust automatically

# Don't exit on error - we want to handle errors gracefully
set +e

APP_NAME="Delta CLI"
APP_SOURCE="Delta CLI.app"
INSTALL_DIR="/Applications"
APP_DEST="$INSTALL_DIR/$APP_NAME.app"

# Get the directory where this script is located (DMG mount point)
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Check if app exists
if [ ! -d "$SCRIPT_DIR/$APP_SOURCE" ]; then
    osascript -e 'display dialog "Delta CLI.app not found in this DMG." buttons {"OK"} default button 1'
    exit 1
fi

# Show installation dialog
osascript <<EOF
tell application "System Events"
    activate
    display dialog "Delta CLI Installer

This will install Delta CLI to Applications and automatically handle security settings.

Click Install to continue." buttons {"Cancel", "Install"} default button 2 with title "Delta CLI Installer"
end tell
EOF

if [ $? -ne 0 ]; then
    exit 0
fi

# Remove old installation if it exists
if [ -d "$APP_DEST" ]; then
    rm -rf "$APP_DEST"
fi

# Copy app to Applications
cp -R "$SCRIPT_DIR/$APP_SOURCE" "$APP_DEST"

# CRITICAL: Remove quarantine and sign BEFORE user tries to open
# This must happen immediately after installation
find "$APP_DEST" -exec xattr -c {} \; 2>/dev/null || true

# Ad-hoc sign the app (helps with Gatekeeper)
if command -v codesign >/dev/null 2>&1; then
    codesign --force --deep --sign - "$APP_DEST" 2>/dev/null || true
fi

# Create symlink for terminal access (optional - don't fail if this doesn't work)
SYMLINK_PATH="/usr/local/bin/delta"
if [ -w "$(dirname "$SYMLINK_PATH")" ] 2>/dev/null; then
    if [ -L "$SYMLINK_PATH" ] || [ -f "$SYMLINK_PATH" ]; then
        rm -f "$SYMLINK_PATH" 2>/dev/null || true
    fi
    ln -s "$APP_DEST/Contents/MacOS/delta" "$SYMLINK_PATH" 2>/dev/null || true
elif sudo -n true 2>/dev/null; then
    # Try with sudo if we have passwordless sudo
    if [ -L "$SYMLINK_PATH" ] || [ -f "$SYMLINK_PATH" ]; then
        sudo rm -f "$SYMLINK_PATH" 2>/dev/null || true
    fi
    sudo ln -s "$APP_DEST/Contents/MacOS/delta" "$SYMLINK_PATH" 2>/dev/null || true
fi
# Note: If symlink creation fails, user can still run the app directly or add to PATH manually

# Show success message
osascript <<EOF
tell application "System Events"
    activate
    display dialog "✅ Delta CLI installed successfully!

The app has been installed to Applications and is ready to use.

You can now:
• Double-click 'Delta CLI.app' to open it
• Or run 'delta' from Terminal" buttons {"OK"} default button 1 with title "Installation Complete"
end tell
EOF

# Open Applications folder to show the installed app
open "$INSTALL_DIR"

