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

# Detect architecture (works on both Linux and macOS)
if command -v dpkg >/dev/null 2>&1; then
    ARCH="${ARCH:-$(dpkg --print-architecture 2>/dev/null || uname -m)}"
else
    ARCH="${ARCH:-$(uname -m)}"
fi

# Normalize architecture names first
case "$ARCH" in
    x86_64|amd64)
        DEB_ARCH="amd64"
        ;;
    aarch64|arm64)
        DEB_ARCH="arm64"
        ;;
    armv7l|armv7|arm)
        DEB_ARCH="armhf"
        ;;
    *)
        DEB_ARCH="$ARCH"
        ;;
esac

# Use normalized architecture in package name
PACKAGE_NAME="${APP_NAME}_${VERSION}_${DEB_ARCH}"
BUILD_DIR="${BUILD_DIR:-build_linux_release}"
PACKAGE_DIR="$SCRIPT_DIR/packages"
DEB_DIR="$PACKAGE_DIR/${PACKAGE_NAME}"

info "Building .deb package for ${DEB_ARCH} (architecture: ${ARCH})..."

# Step 1: Check if build exists
info "Step 1/6: Verifying build..."
if [ ! -f "$BUILD_DIR/delta" ]; then
    # Try alternative build directories (non-interactive)
    ALTERNATIVE_BUILDS=("build_linux" "build" "build_release")
    BUILD_FOUND=false
    
    for alt_build in "${ALTERNATIVE_BUILDS[@]}"; do
        if [ -f "$alt_build/delta" ]; then
            warning "Build not found at $BUILD_DIR/delta, but found at $alt_build/delta"
            info "Using $alt_build instead"
            BUILD_DIR="$alt_build"
            BUILD_FOUND=true
            break
        fi
    done
    
    if [ "$BUILD_FOUND" = false ]; then
        error_exit "Build not found at $BUILD_DIR/delta. Please run ./installers/build_linux.sh first.\n\nOr specify a build directory:\n  BUILD_DIR=/path/to/build ./installers/package_linux_deb.sh"
    fi
fi
success "Build found at $BUILD_DIR/delta"

# Step 2: Create package directory structure
info "Step 2/6: Creating Debian package structure..."
rm -rf "$DEB_DIR"
mkdir -p "$DEB_DIR/DEBIAN"
mkdir -p "$DEB_DIR/usr/bin"
mkdir -p "$DEB_DIR/usr/share/delta-cli"
mkdir -p "$DEB_DIR/usr/share/doc/delta-cli"
mkdir -p "$DEB_DIR/usr/share/man/man1"

# Step 3: Copy binaries and verify architecture
info "Step 3/6: Copying binaries and verifying architecture..."

# Detect the actual binary architecture
BINARY_ARCH=""
if command -v file >/dev/null 2>&1; then
    FILE_OUTPUT=$(file "$BUILD_DIR/delta" 2>/dev/null || echo "")
    # Extract architecture from file output
    # Order matters: check more specific patterns first
    if echo "$FILE_OUTPUT" | grep -qiE "(x86_64|amd64|Intel 80386)"; then
        BINARY_ARCH="amd64"
    elif echo "$FILE_OUTPUT" | grep -qiE "(aarch64|arm64|ARM.*aarch64)"; then
        BINARY_ARCH="arm64"  # 64-bit ARM
    elif echo "$FILE_OUTPUT" | grep -qiE "(armv7|armhf|ARM.*v7)"; then
        BINARY_ARCH="armhf"  # 32-bit ARM
    elif echo "$FILE_OUTPUT" | grep -qiE "ARM"; then
        # Generic ARM - try to determine from context
        # Default to arm64 for modern systems
        BINARY_ARCH="arm64"
    fi
    
    if [ -n "$BINARY_ARCH" ]; then
        info "Detected binary architecture: ${BINARY_ARCH}"
        
        # Error if architecture mismatch
        if [ "$BINARY_ARCH" != "$DEB_ARCH" ]; then
            error_exit "❌ Architecture mismatch detected!\n   Binary is built for: ${BINARY_ARCH}\n   Package will be labeled as: ${DEB_ARCH}\n   This will cause 'Exec format error' on ${DEB_ARCH} systems!\n\nPlease build for ${DEB_ARCH} architecture first:\n   BUILD_DIR=build_linux_release_${DEB_ARCH} ARCH=${DEB_ARCH} ./installers/build_linux.sh\n\nOr use the correct binary:\n   BUILD_DIR=build_linux_release_${BINARY_ARCH} ARCH=${BINARY_ARCH} ./installers/package_linux_deb.sh"
        else
            success "✓ Binary architecture matches package architecture: ${DEB_ARCH}"
        fi
    else
        warning "Could not detect binary architecture from file command"
        warning "Proceeding, but package may not work correctly if architecture is wrong"
        info "File output: $FILE_OUTPUT"
    fi
else
    warning "file command not available, cannot verify binary architecture"
    warning "Proceeding, but package may not work correctly if architecture is wrong"
fi

# Copy the binary
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

# Create control file with enhanced metadata
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
Origin: Delta CLI
Bugs: https://github.com/oderoi/delta-cli/issues
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
 .
 This package is open-source software available on GitHub.
 Source code: https://github.com/oderoi/delta-cli
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

# Copy Ubuntu installation guide if available
if [ -f "$SCRIPT_DIR/UBUNTU_INSTALLATION.md" ]; then
    cp "$SCRIPT_DIR/UBUNTU_INSTALLATION.md" "$DEB_DIR/usr/share/doc/delta-cli/UBUNTU_INSTALLATION.md" 2>/dev/null || true
fi

# Step 6: Build the .deb package
info "Step 6/6: Building .deb package..."

# Fix permissions
find "$DEB_DIR" -type f -exec chmod 644 {} \; 2>/dev/null || true
find "$DEB_DIR" -type d -exec chmod 755 {} \; 2>/dev/null || true
find "$DEB_DIR/usr/bin" -type f -exec chmod 755 {} \; 2>/dev/null || true
find "$DEB_DIR/DEBIAN" -type f -exec chmod 755 {} \; 2>/dev/null || true

# Check for dpkg-deb
DPKG_DEB=""
if command -v dpkg-deb >/dev/null 2>&1; then
    DPKG_DEB="dpkg-deb"
elif command -v dpkg >/dev/null 2>&1 && dpkg --version >/dev/null 2>&1; then
    # Try to find dpkg-deb in common locations
    if [ -f "/usr/bin/dpkg-deb" ]; then
        DPKG_DEB="/usr/bin/dpkg-deb"
    elif [ -f "/usr/local/bin/dpkg-deb" ]; then
        DPKG_DEB="/usr/local/bin/dpkg-deb"
    fi
fi

# Build the package
DEB_FILE="$PACKAGE_DIR/${PACKAGE_NAME}.deb"

if [ -n "$DPKG_DEB" ]; then
    # Use dpkg-deb if available
    if $DPKG_DEB --build "$DEB_DIR" "$DEB_FILE" 2>/dev/null; then
        success "Package built successfully: $DEB_FILE"
        
        # Show package info
        if command -v dpkg-deb >/dev/null 2>&1; then
            info "Package information:"
            dpkg-deb -I "$DEB_FILE" 2>/dev/null | head -20 || true
        fi
        
        # Calculate and display checksum
        if command -v sha256sum >/dev/null 2>&1; then
            SHA256=$(sha256sum "$DEB_FILE" | cut -d' ' -f1)
        elif command -v shasum >/dev/null 2>&1; then
            SHA256=$(shasum -a 256 "$DEB_FILE" | cut -d' ' -f1)
        fi
        
        if [ -n "$SHA256" ]; then
            echo ""
            info "SHA256: $SHA256"
            echo "$SHA256  $(basename "$DEB_FILE")" > "${DEB_FILE}.sha256"
            success "Checksum saved to ${DEB_FILE}.sha256"
        fi
        
    echo ""
    success "✅ .deb package created successfully!"
    echo ""
    warning "⚠️  Ubuntu App Center Warning:"
    echo "   Ubuntu will show a 'potentially unsafe' warning because this"
    echo "   package is not signed or from a trusted repository. This is normal"
    echo "   for third-party packages. The package is safe to install."
    echo ""
    info "Package Architecture: ${DEB_ARCH}"
    echo ""
    warning "⚠️  CRITICAL: Architecture Verification"
    echo "   This package is built for: ${DEB_ARCH}"
    echo "   Make sure your system architecture matches!"
    echo "   Installing the wrong architecture will cause 'Exec format error'"
    echo ""
    echo "   Check your system architecture:"
    echo "     dpkg --print-architecture  # On Debian/Ubuntu"
    echo "     uname -m                    # On any Linux"
    echo ""
    info "Installation options:"
    echo ""
    echo "Option 1: Install via command line (recommended):"
    echo "  sudo dpkg -i $DEB_FILE"
    echo "  sudo apt-get install -f  # Fix dependencies if needed"
    echo ""
    echo "Option 2: Install via App Center:"
    echo "  - Double-click the .deb file"
    echo "  - Click 'Install' when prompted (the warning is normal)"
    echo ""
    echo "Option 3: Verify package before installing:"
    echo "  # Check package architecture:"
    echo "  dpkg-deb -I $DEB_FILE | grep Architecture"
    echo "  # Check package contents:"
    echo "  dpkg-deb -c $DEB_FILE"
    echo "  # Check package info:"
    echo "  dpkg-deb -I $DEB_FILE"
    echo ""
    info "To remove the package:"
    echo "  sudo dpkg -r $APP_NAME"
    echo ""
    info "Package source and verification:"
    echo "  - Source code: https://github.com/oderoi/delta-cli"
    echo "  - Report issues: https://github.com/oderoi/delta-cli/issues"
    echo ""
    else
        error_exit "Failed to build .deb package with dpkg-deb."
    fi
else
    # Alternative: Create .deb manually using ar (available on macOS)
    info "dpkg-deb not found, using alternative method (ar)..."
    
    if ! command -v ar >/dev/null 2>&1; then
        error_exit "Neither dpkg-deb nor ar found. On macOS, install dpkg via Homebrew:\n  brew install dpkg\n\nOr use Docker to build the .deb package on a Linux system."
    fi
    
    # Create temporary directory for ar archive
    AR_TEMP=$(mktemp -d)
    DEBIAN_BINARY="$AR_TEMP/debian-binary"
    CONTROL_TAR="$AR_TEMP/control.tar.gz"
    DATA_TAR="$AR_TEMP/data.tar.gz"
    
    # Create debian-binary file
    echo "2.0" > "$DEBIAN_BINARY"
    
    # Remove macOS extended attributes before creating tar files
    # This prevents "LIBARCHIVE.xattr" warnings on Linux
    info "Cleaning macOS extended attributes..."
    if command -v xattr >/dev/null 2>&1; then
        find "$DEB_DIR" -exec xattr -c {} \; 2>/dev/null || true
    fi
    # Also remove .DS_Store files
    find "$DEB_DIR" -name ".DS_Store" -delete 2>/dev/null || true
    
    # Create control.tar.gz (use POSIX/ustar format to avoid macOS-specific attributes)
    cd "$DEB_DIR/DEBIAN"
    # Use ustar format (POSIX) which doesn't support extended attributes
    tar --format=ustar -czf "$CONTROL_TAR" . 2>/dev/null || \
    tar --format=posix -czf "$CONTROL_TAR" . 2>/dev/null || \
    tar -czf "$CONTROL_TAR" . 2>/dev/null || error_exit "Failed to create control.tar.gz"
    
    # Create data.tar.gz (use POSIX/ustar format)
    cd "$DEB_DIR"
    
    # Only archive 'usr' directory (share is under usr/share/, not a top-level directory)
    if [ ! -d "usr" ]; then
        error_exit "usr directory not found in $DEB_DIR\nDirectory contents:\n$(ls -la "$DEB_DIR" 2>/dev/null || echo 'Directory not accessible')"
    fi
    
    info "Archiving usr directory..."
    
    # Use ustar format to avoid macOS extended attributes
    # Try different tar formats in order of preference
    TAR_SUCCESS=false
    TAR_ERROR=""
    
    # Try ustar format first (POSIX standard, no extended attributes)
    if tar --format=ustar -czf "$DATA_TAR" usr 2>&1; then
        TAR_SUCCESS=true
    else
        TAR_ERROR=$(tar --format=ustar -czf "$DATA_TAR" usr 2>&1)
        # Try posix format
        if tar --format=posix -czf "$DATA_TAR" usr 2>&1; then
            TAR_SUCCESS=true
        else
            TAR_ERROR=$(tar --format=posix -czf "$DATA_TAR" usr 2>&1)
            # Try default format (last resort)
            if tar -czf "$DATA_TAR" usr 2>&1; then
                TAR_SUCCESS=true
            else
                TAR_ERROR=$(tar -czf "$DATA_TAR" usr 2>&1)
            fi
        fi
    fi
    
    if [ "$TAR_SUCCESS" = false ]; then
        # Show debugging info
        warning "Failed to create data.tar.gz"
        warning "Tar error output: $TAR_ERROR"
        info "Current directory: $(pwd)"
        info "usr directory contents:"
        ls -la usr/ 2>/dev/null | head -10 || echo "Cannot list usr directory"
        error_exit "Failed to create data.tar.gz"
    fi
    
    # Verify the tar file was created and has content
    if [ ! -f "$DATA_TAR" ]; then
        error_exit "data.tar.gz was not created at $DATA_TAR"
    fi
    
    TAR_SIZE=$(stat -f%z "$DATA_TAR" 2>/dev/null || stat -c%s "$DATA_TAR" 2>/dev/null || echo "0")
    if [ "$TAR_SIZE" -eq 0 ]; then
        error_exit "data.tar.gz was created but is empty"
    fi
    
    success "Created data.tar.gz ($(du -h "$DATA_TAR" | cut -f1))"
    
    # Create .deb file using ar
    cd "$AR_TEMP"
    ar -r "$DEB_FILE" debian-binary control.tar.gz data.tar.gz 2>/dev/null || error_exit "Failed to create .deb with ar"
    
    # Cleanup
    rm -rf "$AR_TEMP"
    cd "$PROJECT_DIR"
    
    if [ -f "$DEB_FILE" ]; then
        success "Package built successfully: $DEB_FILE"
        
        # Calculate and display checksum
        if command -v sha256sum >/dev/null 2>&1; then
            SHA256=$(sha256sum "$DEB_FILE" | cut -d' ' -f1)
        elif command -v shasum >/dev/null 2>&1; then
            SHA256=$(shasum -a 256 "$DEB_FILE" | cut -d' ' -f1)
        fi
        
        if [ -n "$SHA256" ]; then
            echo ""
            info "SHA256: $SHA256"
            echo "$SHA256  $(basename "$DEB_FILE")" > "${DEB_FILE}.sha256"
            success "Checksum saved to ${DEB_FILE}.sha256"
        fi
        
        echo ""
        success "✅ .deb package created successfully (using ar method)!"
        echo ""
        info "Package architecture: ${DEB_ARCH}"
        echo ""
        info "To install the package on Ubuntu/Debian:"
        echo "  sudo dpkg -i $DEB_FILE"
        echo ""
        info "If you get dependency errors, run:"
        echo "  sudo apt-get install -f"
        echo ""
        info "If you get architecture mismatch error:"
        echo "  - Make sure you're installing the correct architecture"
        echo "  - Check system architecture: dpkg --print-architecture"
        echo "  - This package is for: ${DEB_ARCH}"
        echo "  - Rebuild for your architecture if needed"
        echo ""
        info "To remove the package:"
        echo "  sudo dpkg -r $APP_NAME"
        echo ""
        warning "Note: Package was built using ar. For best compatibility,"
        warning "consider building on a Linux system or using Docker."
        echo ""
    else
        error_exit "Failed to create .deb package."
    fi
fi

success "Done!"

