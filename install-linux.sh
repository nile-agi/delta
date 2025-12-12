#!/bin/bash
# Delta CLI - Complete Linux Installation Script
# Supports: Ubuntu/Debian, Fedora/RHEL, Arch, Alpine, and other distributions

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
echo "║         Delta CLI - Linux Complete Installation              ║"
echo "╚══════════════════════════════════════════════════════════════╝"
echo ""

# Check if running on Linux
if [[ "$OSTYPE" != "linux-gnu"* ]]; then
    error_exit "This script is for Linux only. Use install-macos.sh for macOS."
fi

# Detect architecture
ARCH=$(uname -m)
info "Detected architecture: $ARCH"

# Configuration
BUILD_TYPE=${BUILD_TYPE:-Release}
INSTALL_PREFIX=${INSTALL_PREFIX:-/usr/local}
BUILD_DIR="build_linux"

# Detect package manager
detect_package_manager() {
    if command -v apt-get >/dev/null 2>&1; then
        PKG_MANAGER="apt"
        PKG_INSTALL="apt-get install -y"
        PKG_UPDATE="apt-get update"
        PKG_BUILD_ESSENTIAL="build-essential"
        PKG_CMAKE="cmake"
        PKG_GIT="git"
        PKG_CURL="libcurl4-openssl-dev"
    elif command -v yum >/dev/null 2>&1; then
        PKG_MANAGER="yum"
        PKG_INSTALL="yum install -y"
        PKG_UPDATE="yum check-update || true"
        PKG_BUILD_ESSENTIAL="gcc-c++ make"
        PKG_CMAKE="cmake"
        PKG_GIT="git"
        PKG_CURL="libcurl-devel"
    elif command -v dnf >/dev/null 2>&1; then
        PKG_MANAGER="dnf"
        PKG_INSTALL="dnf install -y"
        PKG_UPDATE="dnf check-update || true"
        PKG_BUILD_ESSENTIAL="gcc-c++ make"
        PKG_CMAKE="cmake"
        PKG_GIT="git"
        PKG_CURL="libcurl-devel"
    elif command -v pacman >/dev/null 2>&1; then
        PKG_MANAGER="pacman"
        PKG_INSTALL="pacman -S --noconfirm"
        PKG_UPDATE="pacman -Sy"
        PKG_BUILD_ESSENTIAL="base-devel"
        PKG_CMAKE="cmake"
        PKG_GIT="git"
        PKG_CURL="curl"
    elif command -v apk >/dev/null 2>&1; then
        PKG_MANAGER="apk"
        PKG_INSTALL="apk add --no-cache"
        PKG_UPDATE="apk update"
        PKG_BUILD_ESSENTIAL="build-base"
        PKG_CMAKE="cmake"
        PKG_GIT="git"
        PKG_CURL="curl-dev"
    elif command -v zypper >/dev/null 2>&1; then
        PKG_MANAGER="zypper"
        PKG_INSTALL="zypper install -y"
        PKG_UPDATE="zypper refresh"
        PKG_BUILD_ESSENTIAL="gcc-c++ make"
        PKG_CMAKE="cmake"
        PKG_GIT="git"
        PKG_CURL="libcurl-devel"
    else
        error_exit "Unsupported package manager. Please install dependencies manually."
    fi
    info "Detected package manager: $PKG_MANAGER"
}

# Step 1: Detect package manager
info "Step 1/8: Detecting package manager..."
detect_package_manager

# Step 2: Check for sudo
info "Step 2/8: Checking permissions..."
if [ "$EUID" -ne 0 ]; then
    if ! command -v sudo >/dev/null 2>&1; then
        error_exit "This script requires root privileges or sudo. Please run with sudo or as root."
    fi
    SUDO_CMD="sudo"
    info "Will use sudo for package installation"
else
    SUDO_CMD=""
    info "Running as root"
fi

# Step 3: Update package lists
info "Step 3/8: Updating package lists..."
$SUDO_CMD $PKG_UPDATE || warning "Package update failed (continuing anyway)"

# Step 4: Install dependencies
info "Step 4/8: Installing dependencies..."
DEPENDENCIES="$PKG_BUILD_ESSENTIAL $PKG_CMAKE $PKG_GIT $PKG_CURL"

# Check what's missing
MISSING=""
for dep in $DEPENDENCIES; do
    # For build-essential, check for g++ instead
    if [ "$dep" = "build-essential" ] || [ "$dep" = "base-devel" ] || [ "$dep" = "build-base" ] || [ "$dep" = "gcc-c++ make" ]; then
        if ! command -v g++ >/dev/null 2>&1 && ! command -v clang++ >/dev/null 2>&1; then
            MISSING="$MISSING $dep"
        fi
    elif [ "$dep" = "libcurl4-openssl-dev" ] || [ "$dep" = "libcurl-devel" ] || [ "$dep" = "curl-dev" ]; then
        if ! command -v curl-config >/dev/null 2>&1; then
            MISSING="$MISSING $dep"
        fi
    else
        if ! command -v $(basename $dep) >/dev/null 2>&1; then
            MISSING="$MISSING $dep"
        fi
    fi
done

if [ -n "$MISSING" ]; then
    info "Installing missing dependencies:$MISSING"
    $SUDO_CMD $PKG_INSTALL $MISSING || error_exit "Failed to install dependencies"
    success "Dependencies installed"
else
    success "All dependencies already installed"
fi

# Verify cmake version
CMAKE_VERSION=$(cmake --version | head -n1 | cut -d' ' -f3)
info "CMake version: $CMAKE_VERSION"

# Step 5: Check for vendored llama.cpp
info "Step 5/8: Verifying project structure..."
if [ ! -f "vendor/llama.cpp/CMakeLists.txt" ]; then
    error_exit "llama.cpp not found in vendor/llama.cpp/. Please ensure you're in the delta-cli directory."
fi
success "Project structure verified"

# Step 6: Build
info "Step 6/8: Building Delta CLI..."
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# Check for optional GPU support
ENABLE_CUDA=OFF
ENABLE_VULKAN=OFF
USE_CURL=OFF

if command -v nvcc >/dev/null 2>&1; then
    ENABLE_CUDA=ON
    info "CUDA detected (GPU acceleration enabled)"
fi

if command -v vulkaninfo >/dev/null 2>&1; then
    ENABLE_VULKAN=ON
    info "Vulkan detected (GPU acceleration enabled)"
fi

if command -v curl-config >/dev/null 2>&1; then
    USE_CURL=ON
    info "libcurl found (telemetry enabled)"
fi

# Configure
info "Configuring build..."
cmake .. \
    -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
    -DCMAKE_INSTALL_PREFIX="$INSTALL_PREFIX" \
    -DGGML_CUDA="$ENABLE_CUDA" \
    -DGGML_VULKAN="$ENABLE_VULKAN" \
    -DUSE_CURL="$USE_CURL" \
    -DBUILD_TESTS=ON \
    -DBUILD_SERVER=ON \
    || error_exit "CMake configuration failed"

# Build
info "Compiling (this may take several minutes)..."
CPU_COUNT=$(nproc)
cmake --build . --config "$BUILD_TYPE" -j$CPU_COUNT || error_exit "Build failed"

# Verify binary exists
if [ ! -f "delta" ]; then
    error_exit "Build completed but delta binary not found"
fi
success "Build completed successfully"

cd ..

# Step 6.5: Use already built web UI from public/ or build from assets/ if needed
info "Step 6.5/8: Checking web UI..."
if [ -d "public" ] && ([ -f "public/index.html" ] || [ -f "public/index.html.gz" ]); then
    success "Using already built web UI from public/"
elif [ -d "assets" ]; then
    info "Web UI not found in public/, building from assets/..."
    cd assets
    if [ ! -d "node_modules" ]; then
        if ! command -v npm >/dev/null 2>&1; then
            warning "npm not found. Please install Node.js:"
            info "  Ubuntu/Debian: sudo apt-get install nodejs npm"
            info "  Fedora/RHEL: sudo dnf install nodejs npm"
            info "  Arch: sudo pacman -S nodejs npm"
            error_exit "Node.js and npm are required to build the web UI"
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

# Step 7: Install system-wide
info "Step 7/8: Installing system-wide..."
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

# Step 8: Final verification
info "Step 8/8: Final verification..."
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
    info "Or add to ~/.bashrc or ~/.zshrc:"
    info "  echo 'export PATH=\"$INSTALL_PREFIX/bin:\$PATH\"' >> ~/.bashrc"
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
