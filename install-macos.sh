#!/bin/bash
# Delta CLI - Complete macOS Installation Script
# Installs all dependencies, builds, and installs system-wide

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
echo "║         Delta CLI - macOS Complete Installation              ║"
echo "╚══════════════════════════════════════════════════════════════╝"
echo ""

# Check if running on macOS
if [[ "$OSTYPE" != "darwin"* ]]; then
    error_exit "This script is for macOS only. Use install-linux.sh for Linux."
fi

# Detect architecture
ARCH=$(uname -m)
info "Detected architecture: $ARCH"

# Configuration
BUILD_TYPE=${BUILD_TYPE:-Release}
INSTALL_PREFIX=${INSTALL_PREFIX:-/usr/local}
BUILD_DIR="build_macos"

# Step 1: Check for Xcode Command Line Tools
info "Step 1/7: Checking Xcode Command Line Tools..."
if ! command -v clang++ >/dev/null 2>&1; then
    warning "Xcode Command Line Tools not found. Installing..."
    xcode-select --install || error_exit "Failed to install Xcode Command Line Tools. Please install manually: xcode-select --install"
    echo "Please complete the Xcode Command Line Tools installation, then run this script again."
    exit 0
fi
success "Xcode Command Line Tools found"

# Step 2: Check/Install Homebrew
info "Step 2/7: Checking Homebrew..."
    if ! command -v brew >/dev/null 2>&1; then
    warning "Homebrew not found. Installing..."
    /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)" || error_exit "Failed to install Homebrew"
    
    # Add Homebrew to PATH for Apple Silicon
    if [[ "$ARCH" == "arm64" ]]; then
        echo 'eval "$(/opt/homebrew/bin/brew shellenv)"' >> ~/.zprofile
        eval "$(/opt/homebrew/bin/brew shellenv)"
    fi
    success "Homebrew installed"
else
    success "Homebrew found"
    brew update || warning "Failed to update Homebrew (continuing anyway)"
fi

# Step 3: Install dependencies
info "Step 3/7: Installing dependencies..."
DEPENDENCIES="cmake git curl"
MISSING_DEPS=""

for dep in $DEPENDENCIES; do
    if ! command -v $dep >/dev/null 2>&1; then
        MISSING_DEPS="$MISSING_DEPS $dep"
    fi
done

if [ -n "$MISSING_DEPS" ]; then
    info "Installing missing dependencies:$MISSING_DEPS"
    brew install $MISSING_DEPS || error_exit "Failed to install dependencies"
    success "Dependencies installed"
else
    success "All dependencies already installed"
fi

# Verify cmake version (need 3.14+)
CMAKE_VERSION=$(cmake --version | head -n1 | cut -d' ' -f3)
info "CMake version: $CMAKE_VERSION"

# Step 4: Check for vendored llama.cpp
info "Step 4/7: Verifying project structure..."
if [ ! -f "vendor/llama.cpp/CMakeLists.txt" ]; then
    error_exit "llama.cpp not found in vendor/llama.cpp/. Please ensure you're in the delta-cli directory."
fi
success "Project structure verified"

# Step 5: Build
info "Step 5/7: Building Delta CLI..."
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# Detect Metal support
if [ "$ARCH" = "arm64" ]; then
    ENABLE_METAL=ON
    info "Building for Apple Silicon (Metal acceleration enabled)"
else
    ENABLE_METAL=ON
    info "Building for Intel Mac (Metal acceleration enabled)"
fi

# Check for curl
USE_CURL=OFF
if command -v curl-config >/dev/null 2>&1; then
    USE_CURL=ON
    info "libcurl found (telemetry enabled)"
fi

# Configure
info "Configuring build..."
cmake .. \
    -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
    -DCMAKE_INSTALL_PREFIX="$INSTALL_PREFIX" \
    -DCMAKE_C_COMPILER=/usr/bin/clang \
    -DCMAKE_CXX_COMPILER=/usr/bin/clang++ \
    -DGGML_METAL="$ENABLE_METAL" \
    -DGGML_CCACHE=OFF \
    -DGGML_OPENMP=OFF \
    -DUSE_CURL="$USE_CURL" \
    -DBUILD_TESTS=ON \
    -DBUILD_SERVER=ON \
    || error_exit "CMake configuration failed"

# Build
info "Compiling (this may take several minutes)..."
CPU_COUNT=$(sysctl -n hw.ncpu)
cmake --build . --config "$BUILD_TYPE" -j$CPU_COUNT || error_exit "Build failed"

# Verify binary exists
if [ ! -f "delta" ]; then
    error_exit "Build completed but delta binary not found"
fi
success "Build completed successfully"

cd ..

# Step 5.5: Use already built web UI from public/ or build from assets/ if needed
info "Step 5.5/7: Checking web UI..."
if [ -d "public" ] && ([ -f "public/index.html" ] || [ -f "public/index.html.gz" ]); then
    success "Using already built web UI from public/"
elif [ -d "assets" ]; then
    info "Web UI not found in public/, building from assets/..."
    cd assets
    if [ ! -d "node_modules" ]; then
        if ! command -v npm >/dev/null 2>&1; then
            warning "npm not found. Installing Node.js via Homebrew..."
            brew install node || error_exit "Failed to install Node.js"
        fi
        info "Installing web UI dependencies..."
        npm install || error_exit "Failed to install web UI dependencies"
    fi
    info "Building web UI..."
    npm run build || error_exit "Failed to build web UI"
    cd ..
    success "Web UI built successfully"
else
    warning "Neither public/ nor assets/ directory found. Web UI will not be available."
fi

# Step 6: Install system-wide
info "Step 6/7: Installing system-wide..."
if [ "$EUID" -ne 0 ]; then
    warning "System-wide installation requires sudo privileges"
    info "Installing to $INSTALL_PREFIX..."
    sudo cmake --install "$BUILD_DIR" --prefix "$INSTALL_PREFIX" || error_exit "Installation failed"
else
    cmake --install "$BUILD_DIR" --prefix "$INSTALL_PREFIX" || error_exit "Installation failed"
fi

# Install web UI if built
if [ -d "public" ] && ([ -f "public/index.html" ] || [ -f "public/index.html.gz" ]); then
    info "Installing web UI..."
    sudo mkdir -p "$INSTALL_PREFIX/share/delta-cli/webui" || mkdir -p "$INSTALL_PREFIX/share/delta-cli/webui"
    sudo cp -r public/* "$INSTALL_PREFIX/share/delta-cli/webui/" 2>/dev/null || \
    cp -r public/* "$INSTALL_PREFIX/share/delta-cli/webui/" 2>/dev/null
    success "Web UI installed to $INSTALL_PREFIX/share/delta-cli/webui"
else
    warning "Web UI not found. Server will use embedded UI or find files at runtime."
fi

# Step 7: Final verification
info "Step 7/7: Final verification..."
# Verify installation
if [ -f "$INSTALL_PREFIX/bin/delta" ]; then
    success "Delta CLI installed to $INSTALL_PREFIX/bin/delta"
else
    error_exit "Installation verification failed"
fi

# Check if delta is in PATH
if command -v delta >/dev/null 2>&1; then
    success "Delta CLI is available in PATH"
    DELTA_VERSION=$(delta --version 2>&1 | head -n1 || echo "unknown")
    info "Installed version: $DELTA_VERSION"
else
    warning "Delta CLI may not be in your PATH"
    info "Add to PATH: export PATH=\"$INSTALL_PREFIX/bin:\$PATH\""
    info "Or restart your terminal"
fi

echo ""
echo "╔══════════════════════════════════════════════════════════════╗"
echo "║              Installation Complete!                         ║"
echo "╚══════════════════════════════════════════════════════════════╝"
echo ""
success "Delta CLI has been successfully installed!"
echo ""
echo "Next steps:"
echo "  1. Test installation: delta --version"
echo "  2. Download a model: delta pull qwen3:0.6b"
echo "  3. Start chatting: delta"
echo ""
echo "If 'delta' command is not found, restart your terminal or run:"
echo "  export PATH=\"$INSTALL_PREFIX/bin:\$PATH\""
echo ""
