#!/bin/bash
# Delta CLI - macOS Build Script for Release
# Builds the application for macOS distribution

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
echo "║         Delta CLI - macOS Release Build                       ║"
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
BUILD_TYPE=${BUILD_TYPE:-Release}
BUILD_DIR="build_macos_release"
ARCH=$(uname -m)

info "Detected architecture: $ARCH"
info "Build type: $BUILD_TYPE"

# Step 1: Check for Xcode Command Line Tools
info "Step 1/5: Checking Xcode Command Line Tools..."
if ! command -v clang++ >/dev/null 2>&1; then
    error_exit "Xcode Command Line Tools not found. Install with: xcode-select --install"
fi

# Verify that xcode-select is properly configured
XCODE_PATH=$(xcode-select -p 2>/dev/null || echo "")
if [ -z "$XCODE_PATH" ] || [ ! -d "$XCODE_PATH" ]; then
    error_exit "Xcode Command Line Tools not properly configured. Run: xcode-select --install"
fi

# Test that the compiler actually works
if ! /usr/bin/clang --version >/dev/null 2>&1; then
    error_exit "Xcode Command Line Tools not working. Please run: xcode-select --install\nThen restart your terminal and try again."
fi

success "Xcode Command Line Tools found and working"

# Step 2: Check dependencies
info "Step 2/5: Checking dependencies..."
DEPENDENCIES="cmake git curl"
MISSING_DEPS=""

for dep in $DEPENDENCIES; do
    if ! command -v $dep >/dev/null 2>&1; then
        MISSING_DEPS="$MISSING_DEPS $dep"
    fi
done

if [ -n "$MISSING_DEPS" ]; then
    warning "Missing dependencies:$MISSING_DEPS"
    if command -v brew >/dev/null 2>&1; then
        info "Installing via Homebrew..."
        brew install $MISSING_DEPS || error_exit "Failed to install dependencies"
    else
        error_exit "Please install missing dependencies: $MISSING_DEPS"
    fi
fi
success "All dependencies available"

# Step 3: Verify project structure
info "Step 3/5: Verifying project structure..."
if [ ! -f "vendor/llama.cpp/CMakeLists.txt" ]; then
    error_exit "llama.cpp not found in vendor/llama.cpp/. Please ensure you're in the delta directory."
fi
success "Project structure verified"

# Step 4: Build web UI
info "Step 4/5: Building web UI..."
if [ -d "assets" ] && [ -f "assets/package.json" ]; then
    cd assets
    if [ ! -d "node_modules" ]; then
        if ! command -v npm >/dev/null 2>&1; then
            error_exit "npm not found. Install Node.js to build the web UI."
        fi
        info "Installing web UI dependencies..."
        npm install || error_exit "Failed to install web UI dependencies"
    fi
    info "Building SvelteKit app..."
    npm run build || error_exit "Failed to build web UI"
    cd ..
    success "Web UI built successfully"
else
    warning "assets/ directory not found. Web UI will not be included."
fi

# Step 5: Patch visionOS issues (must be done before building)
info "Step 5/6: Patching visionOS compatibility issues..."
# Work around visionOS compatibility issues in newer SDKs
# The Accelerate framework headers and some Metal code reference visionOS which older compilers don't recognize
# Since we're using Metal (the primary acceleration on macOS), we can disable BLAS/Accelerate
# Patch the source file to remove visionOS references before building
METAL_DEVICE_FILE="${PROJECT_DIR}/vendor/llama.cpp/ggml/src/ggml-metal/ggml-metal-device.m"
if [ -f "$METAL_DEVICE_FILE" ]; then
    # Create backup if it doesn't exist
    if [ ! -f "${METAL_DEVICE_FILE}.backup" ]; then
        cp "$METAL_DEVICE_FILE" "${METAL_DEVICE_FILE}.backup" 2>/dev/null || true
    fi
    # Remove visionOS from @available checks (replace with just the other platforms)
    # Try macOS sed syntax first, then fall back to GNU sed syntax
    if sed -i '' 's/@available(macOS 15.0, iOS 18.0, tvOS 18.0, visionOS 2.0, \*)/@available(macOS 15.0, iOS 18.0, tvOS 18.0, *)/g' "$METAL_DEVICE_FILE" 2>/dev/null; then
        success "Patched Metal device file (removed visionOS references)"
    elif sed -i 's/@available(macOS 15.0, iOS 18.0, tvOS 18.0, visionOS 2.0, \*)/@available(macOS 15.0, iOS 18.0, tvOS 18.0, *)/g' "$METAL_DEVICE_FILE" 2>/dev/null; then
        success "Patched Metal device file (removed visionOS references)"
    else
        # Fallback: use perl or python if sed doesn't work
        perl -i -pe 's/@available\(macOS 15\.0, iOS 18\.0, tvOS 18\.0, visionOS 2\.0, \*\)/@available(macOS 15.0, iOS 18.0, tvOS 18.0, *)/g' "$METAL_DEVICE_FILE" 2>/dev/null && \
        success "Patched Metal device file (removed visionOS references)" || \
        warning "Could not patch Metal device file automatically"
    fi
else
    warning "Metal device file not found at: $METAL_DEVICE_FILE"
    warning "Trying alternative path..."
    # Try relative path from current directory (we're in PROJECT_DIR)
    METAL_DEVICE_FILE="vendor/llama.cpp/ggml/src/ggml-metal/ggml-metal-device.m"
    if [ -f "$METAL_DEVICE_FILE" ]; then
        if [ ! -f "${METAL_DEVICE_FILE}.backup" ]; then
            cp "$METAL_DEVICE_FILE" "${METAL_DEVICE_FILE}.backup" 2>/dev/null || true
        fi
        if sed -i '' 's/@available(macOS 15.0, iOS 18.0, tvOS 18.0, visionOS 2.0, \*)/@available(macOS 15.0, iOS 18.0, tvOS 18.0, *)/g' "$METAL_DEVICE_FILE" 2>/dev/null || \
           sed -i 's/@available(macOS 15.0, iOS 18.0, tvOS 18.0, visionOS 2.0, \*)/@available(macOS 15.0, iOS 18.0, tvOS 18.0, *)/g' "$METAL_DEVICE_FILE" 2>/dev/null || \
           perl -i -pe 's/@available\(macOS 15\.0, iOS 18\.0, tvOS 18\.0, visionOS 2\.0, \*\)/@available(macOS 15.0, iOS 18.0, tvOS 18.0, *)/g' "$METAL_DEVICE_FILE" 2>/dev/null; then
            success "Patched Metal device file (removed visionOS references)"
        else
            warning "Could not patch Metal device file automatically"
        fi
    else
        warning "Metal device file not found, build may fail with visionOS errors"
    fi
fi

# Step 6: Build application
info "Step 6/6: Building Delta CLI..."
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# Configure build
info "Configuring build..."

# Find the actual compiler paths
C_COMPILER=$(which clang 2>/dev/null || echo "/usr/bin/clang")
CXX_COMPILER=$(which clang++ 2>/dev/null || echo "/usr/bin/clang++")

# Verify compilers exist and work
if [ ! -f "$C_COMPILER" ] || ! "$C_COMPILER" --version >/dev/null 2>&1; then
    error_exit "C compiler not found or not working. Please install Xcode Command Line Tools: xcode-select --install"
fi

if [ ! -f "$CXX_COMPILER" ] || ! "$CXX_COMPILER" --version >/dev/null 2>&1; then
    error_exit "C++ compiler not found or not working. Please install Xcode Command Line Tools: xcode-select --install"
fi

info "Using C compiler: $C_COMPILER"
info "Using C++ compiler: $CXX_COMPILER"

# Also use compiler flags as backup
# Suppress deprecation warnings from dependencies (like llama.cpp using deprecated C++17 features)
export CFLAGS="${CFLAGS} -Wno-error -Wno-deprecated-declarations"
export CXXFLAGS="${CXXFLAGS} -Wno-error -Wno-deprecated-declarations"
export OBJCFLAGS="${OBJCFLAGS} -Wno-error"

info "Configuring build (Metal enabled, BLAS/Accelerate disabled to avoid SDK compatibility issues)..."

cmake .. \
    -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
    -DCMAKE_C_COMPILER="$C_COMPILER" \
    -DCMAKE_CXX_COMPILER="$CXX_COMPILER" \
    -DCMAKE_C_FLAGS="${CFLAGS}" \
    -DCMAKE_CXX_FLAGS="${CXXFLAGS}" \
    -DCMAKE_OBJC_FLAGS="${OBJCFLAGS}" \
    -DGGML_METAL=ON \
    -DGGML_BLAS=OFF \
    -DGGML_ACCELERATE=OFF \
    -DGGML_CCACHE=OFF \
    -DGGML_OPENMP=OFF \
    -DUSE_CURL=ON \
    -DBUILD_TESTS=OFF \
    -DBUILD_SERVER=ON \
    || error_exit "CMake configuration failed. Make sure Xcode Command Line Tools are properly installed: xcode-select --install"

# Build
info "Compiling (this may take several minutes)..."
CPU_COUNT=$(sysctl -n hw.ncpu)
cmake --build . --config "$BUILD_TYPE" -j$CPU_COUNT || error_exit "Build failed"

# Verify binaries exist
if [ ! -f "delta" ]; then
    error_exit "Build completed but delta binary not found"
fi
if [ ! -f "delta-server" ]; then
    warning "delta-server binary not found (this is optional)"
fi

success "Build completed successfully"
cd ..

echo ""
echo "╔══════════════════════════════════════════════════════════════╗"
echo "║              Build Complete!                                 ║"
echo "╚══════════════════════════════════════════════════════════════╝"
echo ""
info "Binaries are in: $BUILD_DIR/"
info "Next step: Run ./installers/package_macos.sh to create .dmg installer"
echo ""

