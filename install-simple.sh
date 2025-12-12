#!/bin/bash
# Simple one-command installer for Delta CLI
# Works on macOS, Linux, and Windows (via WSL)

set -e

echo "╔══════════════════════════════════════════════════════════════╗"
echo "║        Delta CLI - Simple One-Command Installer              ║"
echo "╚══════════════════════════════════════════════════════════════╝"
echo ""

# Detect OS
OS="$(uname -s)"
ARCH="$(uname -m)"

case "$OS" in
    Darwin)
        echo "🍎 Detected: macOS"
        echo ""
        echo "Installing via Homebrew..."
        if ! command -v brew &> /dev/null; then
            echo "📦 Installing Homebrew first..."
            /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
        fi
        echo "📦 Installing Delta CLI..."
        brew tap nile-agi/delta
        brew install delta
        echo ""
        echo "✅ Installation complete!"
        echo ""
        echo "Run: source ~/.zshrc (or restart terminal)"
        echo "Then: delta --version"
        ;;
    Linux)
        echo "🐧 Detected: Linux"
        echo ""
        # Detect distribution
        if [ -f /etc/debian_version ]; then
            echo "📦 Detected: Debian/Ubuntu"
            echo "Installing via apt..."
            curl -fsSL https://raw.githubusercontent.com/nile-agi/delta/main/packaging/linux/install-deb.sh | sudo bash
        elif [ -f /etc/redhat-release ]; then
            echo "📦 Detected: RHEL/CentOS/Fedora"
            echo "Installing via yum/dnf..."
            curl -fsSL https://raw.githubusercontent.com/nile-agi/delta/main/packaging/linux/install-rpm.sh | sudo bash
        else
            echo "❌ Unsupported Linux distribution"
            echo "Please use manual installation:"
            echo "  https://github.com/nile-agi/delta/blob/main/INSTALL.md"
            exit 1
        fi
        ;;
    MINGW*|MSYS*|CYGWIN*)
        echo "🪟 Detected: Windows (Git Bash/Cygwin)"
        echo ""
        echo "Please use PowerShell for Windows installation:"
        echo "  Set-ExecutionPolicy Bypass -Scope Process -Force;"
        echo "  Invoke-WebRequest -Uri 'https://raw.githubusercontent.com/nile-agi/delta/main/packaging/windows/install.ps1' -OutFile install.ps1;"
        echo "  .\install.ps1"
        exit 1
        ;;
    *)
        echo "❌ Unsupported operating system: $OS"
        exit 1
        ;;
esac

