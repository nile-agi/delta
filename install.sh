#!/bin/bash
# Delta CLI Installation Script
# For end users to install Delta CLI easily

set -e

VERSION="${1:-latest}"
INSTALL_DIR="${2:-/usr/local/bin}"
REPO_URL="https://github.com/nile-agi/delta"

echo "╔══════════════════════════════════════════════════════════════╗"
echo "║           Delta CLI Installation Script                      ║"
echo "╚══════════════════════════════════════════════════════════════╝"
echo ""

# Detect platform
OS="$(uname -s)"
ARCH="$(uname -m)"

case "$ARCH" in
    arm64|aarch64)
        if [ "$OS" = "Darwin" ]; then
            PLATFORM="macos-arm64"
        else
            PLATFORM="linux-aarch64"
        fi
        ;;
    x86_64|amd64)
        if [ "$OS" = "Darwin" ]; then
            PLATFORM="macos-x86_64"
        else
            PLATFORM="linux-x86_64"
        fi
        ;;
    *)
        echo "❌ Unsupported architecture: $ARCH"
        exit 1
        ;;
esac

echo "📦 Detected platform: $PLATFORM"
echo "📁 Installation directory: $INSTALL_DIR"
echo ""

# Check if install directory exists and is writable
if [ ! -d "$INSTALL_DIR" ]; then
    echo "📁 Creating installation directory: $INSTALL_DIR"
    sudo mkdir -p "$INSTALL_DIR"
fi

if [ ! -w "$INSTALL_DIR" ]; then
    echo "🔐 Installation requires sudo privileges"
    SUDO="sudo"
else
    SUDO=""
fi

# Download URL
if [ "$VERSION" = "latest" ]; then
    DOWNLOAD_URL="$REPO_URL/releases/latest/download/delta-cli-$PLATFORM.tar.gz"
else
    DOWNLOAD_URL="$REPO_URL/releases/download/v$VERSION/delta-cli-$PLATFORM.tar.gz"
fi

echo "⬇️  Downloading Delta CLI..."
TEMP_DIR=$(mktemp -d)
cd "$TEMP_DIR"

if command -v curl &> /dev/null; then
    curl -L -o delta-cli.tar.gz "$DOWNLOAD_URL"
elif command -v wget &> /dev/null; then
    wget -O delta-cli.tar.gz "$DOWNLOAD_URL"
else
    echo "❌ Error: curl or wget is required"
    exit 1
fi

echo "📦 Extracting..."
tar -xzf delta-cli.tar.gz

echo "📋 Installing binaries..."
$SUDO cp delta-cli-*/delta "$INSTALL_DIR/delta"
$SUDO cp delta-cli-*/delta-server "$INSTALL_DIR/delta-server"
$SUDO chmod +x "$INSTALL_DIR/delta" "$INSTALL_DIR/delta-server"

# Install web UI if present
if [ -d "delta-cli-*/webui" ]; then
    WEBUI_DIR="/usr/local/share/delta-cli/webui"
    echo "📋 Installing web UI..."
    $SUDO mkdir -p "$WEBUI_DIR"
    $SUDO cp -r delta-cli-*/webui/* "$WEBUI_DIR/"
fi

# Cleanup
cd -
rm -rf "$TEMP_DIR"

echo ""
echo "✅ Delta CLI installed successfully!"
echo ""
echo "📝 Quick Start:"
echo "   delta --version           # Check version"
echo "   delta pull qwen2.5:0.5b   # Download a model"
echo "   delta server              # Start web server"
echo "   delta                     # Start interactive mode"
echo ""
echo "📚 Documentation:"
echo "   Visit: $REPO_URL"
echo "   Quick Start: $REPO_URL/blob/main/QUICK_START.md"
echo ""
echo "💡 Tip: No git or GitHub knowledge needed - just use delta!"
echo ""

