#!/bin/bash
# Delta CLI - Build .deb packages for all architectures
# Creates separate .deb files for each supported architecture
# Intelligently matches builds to architectures based on binary architecture

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
ARCHITECTURES=("amd64" "arm64" "armhf")

echo "╔══════════════════════════════════════════════════════════════╗"
echo "║    Delta CLI - Multi-Architecture .deb Package Builder       ║"
echo "╚══════════════════════════════════════════════════════════════╝"
echo ""

info "This script will create .deb packages for multiple architectures."
info "Each architecture will have its own installable .deb file."
echo ""

# Function to detect binary architecture
detect_binary_arch() {
    local binary_path="$1"
    local arch=""
    
    if [ ! -f "$binary_path" ]; then
        echo ""
        return
    fi
    
    if command -v file >/dev/null 2>&1; then
        FILE_OUTPUT=$(file "$binary_path" 2>/dev/null || echo "")
        # Extract architecture from file output (order matters - check specific first)
        if echo "$FILE_OUTPUT" | grep -qiE "(x86_64|amd64|Intel 80386)"; then
            arch="amd64"
        elif echo "$FILE_OUTPUT" | grep -qiE "(aarch64|arm64|ARM.*aarch64)"; then
            arch="arm64"
        elif echo "$FILE_OUTPUT" | grep -qiE "(armv7|armhf|ARM.*v7)"; then
            arch="armhf"
        elif echo "$FILE_OUTPUT" | grep -qiE "ARM"; then
            arch="arm64"  # Default to 64-bit for generic ARM
        fi
    fi
    
    echo "$arch"
}

# Step 1: Scan for all available builds and detect their architectures
info "Step 1: Scanning for available builds..."
AVAILABLE_BUILDS=()  # Array of "BUILD_DIR:BINARY_ARCH"

# Check all possible build directories
ALL_BUILD_DIRS=(
    "build_linux_release_amd64"
    "build_linux_release_x86_64"
    "build_linux_release_arm64"
    "build_linux_release_aarch64"
    "build_linux_release_armhf"
    "build_linux_release_arm"
    "build_linux_release_armv7l"
    "build_linux_amd64"
    "build_linux_arm64"
    "build_linux_armhf"
    "build_amd64"
    "build_arm64"
    "build_armhf"
    "build_linux_release"  # Generic build (check last)
)

for BUILD_DIR in "${ALL_BUILD_DIRS[@]}"; do
    if [ -f "${BUILD_DIR}/delta" ] && [ -x "${BUILD_DIR}/delta" ]; then
        BINARY_ARCH=$(detect_binary_arch "${BUILD_DIR}/delta")
        if [ -n "$BINARY_ARCH" ]; then
            BUILD_ENTRY="${BUILD_DIR}:${BINARY_ARCH}"
            # Only add if not already in list
            if [[ ! " ${AVAILABLE_BUILDS[@]} " =~ " ${BUILD_ENTRY} " ]]; then
                AVAILABLE_BUILDS+=("$BUILD_ENTRY")
                info "Found build: ${BUILD_DIR} → ${BINARY_ARCH}"
            fi
        else
            warning "Found build at ${BUILD_DIR} but could not detect architecture"
        fi
    fi
done

if [ ${#AVAILABLE_BUILDS[@]} -eq 0 ]; then
    echo ""
    error_exit "No builds found. Please build first:\n  ./installers/build_linux.sh\n\nOr build for specific architectures:\n  BUILD_DIR=build_linux_release_amd64 ARCH=amd64 ./installers/build_linux.sh"
fi

echo ""
info "Found ${#AVAILABLE_BUILDS[@]} build(s):"
for BUILD_ENTRY in "${AVAILABLE_BUILDS[@]}"; do
    BUILD_DIR="${BUILD_ENTRY%%:*}"
    BUILD_BINARY_ARCH="${BUILD_ENTRY##*:}"
    echo "  • ${BUILD_DIR} → ${BUILD_BINARY_ARCH}"
done
echo ""

# Step 2: Match builds to requested architectures
info "Step 2: Matching builds to requested architectures..."
BUILDS_FOUND=()
BUILD_DIRS_USED=()
MISSING_ARCHES=()
MISSING_ARCHES=()

for ARCH in "${ARCHITECTURES[@]}"; do
    # Normalize architecture for matching
    case "$ARCH" in
        amd64)
            MATCH_ARCHES=("amd64")
            ;;
        arm64)
            MATCH_ARCHES=("arm64" "aarch64")
            ;;
        armhf)
            MATCH_ARCHES=("armhf" "armv7" "armv7l")
            ;;
        *)
            MATCH_ARCHES=("$ARCH")
            ;;
    esac
    
    BUILD_FOUND=false
    BEST_MATCH=""
    BEST_MATCH_DIR=""
    
    # Find the best matching build for this architecture
    for BUILD_ENTRY in "${AVAILABLE_BUILDS[@]}"; do
        BUILD_DIR="${BUILD_ENTRY%%:*}"
        BUILD_BINARY_ARCH="${BUILD_ENTRY##*:}"
        
        # Skip if this build directory is already assigned
        if [[ " ${BUILD_DIRS_USED[@]} " =~ " ${BUILD_DIR} " ]]; then
            continue
        fi
        
        # Check if binary architecture matches requested architecture
        MATCHES=false
        for MATCH_ARCH in "${MATCH_ARCHES[@]}"; do
            if [ "$BUILD_BINARY_ARCH" = "$MATCH_ARCH" ]; then
                MATCHES=true
                break
            fi
        done
        
        # Also check directory name as secondary indicator
        if [ "$MATCHES" = false ]; then
            case "$ARCH" in
                amd64)
                    if [[ "$BUILD_DIR" == *"amd64"* ]] || [[ "$BUILD_DIR" == *"x86_64"* ]]; then
                        MATCHES=true
                    fi
                    ;;
                arm64)
                    if [[ "$BUILD_DIR" == *"arm64"* ]] || [[ "$BUILD_DIR" == *"aarch64"* ]]; then
                        MATCHES=true
                    fi
                    ;;
                armhf)
                    if [[ "$BUILD_DIR" == *"armhf"* ]] || [[ "$BUILD_DIR" == *"armv7"* ]]; then
                        MATCHES=true
                    fi
                    ;;
            esac
        fi
        
        if [ "$MATCHES" = true ]; then
            # Prefer architecture-specific directories over generic
            if [[ "$BUILD_DIR" == "build_linux_release" ]]; then
                # Generic build - use as fallback
                if [ -z "$BEST_MATCH" ]; then
                    BEST_MATCH="$BUILD_ENTRY"
                    BEST_MATCH_DIR="$BUILD_DIR"
                fi
            else
                # Architecture-specific - prefer this
                BEST_MATCH="$BUILD_ENTRY"
                BEST_MATCH_DIR="$BUILD_DIR"
                break  # Found specific match, use it
            fi
        fi
    done
    
    if [ -n "$BEST_MATCH" ]; then
        BUILD_DIR="${BEST_MATCH%%:*}"
        BUILD_BINARY_ARCH="${BEST_MATCH##*:}"
        info "Matched ${ARCH} → ${BUILD_DIR} (binary: ${BUILD_BINARY_ARCH})"
        BUILDS_FOUND+=("${ARCH}:${BUILD_DIR}")
        BUILD_DIRS_USED+=("${BUILD_DIR}")
        BUILD_FOUND=true
    else
        warning "No matching build found for ${ARCH}"
        warning "  Need a build with architecture: ${ARCH}"
        MISSING_ARCHES+=("$ARCH")
    fi
done

if [ ${#BUILDS_FOUND[@]} -eq 0 ]; then
    echo ""
    error_exit "No matching builds found for any requested architecture.\n\nAvailable builds:\n$(printf '  %s\n' "${AVAILABLE_BUILDS[@]}")\n\nPlease build for the architectures you need."
fi

echo ""
info "Matched ${#BUILDS_FOUND[@]} architecture(s) to builds"
echo ""

# Step 3: Create packages directory
mkdir -p "$PACKAGE_DIR"

# Step 4: Build package for each matched architecture
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
        armv7l|armv7|arm)
            DEB_ARCH="armhf"
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
    
    # Show missing architectures
    if [ ${#MISSING_ARCHES[@]} -gt 0 ]; then
        warning "⚠️  Missing packages for ${#MISSING_ARCHES[@]} architecture(s):"
        for MISSING_ARCH in "${MISSING_ARCHES[@]}"; do
            echo "    - ${MISSING_ARCH}"
        done
        echo ""
        info "To create packages for missing architectures, build for those architectures first:"
        echo ""
        for MISSING_ARCH in "${MISSING_ARCHES[@]}"; do
            case "$MISSING_ARCH" in
                amd64)
                    echo "  # Build for amd64:"
                    echo "  BUILD_DIR=build_linux_release_amd64 ARCH=amd64 ./installers/build_linux.sh"
                    echo "  BUILD_DIR=build_linux_release_amd64 ARCH=amd64 ./installers/package_linux_deb.sh"
                    echo ""
                    ;;
                arm64)
                    echo "  # Build for arm64:"
                    echo "  BUILD_DIR=build_linux_release_arm64 ARCH=arm64 ./installers/build_linux.sh"
                    echo "  BUILD_DIR=build_linux_release_arm64 ARCH=arm64 ./installers/package_linux_deb.sh"
                    echo ""
                    ;;
                armhf)
                    echo "  # Build for armhf:"
                    echo "  BUILD_DIR=build_linux_release_armhf ARCH=armhf ./installers/build_linux.sh"
                    echo "  BUILD_DIR=build_linux_release_armhf ARCH=armhf ./installers/package_linux_deb.sh"
                    echo ""
                    ;;
            esac
        done
        echo "  Or run this script again after building:"
        echo "    ./installers/package_all_architectures.sh"
        echo ""
    fi
    
    info "Package location: $PACKAGE_DIR"
    echo ""
    info "Installation instructions:"
    echo ""
    
    # Only show installation instructions for packages that were created
    CREATED_ARCHES=()
    for DEB_FILE in "${PACKAGES_CREATED[@]}"; do
        if [[ "$DEB_FILE" == *"_amd64.deb" ]]; then
            CREATED_ARCHES+=("amd64")
        elif [[ "$DEB_FILE" == *"_arm64.deb" ]]; then
            CREATED_ARCHES+=("arm64")
        elif [[ "$DEB_FILE" == *"_armhf.deb" ]]; then
            CREATED_ARCHES+=("armhf")
        fi
    done
    
    if [[ " ${CREATED_ARCHES[@]} " =~ " amd64 " ]]; then
        echo "For amd64/x86_64 systems:"
        echo "  sudo dpkg -i $PACKAGE_DIR/delta-cli_${VERSION}_amd64.deb"
        echo ""
    fi
    
    if [[ " ${CREATED_ARCHES[@]} " =~ " arm64 " ]]; then
        echo "For arm64/aarch64 systems:"
        echo "  sudo dpkg -i $PACKAGE_DIR/delta-cli_${VERSION}_arm64.deb"
        echo ""
    fi
    
    if [[ " ${CREATED_ARCHES[@]} " =~ " armhf " ]]; then
        echo "For armhf (32-bit ARM) systems:"
        echo "  sudo dpkg -i $PACKAGE_DIR/delta-cli_${VERSION}_armhf.deb"
        echo ""
    fi
    
    echo "To check your system architecture:"
    echo "  dpkg --print-architecture  # On Debian/Ubuntu"
    echo "  uname -m                    # On any Linux"
    echo ""
    warning "⚠️  CRITICAL: Make sure to install the package matching your system architecture!"
    warning "   Installing the wrong architecture will cause 'Exec format error'"
    echo ""
else
    error_exit "No packages were created. Check build directories and try again."
fi

success "Done!"
