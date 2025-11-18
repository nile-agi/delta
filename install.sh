#!/bin/bash
# Delta CLI Installation Script
# For end users to install Delta CLI easily

set -e

VERSION="${1:-latest}"
REPO_URL="https://github.com/nile-agi/delta"

# Determine best install directory
if [ -n "$2" ]; then
    INSTALL_DIR="$2"
elif [ "$(uname -s)" = "Darwin" ] && [ -d "/opt/homebrew/bin" ]; then
    # On macOS with Homebrew, use /opt/homebrew/bin (comes before /usr/local/bin in PATH)
    INSTALL_DIR="/opt/homebrew/bin"
elif [ -d "/usr/local/bin" ]; then
    INSTALL_DIR="/usr/local/bin"
else
    INSTALL_DIR="${HOME}/.local/bin"
fi

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

echo "⬇️  Downloading Delta CLI from: $DOWNLOAD_URL"
TEMP_DIR=$(mktemp -d)
cd "$TEMP_DIR"

# Download with error checking
HTTP_CODE=0

if command -v curl &> /dev/null; then
    # Download and capture HTTP code separately
    # -w writes to stdout, -o writes to file, -s suppresses progress
    HTTP_CODE=$(curl -L -w "%{http_code}" -o delta-cli.tar.gz -s "$DOWNLOAD_URL" | tail -1)
elif command -v wget &> /dev/null; then
    # Use wget and capture HTTP code
    HTTP_CODE=$(wget -O delta-cli.tar.gz --server-response "$DOWNLOAD_URL" 2>&1 | awk '/HTTP\// {print $2}' | tail -1)
else
    echo "❌ Error: curl or wget is required"
    exit 1
fi

# Check if download was successful
if [ "$HTTP_CODE" != "200" ]; then
    echo ""
    echo "❌ Error: Failed to download Delta CLI (HTTP $HTTP_CODE)"
    echo ""
    echo "📋 This usually means:"
    echo "   • No pre-built release is available for your platform yet"
    echo "   • The release URL has changed"
    echo ""
    echo "💡 Alternative Installation Methods:"
    echo ""
    if [ "$OS" = "Darwin" ]; then
        echo "🍎 Option 1: Install via Homebrew (builds from source)"
        echo "   First, install Xcode Command Line Tools:"
        echo "   xcode-select --install"
        echo ""
        echo "   Then install Delta CLI:"
        echo "   brew tap nile-agi/delta-cli && brew install --HEAD nile-agi/delta-cli/delta-cli"
        echo ""
        echo "🍎 Option 2: Build from source manually"
        echo "   See: $REPO_URL/blob/main/README.md#building-from-source"
    else
        echo "🐧 Option 1: Build from source manually"
        echo "   See: $REPO_URL/blob/main/README.md#building-from-source"
        echo ""
        echo "🐧 Option 2: Check for other installation methods"
        echo "   See: $REPO_URL/blob/main/packaging/INSTALL_SIMPLE.md"
    fi
    echo ""
    echo "📚 For more help, visit: $REPO_URL/issues"
    echo ""
    cd - > /dev/null
    rm -rf "$TEMP_DIR"
    exit 1
fi

# Verify the downloaded file exists and has content
if [ ! -f "delta-cli.tar.gz" ]; then
    echo "❌ Error: Download failed - file not created"
    cd - > /dev/null
    rm -rf "$TEMP_DIR"
    exit 1
fi

# Check file size (should be more than a few bytes for a real archive)
FILE_SIZE=$(stat -f%z delta-cli.tar.gz 2>/dev/null || stat -c%s delta-cli.tar.gz 2>/dev/null || echo "0")
if [ "$FILE_SIZE" -lt 100 ]; then
    echo "❌ Error: Download failed - file is too small (likely an error page)"
    echo "   File size: $FILE_SIZE bytes"
    cd - > /dev/null
    rm -rf "$TEMP_DIR"
    exit 1
fi

# Check if it's a valid tar.gz file (not an HTML error page)
# First check if it's an HTML error page (most common case)
if head -1 delta-cli.tar.gz 2>/dev/null | grep -q "<!DOCTYPE html\|<html\|404\|Not Found"; then
        echo ""
        echo "❌ Error: Release not found (received HTML error page)"
        echo ""
        echo "📋 This means no pre-built release is available yet."
        echo ""
        echo "💡 Alternative Installation Methods:"
        echo ""
        if [ "$OS" = "Darwin" ]; then
            echo "🍎 Option 1: Install via Homebrew (builds from source)"
            echo "   First, install Xcode Command Line Tools:"
            echo "   xcode-select --install"
            echo ""
            echo "   Then install Delta CLI:"
            echo "   brew tap nile-agi/delta-cli && brew install --HEAD nile-agi/delta-cli/delta-cli"
        else
            echo "🐧 Build from source:"
            echo "   See: $REPO_URL/blob/main/README.md#building-from-source"
        fi
        echo ""
        cd - > /dev/null
        rm -rf "$TEMP_DIR"
        exit 1
fi

# Additional check: verify it's actually a gzip file
if command -v file &> /dev/null; then
    if ! file delta-cli.tar.gz 2>/dev/null | grep -q "gzip\|tar\|archive\|compressed"; then
        echo "⚠️  Warning: Downloaded file may not be a valid tar.gz archive"
        echo "   Proceeding anyway, but extraction may fail..."
    fi
fi

echo "📦 Extracting..."
if ! tar -xzf delta-cli.tar.gz 2>/dev/null; then
    echo "❌ Error: Failed to extract archive. The downloaded file may be corrupted."
    echo "   Try downloading again or use an alternative installation method."
    cd - > /dev/null
    rm -rf "$TEMP_DIR"
    exit 1
fi

echo "📋 Installing binaries..."
# Find the extracted directory
EXTRACTED_DIR=$(find . -maxdepth 1 -type d -name "delta-cli-*" | head -1)

if [ -z "$EXTRACTED_DIR" ] || [ ! -d "$EXTRACTED_DIR" ]; then
    echo "❌ Error: Could not find extracted delta-cli directory"
    echo "   The archive structure may be different than expected."
    cd - > /dev/null
    rm -rf "$TEMP_DIR"
    exit 1
fi

if [ ! -f "$EXTRACTED_DIR/delta" ] || [ ! -f "$EXTRACTED_DIR/delta-server" ]; then
    echo "❌ Error: Required binaries (delta, delta-server) not found in archive"
    cd - > /dev/null
    rm -rf "$TEMP_DIR"
    exit 1
fi

$SUDO cp "$EXTRACTED_DIR/delta" "$INSTALL_DIR/delta"
$SUDO cp "$EXTRACTED_DIR/delta-server" "$INSTALL_DIR/delta-server"
$SUDO chmod +x "$INSTALL_DIR/delta" "$INSTALL_DIR/delta-server"

# Install web UI if present
if [ -d "$EXTRACTED_DIR/webui" ]; then
    WEBUI_DIR="/usr/local/share/delta-cli/webui"
    echo "📋 Installing web UI..."
    $SUDO mkdir -p "$WEBUI_DIR"
    $SUDO cp -r "$EXTRACTED_DIR/webui"/* "$WEBUI_DIR/"
fi

# Cleanup
cd -
rm -rf "$TEMP_DIR"

echo ""
echo "✅ Delta CLI installed successfully!"
echo ""

# Check for PATH conflicts
if command -v delta &> /dev/null; then
    DELTA_PATH=$(which delta)
    if [ "$DELTA_PATH" != "$INSTALL_DIR/delta" ]; then
        echo "⚠️  Warning: Another 'delta' command found at: $DELTA_PATH"
        echo "   Our delta is installed at: $INSTALL_DIR/delta"
        echo "   To use our delta, run: $INSTALL_DIR/delta"
        echo "   Or add $INSTALL_DIR to the front of your PATH"
        echo ""
    fi
fi
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

