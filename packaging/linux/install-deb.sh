#!/bin/bash
# Debian/Ubuntu installation script for Delta CLI
# Handles dependencies, installation, and PATH configuration

set -e

echo "╔══════════════════════════════════════════════════════════════╗"
echo "║           Delta CLI Installation (Debian/Ubuntu)            ║"
echo "╚══════════════════════════════════════════════════════════════╝"
echo ""

# Check if running as root
if [ "$EUID" -ne 0 ]; then 
    echo "❌ This script requires root privileges (use sudo)"
    exit 1
fi

# Detect architecture
ARCH=$(dpkg --print-architecture)
case "$ARCH" in
    amd64) PLATFORM="linux-x86_64" ;;
    arm64) PLATFORM="linux-aarch64" ;;
    *)
        echo "❌ Unsupported architecture: $ARCH"
        exit 1
        ;;
esac

echo "📦 Detected platform: $PLATFORM"
echo ""

# Install dependencies
echo "📋 Installing dependencies..."
apt-get update
apt-get install -y \
    curl \
    wget \
    cmake \
    build-essential \
    git \
    pkg-config \
    libcurl4-openssl-dev

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
# /usr/local/bin should already be in PATH, but ensure it's prioritized
if ! grep -q "/usr/local/bin" /etc/environment 2>/dev/null; then
    echo "PATH=\"/usr/local/bin:/usr/bin:/bin:/usr/local/sbin:/usr/sbin:/sbin\"" >> /etc/environment
fi

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

