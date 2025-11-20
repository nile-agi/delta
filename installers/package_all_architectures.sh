#!/bin/bash
# Delta CLI - Build .deb packages for all architectures
# Creates separate .deb files for each supported architecture

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
VERSION="${VERSION:-1.0.0}"
PACKAGE_DIR="$SCRIPT_DIR/packages"

# Supported architectures
ARCHITECTURES=("amd64" "arm64")

echo "╔══════════════════════════════════════════════════════════════╗"
echo "║    Delta CLI - Multi-Architecture .deb Package Builder       ║"
echo "╚══════════════════════════════════════════════════════════════╝"
echo ""

info "This script will create .deb packages for multiple architectures."
info "Each architecture will have its own installable .deb file."
echo ""

# Check if we have builds for the architectures
BUILDS_FOUND=()
for ARCH in "${ARCHITECTURES[@]}"; do
    # Try different build directory patterns
    BUILD_PATTERNS=(
        "build_linux_release_${ARCH}"
        "build_linux_${ARCH}"
        "build_${ARCH}"
        "build_linux_release"  # Generic fallback
    )
    
    BUILD_FOUND=false
    for BUILD_PATTERN in "${BUILD_PATTERNS[@]}"; do
        if [ -f "${BUILD_PATTERN}/delta" ]; then
            info "Found build for ${ARCH} at: ${BUILD_PATTERN}"
            BUILDS_FOUND+=("${ARCH}:${BUILD_PATTERN}")
            BUILD_FOUND=true
            break
        fi
    done
    
    if [ "$BUILD_FOUND" = false ]; then
        warning "No build found for ${ARCH}"
        warning "  Expected build directory: build_linux_release_${ARCH} or build_linux_${ARCH}"
    fi
done

if [ ${#BUILDS_FOUND[@]} -eq 0 ]; then
    error_exit "No builds found for any architecture. Please build first:\n  ./installers/build_linux.sh\n\nOr build for specific architectures and place them in:\n  build_linux_release_amd64/\n  build_linux_release_arm64/"
fi

echo ""
info "Found builds for ${#BUILDS_FOUND[@]} architecture(s)"
echo ""

# Create packages directory
mkdir -p "$PACKAGE_DIR"

# Build package for each architecture
PACKAGES_CREATED=()
for BUILD_INFO in "${BUILDS_FOUND[@]}"; do
    ARCH="${BUILD_INFO%%:*}"
    BUILD_DIR="${BUILD_INFO##*:}"
    
    echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
    info "Building .deb package for ${ARCH}..."
    echo ""
    
    # Build the package
    if BUILD_DIR="$BUILD_DIR" ARCH="$ARCH" VERSION="$VERSION" "$SCRIPT_DIR/package_linux_deb.sh" >/dev/null 2>&1; then
        # Find the created package
        DEB_FILE=$(find "$PACKAGE_DIR" -name "delta-cli_${VERSION}_${ARCH}.deb" -o -name "delta-cli_${VERSION}_*${ARCH}*.deb" | head -1)
        
        if [ -n "$DEB_FILE" ] && [ -f "$DEB_FILE" ]; then
            success "Package created: $(basename "$DEB_FILE")"
            PACKAGES_CREATED+=("$DEB_FILE")
        else
            warning "Package build completed but file not found for ${ARCH}"
        fi
    else
        warning "Failed to build package for ${ARCH}"
    fi
    
    echo ""
done

# Summary
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo ""
if [ ${#PACKAGES_CREATED[@]} -gt 0 ]; then
    success "✅ Created ${#PACKAGES_CREATED[@]} package(s):"
    echo ""
    for DEB_FILE in "${PACKAGES_CREATED[@]}"; do
        FILE_SIZE=$(du -h "$DEB_FILE" | cut -f1)
        echo "  • $(basename "$DEB_FILE") (${FILE_SIZE})"
    done
    echo ""
    info "Package location: $PACKAGE_DIR"
    echo ""
    info "Installation instructions:"
    echo ""
    echo "For amd64/x86_64 systems:"
    echo "  sudo dpkg -i $PACKAGE_DIR/delta-cli_${VERSION}_amd64.deb"
    echo ""
    echo "For arm64/aarch64 systems:"
    echo "  sudo dpkg -i $PACKAGE_DIR/delta-cli_${VERSION}_arm64.deb"
    echo ""
    echo "To check your system architecture:"
    echo "  dpkg --print-architecture  # On Debian/Ubuntu"
    echo "  uname -m                    # On any Linux"
    echo ""
    warning "⚠️  Note: Make sure to install the package matching your system architecture!"
else
    error_exit "No packages were created. Check build directories and try again."
fi

success "Done!"

