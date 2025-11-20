#!/bin/bash
# Delta CLI - Build .deb packages for all architectures
# Creates separate .deb files for each supported architecture

set -e  # Exit on error, but we'll handle errors in the loop

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
BUILD_DIRS_USED=()  # Track which directories we've already used

for ARCH in "${ARCHITECTURES[@]}"; do
    # Try different build directory patterns (architecture-specific first)
    BUILD_PATTERNS=(
        "build_linux_release_${ARCH}"
        "build_linux_${ARCH}"
        "build_${ARCH}"
    )
    
    BUILD_FOUND=false
    SELECTED_BUILD=""
    
    # First, try architecture-specific directories
    for BUILD_PATTERN in "${BUILD_PATTERNS[@]}"; do
        if [ -f "${BUILD_PATTERN}/delta" ] && [ -x "${BUILD_PATTERN}/delta" ]; then
            # Check if this directory was already assigned to another architecture
            if [[ ! " ${BUILD_DIRS_USED[@]} " =~ " ${BUILD_PATTERN} " ]]; then
                info "Found build for ${ARCH} at: ${BUILD_PATTERN}"
                # Check the actual binary architecture if possible
                if command -v file >/dev/null 2>&1; then
                    BINARY_ARCH=$(file "${BUILD_PATTERN}/delta" 2>/dev/null | grep -oE "(x86_64|amd64|arm64|aarch64)" | head -1 || echo "")
                    if [ -n "$BINARY_ARCH" ]; then
                        info "  Binary architecture: ${BINARY_ARCH}"
                    fi
                fi
                BUILDS_FOUND+=("${ARCH}:${BUILD_PATTERN}")
                BUILD_DIRS_USED+=("${BUILD_PATTERN}")
                BUILD_FOUND=true
                SELECTED_BUILD="${BUILD_PATTERN}"
                break
            fi
        elif [ -d "${BUILD_PATTERN}" ]; then
            # Directory exists but no binary - might be incomplete build
            warning "  Directory ${BUILD_PATTERN} exists but delta binary not found"
            warning "  Run: ./installers/build_linux.sh to build"
        fi
    done
    
    # If no architecture-specific build found, try generic (but only if not already used)
    if [ "$BUILD_FOUND" = false ]; then
        GENERIC_BUILD="build_linux_release"
        if [ -f "${GENERIC_BUILD}/delta" ] && [ -x "${GENERIC_BUILD}/delta" ] && [[ ! " ${BUILD_DIRS_USED[@]} " =~ " ${GENERIC_BUILD} " ]]; then
            info "Found generic build for ${ARCH} at: ${GENERIC_BUILD}"
            # Check the actual binary architecture
            if command -v file >/dev/null 2>&1; then
                BINARY_ARCH=$(file "${GENERIC_BUILD}/delta" 2>/dev/null | grep -oE "(x86_64|amd64|arm64|aarch64)" | head -1 || echo "")
                if [ -n "$BINARY_ARCH" ]; then
                    info "  Binary architecture: ${BINARY_ARCH}"
                    # Warn if architecture doesn't match
                    case "$ARCH" in
                        amd64)
                            if [[ "$BINARY_ARCH" != "amd64" ]] && [[ "$BINARY_ARCH" != "x86_64" ]]; then
                                warning "  ⚠️  Architecture mismatch: requested ${ARCH} but binary is ${BINARY_ARCH}"
                            fi
                            ;;
                        arm64)
                            if [[ "$BINARY_ARCH" != "arm64" ]] && [[ "$BINARY_ARCH" != "aarch64" ]]; then
                                warning "  ⚠️  Architecture mismatch: requested ${ARCH} but binary is ${BINARY_ARCH}"
                            fi
                            ;;
                    esac
                fi
            fi
            warning "  ⚠️  Using same build directory for multiple architectures"
            warning "  For best results, build separately for each architecture"
            BUILDS_FOUND+=("${ARCH}:${GENERIC_BUILD}")
            BUILD_DIRS_USED+=("${GENERIC_BUILD}")
            BUILD_FOUND=true
        elif [ -d "${GENERIC_BUILD}" ] && [ ! -f "${GENERIC_BUILD}/delta" ]; then
            warning "Generic build directory exists but delta binary not found"
            warning "  Run: ./installers/build_linux.sh to build"
        elif [ -f "${GENERIC_BUILD}/delta" ]; then
            warning "Generic build found but already assigned to another architecture"
            warning "  Please create architecture-specific builds: build_linux_release_${ARCH}"
        fi
    fi
    
    if [ "$BUILD_FOUND" = false ]; then
        warning "No build found for ${ARCH}"
        warning "  Expected build directory: build_linux_release_${ARCH} or build_linux_${ARCH}"
        warning "  Or a generic build_linux_release (will be used for all missing architectures)"
    fi
done

if [ ${#BUILDS_FOUND[@]} -eq 0 ]; then
    echo ""
    error_exit "No builds found for any architecture.\n\nPlease build first:\n  ./installers/build_linux.sh\n\nOr build for specific architectures:\n  # For amd64\n  BUILD_DIR=build_linux_release_amd64 ./installers/build_linux.sh\n  # For arm64  \n  BUILD_DIR=build_linux_release_arm64 ./installers/build_linux.sh\n\nThen place the builds in:\n  build_linux_release_amd64/\n  build_linux_release_arm64/"
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
    
    # Normalize architecture name for package filename
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
    
    # Verify build exists before attempting to package
    if [ ! -f "$BUILD_DIR/delta" ]; then
        warning "Build binary not found at: $BUILD_DIR/delta"
        warning "Skipping package creation for ${ARCH}"
        echo ""
        continue
    fi
    
    # Build the package (show output so we can see errors)
    info "Running: BUILD_DIR=\"$BUILD_DIR\" ARCH=\"$ARCH\" VERSION=\"$VERSION\""
    info "Expected package: delta-cli_${VERSION}_${DEB_ARCH}.deb"
    echo ""
    
    # Capture output but also show it
    LOG_FILE="/tmp/delta-deb-build-${ARCH}.log"
    if BUILD_DIR="$BUILD_DIR" ARCH="$ARCH" VERSION="$VERSION" "$SCRIPT_DIR/package_linux_deb.sh" 2>&1 | tee "$LOG_FILE"; then
        # Find the created package using normalized architecture
        DEB_FILE="$PACKAGE_DIR/delta-cli_${VERSION}_${DEB_ARCH}.deb"
        
        # If not found with normalized name, search for it
        if [ ! -f "$DEB_FILE" ]; then
            DEB_FILE=$(find "$PACKAGE_DIR" -name "delta-cli_${VERSION}_${DEB_ARCH}.deb" 2>/dev/null | head -1)
        fi
        
        # If still not found, try any recently created deb file
        if [ ! -f "$DEB_FILE" ]; then
            DEB_FILE=$(find "$PACKAGE_DIR" -name "delta-cli_${VERSION}_*.deb" -type f -newermt "1 minute ago" 2>/dev/null | head -1)
        fi
        
        if [ -n "$DEB_FILE" ] && [ -f "$DEB_FILE" ]; then
            success "Package created: $(basename "$DEB_FILE")"
            PACKAGES_CREATED+=("$DEB_FILE")
        else
            warning "Package build reported success but file not found for ${ARCH} (${DEB_ARCH})"
            warning "Searched in: $PACKAGE_DIR"
            warning "Looking for: delta-cli_${VERSION}_${DEB_ARCH}.deb"
            if [ -d "$PACKAGE_DIR" ]; then
                info "Files in package directory:"
                ls -la "$PACKAGE_DIR" 2>/dev/null | head -10 || true
            fi
        fi
    else
        EXIT_CODE=${PIPESTATUS[0]}
        warning "Failed to build package for ${ARCH} (exit code: ${EXIT_CODE})"
        if [ -f "$LOG_FILE" ]; then
            warning "Last few lines of build log:"
            tail -20 "$LOG_FILE" 2>/dev/null || true
        fi
    fi
    
    echo ""
    
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

