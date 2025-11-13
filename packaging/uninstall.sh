#!/bin/bash
# Universal uninstall script for Delta CLI
# Works on macOS and Linux

set -e

echo "╔══════════════════════════════════════════════════════════════╗"
echo "║              Delta CLI Uninstaller                          ║"
echo "╚══════════════════════════════════════════════════════════════╝"
echo ""

# Detect OS
OS="$(uname -s)"

# Check if running as root (for Linux)
if [ "$OS" != "Darwin" ] && [ "$EUID" -ne 0 ]; then 
    echo "❌ This script requires root privileges (use sudo)"
    exit 1
fi

echo "🔍 Detecting installation method..."

# Check for Homebrew installation
if command -v brew &> /dev/null; then
    if brew list delta &> /dev/null 2>&1; then
        echo "📦 Found Homebrew installation"
        echo ""
        read -p "Uninstall via Homebrew? (y/n) " -n 1 -r
        echo
        if [[ $REPLY =~ ^[Yy]$ ]]; then
            brew uninstall delta
            brew untap nile-agi/delta 2>/dev/null || true
            echo "✅ Uninstalled via Homebrew"
            exit 0
        fi
    fi
fi

# Manual uninstall
echo "🗑️  Removing Delta CLI files..."

# Remove binaries
BIN_PATHS=(
    "/opt/homebrew/bin/delta"
    "/opt/homebrew/bin/delta-server"
    "/usr/local/bin/delta"
    "/usr/local/bin/delta-server"
)

for path in "${BIN_PATHS[@]}"; do
    if [ -f "$path" ]; then
        echo "  Removing: $path"
        rm -f "$path"
    fi
done

# Remove web UI
WEBUI_PATHS=(
    "/opt/homebrew/share/delta-cli"
    "/usr/local/share/delta-cli"
)

for path in "${WEBUI_PATHS[@]}"; do
    if [ -d "$path" ]; then
        echo "  Removing: $path"
        rm -rf "$path"
    fi
done

# Remove configuration files
CONFIG_PATHS=(
    "$HOME/.delta"
    "$HOME/.config/delta-cli"
)

for path in "${CONFIG_PATHS[@]}"; do
    if [ -d "$path" ]; then
        echo "  Removing: $path"
        rm -rf "$path"
    fi
done

# Remove PATH configuration (Linux)
if [ "$OS" != "Darwin" ]; then
    if [ -f "/etc/profile.d/delta-cli.sh" ]; then
        echo "  Removing: /etc/profile.d/delta-cli.sh"
        rm -f /etc/profile.d/delta-cli.sh
    fi
fi

# Clean shell config (user needs to do manually)
echo ""
echo "⚠️  Manual cleanup needed:"
echo "   Edit your shell config file (~/.zshrc or ~/.bash_profile)"
echo "   and remove lines containing 'Delta CLI' or 'delta' alias"
echo ""

echo "✅ Delta CLI uninstalled!"
echo ""
echo "💡 To completely remove, also clean your shell config:"
if [ -f "$HOME/.zshrc" ]; then
    echo "   nano ~/.zshrc  # Remove Delta CLI related lines"
elif [ -f "$HOME/.bash_profile" ]; then
    echo "   nano ~/.bash_profile  # Remove Delta CLI related lines"
fi

