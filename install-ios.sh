#!/bin/bash
# Delta CLI - Complete iOS Installation Script
# For iOS devices (requires Xcode and iOS development setup)

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
echo "║         Delta CLI - iOS Complete Installation                ║"
echo "╚══════════════════════════════════════════════════════════════╝"
echo ""

# Check if running on macOS (required for iOS development)
if [[ "$OSTYPE" != "darwin"* ]]; then
    error_exit "iOS development requires macOS with Xcode. This script must be run on macOS."
fi

# Configuration
BUILD_TYPE=${BUILD_TYPE:-Release}
IOS_PLATFORM=${IOS_PLATFORM:-OS64}  # OS64 (device) or SIMULATOR64
BUILD_DIR="build_ios_${IOS_PLATFORM}"
DEPLOYMENT_TARGET=${DEPLOYMENT_TARGET:-14.0}

info "Building for iOS:"
info "  Platform: $IOS_PLATFORM"
info "  Deployment Target: iOS $DEPLOYMENT_TARGET"

# Step 1: Check for Xcode
info "Step 1/7: Checking for Xcode..."
if ! command -v xcodebuild >/dev/null 2>&1; then
    error_exit "Xcode is required but not installed."
    echo "Please install Xcode from the App Store:"
    echo "  https://apps.apple.com/app/xcode/id497799835"
fi

XCODE_VERSION=$(xcodebuild -version | head -n1)
info "Found: $XCODE_VERSION"

# Verify Xcode license
info "Verifying Xcode license..."
sudo xcodebuild -license accept 2>/dev/null || warning "Xcode license may need to be accepted manually"

# Step 2: Check for iOS CMake toolchain
info "Step 2/7: Checking for iOS CMake toolchain..."
TOOLCHAIN_FILE="vendor/ios-cmake/ios.toolchain.cmake"

if [ ! -f "$TOOLCHAIN_FILE" ]; then
    warning "iOS CMake toolchain not found. Downloading..."
    mkdir -p vendor
    cd vendor
    
    if [ -d "ios-cmake" ]; then
        info "Updating existing ios-cmake..."
        cd ios-cmake
        git pull || warning "Failed to update (continuing anyway)"
        cd ..
    else
        info "Cloning ios-cmake..."
        git clone https://github.com/leetal/ios-cmake.git || error_exit "Failed to clone ios-cmake"
    fi
    
    cd ..
    
    if [ ! -f "$TOOLCHAIN_FILE" ]; then
        error_exit "Failed to obtain iOS CMake toolchain"
    fi
fi
success "iOS CMake toolchain found"

# Step 3: Check for CMake
info "Step 3/7: Checking for CMake..."
if ! command -v cmake >/dev/null 2>&1; then
    # Try to install via Homebrew
    if command -v brew >/dev/null 2>&1; then
        info "Installing CMake via Homebrew..."
        brew install cmake || error_exit "Failed to install CMake"
    else
        error_exit "CMake is required. Install via Homebrew: brew install cmake"
    fi
fi

CMAKE_VERSION=$(cmake --version | head -n1 | cut -d' ' -f3)
info "CMake version: $CMAKE_VERSION"
success "CMake found"

# Step 4: Check for Git
info "Step 4/7: Checking for Git..."
if ! command -v git >/dev/null 2>&1; then
    error_exit "Git is required. Install Xcode Command Line Tools: xcode-select --install"
fi
success "Git found"

# Step 5: Verify project structure
info "Step 5/7: Verifying project structure..."
if [ ! -f "vendor/llama.cpp/CMakeLists.txt" ]; then
    error_exit "llama.cpp not found in vendor/llama.cpp/. Please ensure you're in the delta-cli directory."
fi
success "Project structure verified"

# Step 6: Build
info "Step 6/7: Building Delta CLI for iOS..."
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# Configure
info "Configuring build for iOS..."
cmake .. \
    -DCMAKE_TOOLCHAIN_FILE="../$TOOLCHAIN_FILE" \
    -DPLATFORM="$IOS_PLATFORM" \
    -DDEPLOYMENT_TARGET="$DEPLOYMENT_TARGET" \
    -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
    -DBUILD_TESTS=OFF \
    -DBUILD_SERVER=ON \
    -DUSE_CURL=ON \
    || error_exit "CMake configuration failed"

# Build
info "Compiling (this may take a long time)..."
CPU_COUNT=$(sysctl -n hw.ncpu)
cmake --build . --config "$BUILD_TYPE" -j$CPU_COUNT || error_exit "Build failed"

# Verify binary exists
if [ ! -f "delta" ]; then
    cd ..
    error_exit "Build completed but delta binary not found"
fi
success "Build completed successfully"

cd ..

# Step 6.5: Modify Web UI for Delta branding (REQUIRED)
info "Step 6.5/7: Customizing Web UI for Delta..."
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
if [ -f "$SCRIPT_DIR/modify-webui.sh" ]; then
    source "$SCRIPT_DIR/modify-webui.sh"
    modify_webui_for_delta || error_exit "Failed to customize web UI for Delta"
else
    error_exit "modify-webui.sh not found. Delta web UI customization is required."
fi

# Step 7: Installation notes
info "Step 7/7: Installation instructions..."

echo ""
echo "╔══════════════════════════════════════════════════════════════╗"
echo "║              Build Complete!                                ║"
echo "╚══════════════════════════════════════════════════════════════╝"
echo ""
success "Delta CLI has been successfully built for iOS!"
echo ""
echo "⚠️  IMPORTANT: iOS Installation Notes"
echo ""
echo "The binary has been built but cannot be directly installed like"
echo "on other platforms. iOS requires code signing and app packaging."
echo ""
echo "Next steps:"
echo ""
echo "Option 1: Use in Xcode Project"
echo "  1. Create a new iOS app project in Xcode"
echo "  2. Add $BUILD_DIR/delta as a framework or library"
echo "  3. Configure code signing and build"
echo ""
echo "Option 2: Create iOS Framework"
echo "  1. Use the built binary in an Xcode project"
echo "  2. Create a framework target"
echo "  3. Link against the delta binary"
echo ""
echo "Option 3: Command Line Tool (Jailbroken devices only)"
echo "  1. Copy $BUILD_DIR/delta to your device"
echo "  2. Place in /usr/local/bin or similar"
echo "  3. Make executable: chmod +x delta"
echo ""
echo "Binary location: $BUILD_DIR/delta"
echo "Platform: $IOS_PLATFORM"
echo "Deployment Target: iOS $DEPLOYMENT_TARGET"
echo ""
echo "For more information on iOS development, see:"
echo "  https://developer.apple.com/ios/"
echo ""

