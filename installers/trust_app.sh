#!/bin/bash
# Script to trust and allow Delta CLI to run (bypasses Gatekeeper)

set -e

APP_PATH="/Applications/Delta CLI.app"

echo "Delta CLI - Trust App Script"
echo "============================"
echo ""

if [ ! -d "$APP_PATH" ]; then
    echo "❌ Error: Delta CLI.app not found at: $APP_PATH"
    echo ""
    echo "Please install the app first by:"
    echo "  1. Opening the DMG"
    echo "  2. Dragging 'Delta CLI.app' to Applications"
    echo ""
    exit 1
fi

echo "Found app at: $APP_PATH"
echo ""

# Remove quarantine attributes
echo "Step 1: Removing quarantine attributes..."
find "$APP_PATH" -exec xattr -c {} \; 2>/dev/null || true
echo "✓ Quarantine attributes removed"
echo ""

# Try to ad-hoc sign (helps but not required)
echo "Step 2: Signing app (ad-hoc)..."
if command -v codesign >/dev/null 2>&1; then
    codesign --force --deep --sign - "$APP_PATH" 2>/dev/null && \
    echo "✓ App signed" || \
    echo "⚠ Could not sign (this is okay)"
else
    echo "⚠ codesign not available, skipping"
fi
echo ""

# Verify
echo "Step 3: Verifying..."
if [ -x "$APP_PATH/Contents/MacOS/delta" ]; then
    echo "✓ Binary is executable"
    
    # Test if it runs
    if "$APP_PATH/Contents/MacOS/delta" --version >/dev/null 2>&1; then
        echo "✓ App runs successfully"
        echo ""
        echo "✅ Delta CLI is now trusted and ready to use!"
        echo ""
        echo "You can now:"
        echo "  - Double-click the app to open it"
        echo "  - Run from Terminal: delta --version"
        echo ""
    else
        echo "⚠ App found but execution test failed"
        echo "  Try running manually: $APP_PATH/Contents/MacOS/delta --version"
    fi
else
    echo "❌ Binary is not executable"
    chmod +x "$APP_PATH/Contents/MacOS/delta"
    echo "✓ Fixed permissions"
fi

echo ""
echo "If you still see security warnings:"
echo "  1. Right-click on Delta CLI.app → Open"
echo "  2. Click 'Open' in the security dialog"
echo ""

