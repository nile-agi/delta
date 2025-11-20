#!/bin/bash
# Delta CLI - Ubuntu/Debian .deb Package Builder
# Creates a proper .deb package for easy installation on Ubuntu/Debian systems

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

error_exit() {
    echo -e "${RED}❌ Error: $1${NC}" >&2
    exit 1
}

info() {
    echo -e "${BLUE}ℹ $1${NC}"
}

success() {
    echo -e "${GREEN}✓ $1${NC}"
}

warning() {
    echo -e "${YELLOW}⚠ $1${NC}"
}

# Get script directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
cd "$PROJECT_DIR"

# Configuration
APP_NAME="delta-cli"
VERSION="${VERSION:-1.0.0}"
ARCH="${ARCH:-$(dpkg --print-architecture 2>/dev/null || uname -m)}"
PACKAGE_NAME="${APP_NAME}_${VERSION}_${ARCH}"
BUILD_DIR="${BUILD_DIR:-build_linux_release}"
PACKAGE_DIR="$SCRIPT_DIR/packages"
DEB_DIR="$PACKAGE_DIR/${PACKAGE_NAME}"

# Normalize architecture names
case "$ARCH" in
    x86_64|amd64)
        DEB_ARCH="amd64"
        ;;
    aarch64|arm64)
        DEB_ARCH="arm64"
        ;;
    *)
        DEB_ARCH="$ARCH"
        ;;
esac

info "Building .deb package for ${DEB_ARCH}..."

# Step 1: Check if build exists
info "Step 1/6: Verifying build..."
if [ ! -f "$BUILD_DIR/delta" ]; then
    error_exit "Build not found at $BUILD_DIR/delta. Please run ./installers/build_linux.sh first."
fi
success "Build found"

# Step 2: Create package directory structure
info "Step 2/6: Creating Debian package structure..."
rm -rf "$DEB_DIR"
mkdir -p "$DEB_DIR/DEBIAN"
mkdir -p "$DEB_DIR/usr/bin"
mkdir -p "$DEB_DIR/usr/share/delta-cli"
mkdir -p "$DEB_DIR/usr/share/doc/delta-cli"
mkdir -p "$DEB_DIR/usr/share/man/man1"

# Step 3: Copy binaries
info "Step 3/6: Copying binaries..."
cp "$BUILD_DIR/delta" "$DEB_DIR/usr/bin/delta"
chmod +x "$DEB_DIR/usr/bin/delta"

if [ -f "$BUILD_DIR/delta-server" ]; then
    cp "$BUILD_DIR/delta-server" "$DEB_DIR/usr/bin/delta-server"
    chmod +x "$DEB_DIR/usr/bin/delta-server"
fi

# Copy web UI if available
if [ -d "public" ] && ([ -f "public/index.html" ] || [ -f "public/index.html.gz" ]); then
    info "Including web UI..."
    cp -r public/* "$DEB_DIR/usr/share/delta-cli/" 2>/dev/null || true
fi

# Copy logo if available
if [ -f "assets/delta-logo.png" ]; then
    cp "assets/delta-logo.png" "$DEB_DIR/usr/share/delta-cli/delta-logo.png" 2>/dev/null || true
fi

# Step 4: Create Debian control file
info "Step 4/6: Creating Debian control files..."

# Calculate installed size
INSTALLED_SIZE=$(du -sk "$DEB_DIR" | cut -f1)

# Create control file
cat > "$DEB_DIR/DEBIAN/control" <<EOF
Package: ${APP_NAME}
Version: ${VERSION}
Architecture: ${DEB_ARCH}
Maintainer: Delta CLI Team <support@delta-cli.com>
Installed-Size: ${INSTALLED_SIZE}
Depends: libc6 (>= 2.17), libstdc++6 (>= 4.9), libcurl4
Recommends: curl
Section: utils
Priority: optional
Homepage: https://github.com/oderoi/delta-cli
Description: Delta CLI - AI-powered command-line interface
 Delta CLI is a powerful command-line interface for interacting with
 large language models. It provides an easy-to-use interface for
 downloading, managing, and using AI models locally.
 .
 Features:
  - Download and manage AI models
  - Interactive chat interface
  - Web UI dashboard
  - Server mode for API access
  - Support for multiple model formats
EOF

# Create postinst script (runs after installation)
cat > "$DEB_DIR/DEBIAN/postinst" <<'POSTINST_EOF'
#!/bin/bash
set -e

# Update man page database
if command -v mandb >/dev/null 2>&1; then
    mandb >/dev/null 2>&1 || true
fi

# Create user data directory
mkdir -p "$HOME/.delta-cli" 2>/dev/null || true
chmod 755 "$HOME/.delta-cli" 2>/dev/null || true

echo "Delta CLI has been installed successfully!"
echo "Run 'delta --help' to get started."

exit 0
POSTINST_EOF
chmod +x "$DEB_DIR/DEBIAN/postinst"

# Create prerm script (runs before removal)
cat > "$DEB_DIR/DEBIAN/prerm" <<'PRERM_EOF'
#!/bin/bash
set -e
exit 0
PRERM_EOF
chmod +x "$DEB_DIR/DEBIAN/prerm"

# Create postrm script (runs after removal)
cat > "$DEB_DIR/DEBIAN/postrm" <<'POSTRM_EOF'
#!/bin/bash
set -e

# Update man page database
if command -v mandb >/dev/null 2>&1; then
    mandb >/dev/null 2>&1 || true
fi

exit 0
POSTRM_EOF
chmod +x "$DEB_DIR/DEBIAN/postrm"

# Step 5: Create documentation
info "Step 5/6: Creating documentation..."

# Create changelog
cat > "$DEB_DIR/usr/share/doc/delta-cli/changelog.Debian" <<EOF
delta-cli (${VERSION}) unstable; urgency=medium

  * Initial release

 -- Delta CLI Team <support@delta-cli.com>  $(date -R)
EOF
gzip -9 "$DEB_DIR/usr/share/doc/delta-cli/changelog.Debian" 2>/dev/null || true

# Create copyright file
cat > "$DEB_DIR/usr/share/doc/delta-cli/copyright" <<EOF
Format: https://www.debian.org/doc/packaging-manuals/copyright-format/1.0/
Upstream-Name: delta-cli
Source: https://github.com/oderoi/delta-cli

Files: *
Copyright: $(date +%Y) Delta CLI Team
License: MIT

License: MIT
 MIT License
 .
 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:
 .
 The above copyright notice and this permission notice shall be included in all
 copies or substantial portions of the Software.
 .
 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 SOFTWARE.
EOF

# Create README
if [ -f "README.md" ]; then
    cp "README.md" "$DEB_DIR/usr/share/doc/delta-cli/README.md" 2>/dev/null || true
fi

# Step 6: Build the .deb package
info "Step 6/6: Building .deb package..."

# Fix permissions
find "$DEB_DIR" -type f -exec chmod 644 {} \; 2>/dev/null || true
find "$DEB_DIR" -type d -exec chmod 755 {} \; 2>/dev/null || true
find "$DEB_DIR/usr/bin" -type f -exec chmod 755 {} \; 2>/dev/null || true
find "$DEB_DIR/DEBIAN" -type f -exec chmod 755 {} \; 2>/dev/null || true

# Build the package
DEB_FILE="$PACKAGE_DIR/${PACKAGE_NAME}.deb"
if dpkg-deb --build "$DEB_DIR" "$DEB_FILE" 2>/dev/null; then
    success "Package built successfully: $DEB_FILE"
    
    # Show package info
    info "Package information:"
    dpkg-deb -I "$DEB_FILE" 2>/dev/null | head -20 || true
    
    # Calculate and display checksum
    if command -v sha256sum >/dev/null 2>&1; then
        SHA256=$(sha256sum "$DEB_FILE" | cut -d' ' -f1)
        echo ""
        info "SHA256: $SHA256"
        echo "$SHA256  $(basename "$DEB_FILE")" > "${DEB_FILE}.sha256"
        success "Checksum saved to ${DEB_FILE}.sha256"
    fi
    
    echo ""
    success "✅ .deb package created successfully!"
    echo ""
    info "To install the package:"
    echo "  sudo dpkg -i $DEB_FILE"
    echo ""
    info "If you get dependency errors, run:"
    echo "  sudo apt-get install -f"
    echo ""
    info "To remove the package:"
    echo "  sudo dpkg -r $APP_NAME"
    echo ""
else
    error_exit "Failed to build .deb package. Make sure dpkg-deb is installed."
fi

success "Done!"

