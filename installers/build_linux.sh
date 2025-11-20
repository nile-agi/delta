#!/bin/bash
# Delta CLI - Linux Build Script for Release
# Builds the application for Linux distribution

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
echo "║         Delta CLI - Linux Release Build                      ║"
echo "╚══════════════════════════════════════════════════════════════╝"
echo ""

# Get script directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
cd "$PROJECT_DIR"

# Configuration
BUILD_TYPE=${BUILD_TYPE:-Release}
BUILD_DIR="build_linux_release"
ARCH=$(uname -m)

# Step 1: Check dependencies
info "Step 1/6: Checking dependencies..."

# Check for CMake
if ! command -v cmake >/dev/null 2>&1; then
    error_exit "CMake not found. Please install: sudo apt-get install cmake"
fi
CMAKE_VERSION=$(cmake --version | head -1 | cut -d' ' -f3)
info "CMake version: $CMAKE_VERSION"

# Check for C++ compiler
if command -v g++ >/dev/null 2>&1; then
    CXX_COMPILER="g++"
    C_COMPILER="gcc"
    success "Found GCC compiler"
elif command -v clang++ >/dev/null 2>&1; then
    CXX_COMPILER="clang++"
    C_COMPILER="clang"
    success "Found Clang compiler"
else
    error_exit "No C++ compiler found. Please install: sudo apt-get install build-essential"
fi

# Check for Node.js (for web UI)
if command -v npm >/dev/null 2>&1; then
    NODE_VERSION=$(node --version)
    success "Node.js version: $NODE_VERSION"
    HAS_NODE=true
else
    warning "Node.js not found. Web UI will not be built."
    warning "Install with: sudo apt-get install nodejs npm"
    HAS_NODE=false
fi

# Check for required libraries
MISSING_DEPS=()
if ! pkg-config --exists libcurl 2>/dev/null && ! ldconfig -p | grep -q libcurl; then
    MISSING_DEPS+=("libcurl4-openssl-dev")
fi

if [ ${#MISSING_DEPS[@]} -gt 0 ]; then
    warning "Missing dependencies: ${MISSING_DEPS[*]}"
    warning "Install with: sudo apt-get install ${MISSING_DEPS[*]}"
fi

# Step 2: Build web UI (if Node.js is available)
if [ "$HAS_NODE" = true ]; then
    info "Step 2/6: Building web UI..."
    if [ -d "assets" ] && [ -f "assets/package.json" ]; then
        cd assets
        if [ ! -d "node_modules" ]; then
            info "Installing npm dependencies..."
            npm install --silent || warning "npm install failed, continuing..."
        fi
        if npm run build >/dev/null 2>&1; then
            success "Web UI built successfully"
        else
            warning "Web UI build failed, continuing without it"
        fi
        cd "$PROJECT_DIR"
    else
        warning "Web UI source not found, skipping..."
    fi
else
    info "Step 2/6: Skipping web UI (Node.js not available)"
fi

# Step 3: Create build directory
info "Step 3/6: Creating build directory..."
rm -rf "$BUILD_DIR"
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# Step 4: Configure with CMake
info "Step 4/6: Configuring with CMake..."
cmake .. \
    -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
    -DCMAKE_C_COMPILER="$C_COMPILER" \
    -DCMAKE_CXX_COMPILER="$CXX_COMPILER" \
    -DUSE_CURL=ON \
    -DBUILD_TESTS=OFF \
    -DBUILD_SERVER=ON \
    || error_exit "CMake configuration failed"

# Step 5: Build
info "Step 5/6: Building (this may take a while)..."
CORES=$(nproc 2>/dev/null || echo 4)
if cmake --build . --config "$BUILD_TYPE" -j "$CORES"; then
    success "Build completed successfully"
else
    error_exit "Build failed"
fi

# Step 6: Verify build
info "Step 6/6: Verifying build..."
if [ -f "delta" ]; then
    success "Binary created: $BUILD_DIR/delta"
    file delta
    ls -lh delta
    if [ -f "delta-server" ]; then
        success "Server binary created: $BUILD_DIR/delta-server"
        ls -lh delta-server
    fi
else
    error_exit "Build verification failed: delta binary not found"
fi

cd "$PROJECT_DIR"

echo ""
success "✅ Build completed successfully!"
echo ""
info "Build output: $BUILD_DIR/"
info "To create a .deb package, run: ./installers/package_linux_deb.sh"
echo ""

