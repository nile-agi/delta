#!/bin/bash
# Delta CLI - Complete macOS DMG Creation Script
# This script builds the application and creates a .dmg installer in one step

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
cd "$PROJECT_DIR"

echo "╔══════════════════════════════════════════════════════════════╗"
echo "║    Delta CLI - Complete macOS DMG Builder                   ║"
echo "╚══════════════════════════════════════════════════════════════╝"
echo ""

# Step 1: Build the application
echo "Step 1/2: Building application..."
"$SCRIPT_DIR/build_macos.sh" || exit 1

echo ""
echo "Step 2/2: Creating DMG installer..."
"$SCRIPT_DIR/package_macos.sh" || exit 1

echo ""
echo "✅ Complete! Your DMG installer is ready for distribution."
echo ""

