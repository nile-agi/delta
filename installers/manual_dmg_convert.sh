#!/bin/bash
# Manual DMG conversion script - use if automatic conversion fails

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
cd "$PROJECT_DIR"

VERSION=${VERSION:-$(grep "project(delta-cli VERSION" CMakeLists.txt | sed 's/.*VERSION \([0-9.]*\).*/\1/')}
if [ -z "$VERSION" ]; then
    VERSION="1.0.0"
fi
DMG_NAME="DeltaCLI-${VERSION}-macOS-$(uname -m)"
TEMP_DMG="installers/packages/${DMG_NAME}.dmg.temp.dmg"
FINAL_DMG="installers/packages/${DMG_NAME}.dmg"

echo "Manual DMG Conversion Script"
echo "============================"
echo ""

# Step 1: Check if temp DMG exists
if [ ! -f "$TEMP_DMG" ]; then
    echo "❌ Error: Temp DMG not found: $TEMP_DMG"
    echo "Please run ./installers/package_macos.sh first to create the temp DMG"
    exit 1
fi

echo "✓ Found temp DMG: $TEMP_DMG"
echo ""

# Step 2: Detach all DMG mounts
echo "Step 1: Detaching all DMG mounts..."
hdiutil detach all 2>/dev/null || true
sleep 2

# Step 3: Check for any remaining mounts
echo "Step 2: Checking for remaining mounts..."
if hdiutil info | grep -q "$(basename "$TEMP_DMG")"; then
    echo "⚠ Warning: DMG still appears to be mounted"
    echo "Attempting force detach..."
    DEVICE_ID=$(hdiutil info | grep -B 5 "$(basename "$TEMP_DMG")" | grep "^/dev/" | awk '{print $1}' | head -1)
    if [ -n "$DEVICE_ID" ]; then
        hdiutil detach "$DEVICE_ID" -force
        sleep 2
    fi
else
    echo "✓ No mounts found"
fi

# Step 4: Remove final DMG if it exists
echo "Step 3: Cleaning up existing final DMG..."
rm -f "$FINAL_DMG"

# Step 5: Convert
echo "Step 4: Converting DMG..."
echo "This may take a minute..."
if hdiutil convert "$TEMP_DMG" \
    -format UDZO \
    -imagekey zlib-level=9 \
    -o "$FINAL_DMG"; then
    echo ""
    echo "✅ Success! DMG created at:"
    echo "   $FINAL_DMG"
    echo ""
    echo "You can now:"
    echo "  1. Test it: open $FINAL_DMG"
    echo "  2. Upload it to your release page"
    echo ""
else
    echo ""
    echo "❌ Conversion failed. Try:"
    echo "  1. Close all Finder windows"
    echo "  2. Run: hdiutil detach all"
    echo "  3. Run this script again"
    exit 1
fi

