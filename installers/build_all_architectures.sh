#!/bin/bash
# Delta CLI - Build for all architectures
# Helps build for multiple architectures using Docker or native builds

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

# Detect current system architecture
CURRENT_ARCH=$(uname -m)
case "$CURRENT_ARCH" in
    x86_64)
        CURRENT_DEB_ARCH="amd64"
        ;;
    arm64|aarch64)
        CURRENT_DEB_ARCH="arm64"
        ;;
    *)
        CURRENT_DEB_ARCH="$CURRENT_ARCH"
        ;;
esac

echo "╔══════════════════════════════════════════════════════════════╗"
echo "║    Delta CLI - Multi-Architecture Builder                    ║"
echo "╚══════════════════════════════════════════════════════════════╝"
echo ""

info "Current system architecture: ${CURRENT_ARCH} (${CURRENT_DEB_ARCH})"
echo ""

# Check for Docker
HAS_DOCKER=false
if command -v docker >/dev/null 2>&1; then
    if docker info >/dev/null 2>&1; then
        HAS_DOCKER=true
        info "Docker is available - can build for all architectures"
    else
        warning "Docker is installed but not running"
    fi
else
    warning "Docker not found - can only build for current architecture"
fi

echo ""

# Build for current architecture (native)
info "Building for current architecture: ${CURRENT_DEB_ARCH}..."
BUILD_DIR="build_linux_release_${CURRENT_DEB_ARCH}"
if BUILD_DIR="$BUILD_DIR" ARCH="$CURRENT_DEB_ARCH" ./installers/build_linux.sh; then
    success "Build completed for ${CURRENT_DEB_ARCH}"
else
    warning "Build failed for ${CURRENT_DEB_ARCH}"
fi

echo ""

# Build for other architectures using Docker
if [ "$HAS_DOCKER" = true ]; then
    info "Building for other architectures using Docker..."
    echo ""
    
    # Build for amd64 if not current
    if [ "$CURRENT_DEB_ARCH" != "amd64" ]; then
        info "Building for amd64 using Docker..."
        if docker run --rm -v "$(pwd):/workspace" -w /workspace \
            ubuntu:22.04 bash -c "
            apt-get update -qq && \
            apt-get install -y -qq build-essential cmake git curl libcurl4-openssl-dev >/dev/null 2>&1 && \
            BUILD_DIR=build_linux_release_amd64 ARCH=amd64 ./installers/build_linux.sh
        "; then
            success "Build completed for amd64"
        else
            warning "Build failed for amd64"
        fi
        echo ""
    fi
    
    # Build for arm64 if not current
    if [ "$CURRENT_DEB_ARCH" != "arm64" ]; then
        info "Building for arm64 using Docker..."
        if docker run --rm --platform linux/arm64 -v "$(pwd):/workspace" -w /workspace \
            ubuntu:22.04 bash -c "
            apt-get update -qq && \
            apt-get install -y -qq build-essential cmake git curl libcurl4-openssl-dev >/dev/null 2>&1 && \
            BUILD_DIR=build_linux_release_arm64 ARCH=arm64 ./installers/build_linux.sh
        "; then
            success "Build completed for arm64"
        else
            warning "Build failed for arm64"
        fi
        echo ""
    fi
else
    warning "Docker not available - cannot build for other architectures"
    warning "To build for all architectures:"
    echo ""
    echo "  Option 1: Install Docker and run this script again"
    echo "  Option 2: Build on systems with different architectures"
    echo "  Option 3: Use cross-compilation (advanced)"
    echo ""
fi

echo ""
info "Build summary:"
for ARCH in amd64 arm64; do
    BUILD_DIR="build_linux_release_${ARCH}"
    if [ -f "${BUILD_DIR}/delta" ]; then
        success "  ${ARCH}: ${BUILD_DIR}/delta exists"
    else
        warning "  ${ARCH}: No build found"
    fi
done

echo ""
info "Next step: Create .deb packages"
echo "  ./installers/package_all_architectures.sh"
echo ""

success "Done!"

