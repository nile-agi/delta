#!/bin/bash
# Delta CLI - macOS DMG Packaging Script
# Creates a .dmg installer for easy distribution

set -e  # Exit on error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Error handling
error_exit() {
    echo -e "${RED}❌ Error: $1${NC}" >&2
    exit 1
}

success() {
    echo -e "${GREEN}✓ $1${NC}"
}

info() {
    echo -e "${BLUE}ℹ $1${NC}"
}

warning() {
    echo -e "${YELLOW}⚠ $1${NC}"
}

echo "╔══════════════════════════════════════════════════════════════╗"
echo "║         Delta CLI - macOS DMG Packaging                      ║"
echo "╚══════════════════════════════════════════════════════════════╝"
echo ""

# Check if running on macOS
if [[ "$OSTYPE" != "darwin"* ]]; then
    error_exit "This script is for macOS only."
fi

# Get script directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
cd "$PROJECT_DIR"

# Configuration
BUILD_DIR="build_macos_release"
VERSION=${VERSION:-$(grep "project(delta-cli VERSION" CMakeLists.txt | sed 's/.*VERSION \([0-9.]*\).*/\1/')}
if [ -z "$VERSION" ]; then
    VERSION="1.0.0"
fi
APP_NAME="Delta CLI"
DMG_NAME="DeltaCLI-${VERSION}-macOS-$(uname -m)"
TEMP_DMG_DIR="dmg_temp"
DMG_PATH="installers/packages/${DMG_NAME}.dmg"

info "Version: $VERSION"
info "Architecture: $(uname -m)"

# Step 1: Verify build exists
info "Step 1/6: Verifying build..."
if [ ! -f "$BUILD_DIR/delta" ]; then
    error_exit "Build not found. Please run ./installers/build_macos.sh first."
fi
success "Build found"

# Step 2: Create temporary directory structure
info "Step 2/6: Creating package structure..."
rm -rf "$TEMP_DMG_DIR"
mkdir -p "$TEMP_DMG_DIR"
mkdir -p "$(dirname "$DMG_PATH")"

# Step 3: Create .app bundle (optional but recommended for better UX)
info "Step 3/6: Creating macOS application bundle..."
APP_BUNDLE="$TEMP_DMG_DIR/${APP_NAME}.app"
mkdir -p "$APP_BUNDLE/Contents/MacOS"
mkdir -p "$APP_BUNDLE/Contents/Resources"

# Copy binaries
cp "$BUILD_DIR/delta" "$APP_BUNDLE/Contents/MacOS/delta"
if [ -f "$BUILD_DIR/delta-server" ]; then
    cp "$BUILD_DIR/delta-server" "$APP_BUNDLE/Contents/MacOS/delta-server"
fi

# Make binaries executable
chmod +x "$APP_BUNDLE/Contents/MacOS/delta"
if [ -f "$APP_BUNDLE/Contents/MacOS/delta-server" ]; then
    chmod +x "$APP_BUNDLE/Contents/MacOS/delta-server"
fi

# Create launcher script (allows app to be double-clicked and opens Terminal)
cat > "$APP_BUNDLE/Contents/MacOS/DeltaCLI" <<'LAUNCHER_EOF'
#!/bin/bash
# Launcher for Delta CLI - opens Terminal and runs delta

# Get the app bundle directory
APP_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
APP_ROOT="$(cd "$APP_DIR/../../.." && pwd)"
DELTA_BINARY="$APP_DIR/delta"

# Open Terminal and run delta
osascript <<EOF
tell application "Terminal"
    activate
    do script "\"$DELTA_BINARY\" \"\$@\""
end tell
EOF
LAUNCHER_EOF
chmod +x "$APP_BUNDLE/Contents/MacOS/DeltaCLI"

# Verify binary architecture
info "Verifying binary architecture..."
BINARY_ARCH=$(file "$APP_BUNDLE/Contents/MacOS/delta" | grep -o "arm64\|x86_64" | head -1)
if [ -n "$BINARY_ARCH" ]; then
    info "Binary architecture: $BINARY_ARCH"
    if [ "$BINARY_ARCH" = "arm64" ]; then
        success "Binary is arm64 (compatible with M1, M2, M3 Macs)"
    fi
else
    warning "Could not determine binary architecture"
fi

# Remove quarantine attribute (allows app to run without Gatekeeper blocking)
# Use compatible xattr command for all macOS versions
find "$APP_BUNDLE" -exec xattr -c {} \; 2>/dev/null || true

# Create Info.plist
cat > "$APP_BUNDLE/Contents/Info.plist" <<EOF
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>CFBundleExecutable</key>
    <string>DeltaCLI</string>
    <key>CFBundleIdentifier</key>
    <string>com.delta.cli</string>
    <key>CFBundleName</key>
    <string>${APP_NAME}</string>
    <key>CFBundleVersion</key>
    <string>${VERSION}</string>
    <key>CFBundleShortVersionString</key>
    <string>${VERSION}</string>
    <key>CFBundlePackageType</key>
    <string>APPL</string>
    <key>CFBundleSignature</key>
    <string>????</string>
    <key>LSMinimumSystemVersion</key>
    <string>10.13</string>
    <key>LSRequiresIPhoneOS</key>
    <false/>
    <key>NSHighResolutionCapable</key>
    <true/>
    <key>LSArchitecturePriority</key>
    <array>
        <string>arm64</string>
        <string>x86_64</string>
    </array>
    <key>NSHumanReadableCopyright</key>
    <string>Copyright © 2024 Delta CLI. All rights reserved.</string>
    <key>LSApplicationCategoryType</key>
    <string>public.app-category.utilities</string>
</dict>
</plist>
EOF

# Note: The CFBundleExecutable in Info.plist points to "delta"
# So the main executable is the delta binary itself

# Verify Info.plist is valid
if plutil -lint "$APP_BUNDLE/Contents/Info.plist" >/dev/null 2>&1; then
    success "Info.plist is valid"
else
    warning "Info.plist validation failed, but continuing..."
fi

# Copy web UI if available
if [ -d "public" ] && ([ -f "public/index.html" ] || [ -f "public/index.html.gz" ]); then
    info "Including web UI..."
    mkdir -p "$APP_BUNDLE/Contents/Resources/webui"
    cp -r public/* "$APP_BUNDLE/Contents/Resources/webui/" 2>/dev/null || true
fi

success "Application bundle created"

# Step 4: Create Applications symlink and README
info "Step 4/6: Creating DMG contents..."
# Create a symlink to Applications
ln -s /Applications "$TEMP_DMG_DIR/Applications"

# Create README
cat > "$TEMP_DMG_DIR/README.txt" <<EOF
Delta CLI ${VERSION} - Installation Instructions
================================================

1. Drag "${APP_NAME}.app" to the Applications folder.

2. To use Delta CLI from Terminal:
   - Open Terminal
   - Run: delta --version
   
   If the command is not found, add to your PATH:
   export PATH="/Applications/${APP_NAME}.app/Contents/MacOS:\$PATH"
   
   Or create a symlink:
   sudo ln -s /Applications/${APP_NAME}.app/Contents/MacOS/delta /usr/local/bin/delta

3. Start using Delta CLI:
   delta pull qwen3:0.6b    # Download a model
   delta                    # Start chatting

For more information, visit:
https://github.com/oderoi/delta-cli

EOF

success "DMG contents prepared"

# Step 5: Create DMG (simplified approach - create compressed DMG directly)
info "Step 5/6: Creating DMG image..."
# Remove existing DMG files if they exist
info "Cleaning up any existing DMG files..."
rm -f "$DMG_PATH"
rm -f "$DMG_PATH.temp.dmg"

# Clean up any existing mounts
info "Cleaning up any existing mounts..."
hdiutil detach all 2>/dev/null || true
osascript -e 'tell application "Finder" to close every window' 2>/dev/null || true
sleep 2

# Create compressed DMG directly (no temp file, no mounting needed)
info "Creating compressed DMG directly..."
# Calculate size needed
SIZE=$(du -sk "$TEMP_DMG_DIR" | cut -f1)
SIZE=$((SIZE + 2000))  # Add 2MB overhead

# Create DMG directly as compressed format - this avoids mount/unmount issues
if hdiutil create -srcfolder "$TEMP_DMG_DIR" \
    -volname "${APP_NAME} ${VERSION}" \
    -fs HFS+ \
    -format UDZO \
    -imagekey zlib-level=9 \
    -size ${SIZE}k \
    "$DMG_PATH" 2>&1; then
    success "DMG created successfully: $DMG_PATH"
    SKIP_CONVERSION=true
else
    # Fallback: create temp DMG if direct creation fails
    warning "Direct creation failed, trying two-step approach..."
    SKIP_CONVERSION=false
    hdiutil create -srcfolder "$TEMP_DMG_DIR" \
        -volname "${APP_NAME} ${VERSION}" \
        -fs HFS+ \
        -fsargs "-c c=64,a=16,e=16" \
        -format UDRW \
        -size ${SIZE}k \
        "$DMG_PATH.temp.dmg" || error_exit "Failed to create DMG"
fi

# Only mount if we need to customize (skip if direct creation worked)
if [ "$SKIP_CONVERSION" = "false" ]; then
    # Mount the DMG
    MOUNT_DIR=$(hdiutil attach -readwrite -noverify -noautoopen "$DMG_PATH.temp.dmg" 2>&1 | \
        egrep '^/dev/' | sed 1q | awk '{print $3}')

    # Wait for mount
    sleep 2

    # Set DMG properties
    if [ -n "$MOUNT_DIR" ] && [ -d "$MOUNT_DIR" ]; then
        # Set background (optional - requires background.png)
        if [ -f "$SCRIPT_DIR/background.png" ]; then
            # Create .background directory
            mkdir -p "$MOUNT_DIR/.background"
            cp "$SCRIPT_DIR/background.png" "$MOUNT_DIR/.background/background.png"
        
            # Use AppleScript to set background and layout
            osascript <<EOF
tell application "Finder"
    tell disk "${APP_NAME} ${VERSION}"
        open
        set current view of container window to icon view
        set toolbar visible of container window to false
        set statusbar visible of container window to false
        set the bounds of container window to {400, 100, 920, 420}
        set viewOptions to the icon view options of container window
        set arrangement of viewOptions to not arranged
        set icon size of viewOptions to 72
        set background picture of viewOptions to file ".background:background.png"
        set position of item "${APP_NAME}.app" of container window to {160, 205}
        set position of item "Applications" of container window to {360, 205}
        set position of item "README.txt" of container window to {260, 105}
        close
        open
        update without registering applications
        delay 2
    end tell
end tell
EOF
        else
            # Simple layout without background
            osascript <<EOF
tell application "Finder"
    tell disk "${APP_NAME} ${VERSION}"
        open
        set current view of container window to icon view
        set toolbar visible of container window to false
        set statusbar visible of container window to false
        set the bounds of container window to {400, 100, 920, 420}
        set viewOptions to the icon view options of container window
        set arrangement of viewOptions to not arranged
        set icon size of viewOptions to 72
        set position of item "${APP_NAME}.app" of container window to {160, 205}
        set position of item "Applications" of container window to {360, 205}
        set position of item "README.txt" of container window to {260, 105}
        close
        open
        update without registering applications
        delay 2
    end tell
end tell
EOF
        fi
    
    # Unmount - wait for it to complete and retry if needed
    info "Unmounting DMG..."
    
    # First, close any Finder windows that might have the DMG open
    osascript -e "tell application \"Finder\" to close every window whose name contains \"${APP_NAME}\"" 2>/dev/null || true
    sleep 1
    
    # Try to unmount gracefully first
    for i in {1..3}; do
        if hdiutil detach "$MOUNT_DIR" -quiet 2>/dev/null; then
            success "DMG unmounted"
            break
        else
            if [ $i -eq 3 ]; then
                warning "Could not unmount cleanly, forcing detach..."
                hdiutil detach "$MOUNT_DIR" -force 2>/dev/null || true
            else
                info "Waiting for DMG to unmount (attempt $i/3)..."
                sleep 2
            fi
        fi
    done
    
    # Verify it's actually unmounted by checking if the mount point still exists
    for i in {1..10}; do
        if [ ! -d "$MOUNT_DIR" ]; then
            success "DMG confirmed unmounted"
            break
        else
            if [ $i -eq 10 ]; then
                warning "Mount point still exists, forcing detach again..."
                hdiutil detach "$MOUNT_DIR" -force 2>/dev/null || true
                # Try alternative method
                diskutil unmount force "$MOUNT_DIR" 2>/dev/null || true
            else
                info "Waiting for mount point to disappear (attempt $i/10)..."
                sleep 1
            fi
        fi
    done
    
    # Wait a bit more to ensure the filesystem is fully released
    sleep 2
    
    # Convert to final compressed DMG
    info "Converting to compressed DMG..."
    # Force unmount
    hdiutil detach "$MOUNT_DIR" -force 2>/dev/null || true
    sleep 3
    
    # Convert
    if hdiutil convert "$DMG_PATH.temp.dmg" \
        -format UDZO \
        -imagekey zlib-level=9 \
        -o "$DMG_PATH" 2>&1; then
        success "DMG converted successfully"
        rm -f "$DMG_PATH.temp.dmg"
    else
        # If conversion fails, keep the temp DMG and inform user
        warning "Conversion failed, but temp DMG is available at: $DMG_PATH.temp.dmg"
        warning "You can manually convert it later or use: ./installers/manual_dmg_convert.sh"
        # Still create a symlink or copy for convenience
        cp "$DMG_PATH.temp.dmg" "$DMG_PATH" 2>/dev/null || true
    fi
    else
        warning "Could not mount DMG for customization, creating basic DMG..."
        # Just convert the temp DMG we created
        if [ -f "$DMG_PATH.temp.dmg" ]; then
            hdiutil convert "$DMG_PATH.temp.dmg" \
                -format UDZO \
                -imagekey zlib-level=9 \
                -o "$DMG_PATH" 2>&1 && rm -f "$DMG_PATH.temp.dmg" || \
            warning "Conversion failed, temp DMG available at: $DMG_PATH.temp.dmg"
        fi
    fi
fi

success "DMG created: $DMG_PATH"

# Step 6: Cleanup
info "Step 6/6: Cleaning up..."
rm -rf "$TEMP_DMG_DIR"

# Get DMG size
DMG_SIZE=$(du -h "$DMG_PATH" | cut -f1)

echo ""
echo "╔══════════════════════════════════════════════════════════════╗"
echo "║              DMG Packaging Complete!                         ║"
echo "╚══════════════════════════════════════════════════════════════╝"
echo ""
success "DMG installer created successfully!"
echo ""
info "File: $DMG_PATH"
info "Size: $DMG_SIZE"
echo ""
info "To distribute:"
echo "  1. Upload $DMG_PATH to your release page"
echo "  2. Users can download and double-click to mount"
echo "  3. They drag ${APP_NAME}.app to Applications folder"
echo ""
info "To test the DMG:"
echo "  open $DMG_PATH"
echo ""

