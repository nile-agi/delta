#!/bin/bash
# RHEL/CentOS/Fedora installation script for Delta CLI
# Handles dependencies, installation, and PATH configuration

set -e

echo "╔══════════════════════════════════════════════════════════════╗"
echo "║           Delta CLI Installation (RHEL/CentOS/Fedora)        ║"
echo "╚══════════════════════════════════════════════════════════════╝"
echo ""

# Check if running as root
if [ "$EUID" -ne 0 ]; then 
    echo "❌ This script requires root privileges (use sudo)"
    exit 1
fi

# Detect architecture
ARCH=$(uname -m)
case "$ARCH" in
    x86_64) PLATFORM="linux-x86_64" ;;
    aarch64) PLATFORM="linux-aarch64" ;;
    *)
        echo "❌ Unsupported architecture: $ARCH"
        exit 1
        ;;
esac

echo "📦 Detected platform: $PLATFORM"
echo ""

# Detect package manager
if command -v dnf &> /dev/null; then
    PKG_MGR="dnf"
elif command -v yum &> /dev/null; then
    PKG_MGR="yum"
else
    echo "❌ No supported package manager found (dnf/yum)"
    exit 1
fi

# Install minimal dependencies (only for downloading)
echo "📋 Installing minimal dependencies..."
$PKG_MGR install -y \
    curl \
    wget \
    tar

# Download and install Delta CLI
echo "⬇️  Downloading Delta CLI..."
TEMP_DIR=$(mktemp -d)
cd "$TEMP_DIR"

DOWNLOAD_URL="https://github.com/nile-agi/delta/releases/latest/download/delta-cli-${PLATFORM}.tar.gz"
curl -L -o delta-cli.tar.gz "$DOWNLOAD_URL"

echo "📦 Extracting..."
tar -xzf delta-cli.tar.gz

echo "📋 Installing binaries..."
INSTALL_DIR="/usr/local/bin"
cp delta-cli-*/delta "$INSTALL_DIR/delta"
cp delta-cli-*/delta-server "$INSTALL_DIR/delta-server"
chmod +x "$INSTALL_DIR/delta" "$INSTALL_DIR/delta-server"

# Install web UI if present
if [ -d "delta-cli-*/webui" ]; then
    WEBUI_DIR="/usr/local/share/delta-cli/webui"
    echo "📋 Installing web UI..."
    mkdir -p "$WEBUI_DIR"
    cp -r delta-cli-*/webui/* "$WEBUI_DIR/"
fi

# Cleanup
cd -
rm -rf "$TEMP_DIR"

# Configure PATH for all users
echo "🔧 Configuring PATH..."
# Create system-wide profile script
cat > /etc/profile.d/delta-cli.sh << 'EOF'
# Delta CLI PATH configuration
# Ensure /usr/local/bin comes first to avoid conflicts
export PATH="/usr/local/bin:/usr/local/sbin:$PATH"
EOF

chmod +x /etc/profile.d/delta-cli.sh

echo ""
echo "✅ Delta CLI installed successfully!"
echo ""
echo "📝 Quick Start:"
echo "   delta --version           # Check version"
echo "   delta pull qwen2.5:0.5b   # Download a model"
echo "   delta server              # Start web server"
echo ""
echo "💡 Note: You may need to restart your terminal or run:"
echo "   source /etc/profile.d/delta-cli.sh"

