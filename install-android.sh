#!/bin/bash
# Delta CLI - Complete Android Installation Script
# For Termux or Android devices with development environment

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
echo "║         Delta CLI - Android Complete Installation            ║"
echo "╚══════════════════════════════════════════════════════════════╝"
echo ""

# Check if running on Android
if [[ "$OSTYPE" != "linux-android"* ]] && [[ -z "$ANDROID_ROOT" ]]; then
    # Check if we're in Termux
    if [ ! -d "/data/data/com.termux" ] && [ -z "$PREFIX" ]; then
        warning "This script is designed for Android/Termux"
        warning "If you're using Termux, make sure you're running it from Termux"
        read -p "Continue anyway? (y/N) " -n 1 -r
        echo
        if [[ ! $REPLY =~ ^[Yy]$ ]]; then
            exit 1
        fi
    fi
fi

# Detect architecture
ARCH=$(uname -m)
info "Detected architecture: $ARCH"

# Determine Android ABI
case $ARCH in
    aarch64)
        ANDROID_ABI="arm64-v8a"
        ;;
    armv7l|armv8l)
        ANDROID_ABI="armeabi-v7a"
        ;;
    x86_64)
        ANDROID_ABI="x86_64"
        ;;
    i686)
        ANDROID_ABI="x86"
        ;;
    *)
        ANDROID_ABI="arm64-v8a"
        warning "Unknown architecture, defaulting to arm64-v8a"
        ;;
esac

info "Android ABI: $ANDROID_ABI"

# Configuration
BUILD_TYPE=${BUILD_TYPE:-Release}
ANDROID_PLATFORM=${ANDROID_PLATFORM:-21}  # Minimum API level
BUILD_DIR="build_android_${ANDROID_ABI}"

# Check if we're using Termux or Android NDK
USE_TERMUX=false
if [ -d "/data/data/com.termux" ] || [ -n "$PREFIX" ]; then
    USE_TERMUX=true
    info "Detected Termux environment"
    INSTALL_PREFIX="$PREFIX/local"
else
    # Using Android NDK
    ANDROID_NDK=${ANDROID_NDK:-$ANDROID_HOME/ndk-bundle}
    if [ -z "$ANDROID_HOME" ]; then
        ANDROID_NDK=${ANDROID_NDK:-$HOME/Android/Sdk/ndk-bundle}
    fi
    INSTALL_PREFIX="/data/local/tmp/delta"
    info "Using Android NDK: $ANDROID_NDK"
fi

# Step 1: Install dependencies (Termux)
if [ "$USE_TERMUX" = true ]; then
    info "Step 1/6: Installing Termux dependencies..."
    
    if ! command -v pkg >/dev/null 2>&1; then
        error_exit "Termux package manager (pkg) not found"
    fi
    
    info "Updating package lists..."
    pkg update -y || warning "Package update failed (continuing anyway)"
    
    info "Installing build dependencies..."
    pkg install -y \
        build-essential \
        cmake \
        git \
        curl \
        clang \
        make \
        || error_exit "Failed to install dependencies"
    
    success "Dependencies installed"
else
    # Step 1: Check Android NDK
    info "Step 1/6: Checking Android NDK..."
    
    if [ ! -d "$ANDROID_NDK" ]; then
        error_exit "Android NDK not found at $ANDROID_NDK"
        echo "Please set ANDROID_NDK environment variable or install via Android Studio"
        echo "NDK download: https://developer.android.com/ndk/downloads"
    fi
    
    if [ ! -f "$ANDROID_NDK/build/cmake/android.toolchain.cmake" ]; then
        error_exit "Android CMake toolchain not found. NDK version may be too old."
    fi
    
    success "Android NDK found"
    
    # Check for cmake
    if ! command -v cmake >/dev/null 2>&1; then
        error_exit "CMake is required. Install via Android Studio SDK Manager or system package manager."
    fi
fi

# Step 2: Verify project structure
info "Step 2/6: Verifying project structure..."
if [ ! -f "vendor/llama-cpp/CMakeLists.txt" ]; then
    error_exit "llama-cpp not found in vendor/llama-cpp/. Please ensure you're in the delta-cli directory."
fi
success "Project structure verified"

# Step 3: Build
info "Step 3/6: Building Delta CLI..."
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

if [ "$USE_TERMUX" = true ]; then
    # Termux build (native compilation)
    info "Configuring build for Termux..."
    cmake .. \
        -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
        -DCMAKE_INSTALL_PREFIX="$INSTALL_PREFIX" \
        -DUSE_CURL=ON \
        -DBUILD_TESTS=ON \
        -DBUILD_SERVER=ON \
        || error_exit "CMake configuration failed"
else
    # Android NDK build
    info "Configuring build for Android NDK..."
    cmake .. \
        -DCMAKE_TOOLCHAIN_FILE="$ANDROID_NDK/build/cmake/android.toolchain.cmake" \
        -DANDROID_ABI="$ANDROID_ABI" \
        -DANDROID_PLATFORM="android-$ANDROID_PLATFORM" \
        -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
        -DCMAKE_INSTALL_PREFIX="$INSTALL_PREFIX" \
        -DUSE_CURL=ON \
        -DBUILD_TESTS=OFF \
        -DBUILD_SERVER=ON \
        || error_exit "CMake configuration failed"
fi

# Build
info "Compiling (this may take a long time on mobile devices)..."
CPU_COUNT=$(nproc 2>/dev/null || echo "1")
cmake --build . --config "$BUILD_TYPE" -j$CPU_COUNT || error_exit "Build failed"

# Verify binary exists
if [ ! -f "delta" ]; then
    cd ..
    error_exit "Build completed but delta binary not found"
fi
success "Build completed successfully"

cd ..

# Step 3.5: Modify Web UI for Delta branding (REQUIRED)
info "Step 3.5/6: Customizing Web UI for Delta..."
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
if [ -f "$SCRIPT_DIR/modify-webui.sh" ]; then
    source "$SCRIPT_DIR/modify-webui.sh"
    modify_webui_for_delta || error_exit "Failed to customize web UI for Delta"
else
    error_exit "modify-webui.sh not found. Delta web UI customization is required."
fi

# Step 4: Install
info "Step 4/6: Installing Delta CLI..."

if [ "$USE_TERMUX" = true ]; then
    # Termux installation
    mkdir -p "$INSTALL_PREFIX/bin"
    cp "$BUILD_DIR/delta" "$INSTALL_PREFIX/bin/delta"
    chmod +x "$INSTALL_PREFIX/bin/delta"
    
    # Add to PATH if not already there
    if [[ ":$PATH:" != *":$INSTALL_PREFIX/bin:"* ]]; then
        echo "export PATH=\"$INSTALL_PREFIX/bin:\$PATH\"" >> ~/.bashrc
        export PATH="$INSTALL_PREFIX/bin:$PATH"
        info "Added $INSTALL_PREFIX/bin to PATH in ~/.bashrc"
    fi
    
    success "Delta CLI installed to $INSTALL_PREFIX/bin/delta"
else
    # Android NDK installation (manual copy required)
    warning "Android NDK build complete. Manual installation required:"
    info "1. Copy $BUILD_DIR/delta to your Android device"
    info "2. Make it executable: chmod +x delta"
    info "3. Place in a directory in your PATH or use full path"
    INSTALL_PREFIX="/data/local/tmp"
fi

# Step 5: Verify installation
info "Step 5/6: Verifying installation..."
if [ -f "$INSTALL_PREFIX/bin/delta" ] || [ -f "$BUILD_DIR/delta" ]; then
    success "Installation verified"
    
    if command -v delta >/dev/null 2>&1; then
        success "Delta CLI is available in PATH"
        DELTA_VERSION=$(delta --version 2>&1 | head -n1 || echo "unknown")
        info "Installed version: $DELTA_VERSION"
    else
        warning "Delta CLI may not be in your PATH"
        if [ "$USE_TERMUX" = true ]; then
            info "Restart Termux or run: source ~/.bashrc"
        else
            info "Use full path: $INSTALL_PREFIX/bin/delta"
        fi
    fi
else
    warning "Installation verification failed"
fi

# Step 6: Post-installation notes
info "Step 6/6: Post-installation setup..."

echo ""
echo "╔══════════════════════════════════════════════════════════════╗"
echo "║              Installation Complete!                         ║"
echo "╚══════════════════════════════════════════════════════════════╝"
echo ""
success "Delta CLI has been successfully built!"

if [ "$USE_TERMUX" = true ]; then
    echo ""
    echo "Next steps:"
    echo "  1. Restart Termux or run: source ~/.bashrc"
    echo "  2. Test installation: delta --version"
    echo "  3. Download a model: delta pull qwen3:0.6b"
    echo "  4. Start chatting: delta"
    echo ""
    echo "Note: Building and running AI models on mobile devices"
    echo "      may be slow. Consider using smaller models."
else
    echo ""
    echo "Next steps:"
    echo "  1. Copy $BUILD_DIR/delta to your Android device"
    echo "  2. Make executable: chmod +x delta"
    echo "  3. Run: ./delta --version"
    echo ""
    echo "Note: This is a development build. For production use,"
    echo "      consider creating an APK or using Android Studio."
fi
echo ""

