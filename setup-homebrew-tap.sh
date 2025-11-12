#!/bin/bash
# Quick setup script for Homebrew tap

set -e

REPO_NAME="homebrew-delta-cli"
GITHUB_USER="nile-agi"
DELTA_REPO_PATH="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

echo "╔══════════════════════════════════════════════════════════════╗"
echo "║     Setting up Homebrew Tap for Delta CLI                    ║"
echo "╚══════════════════════════════════════════════════════════════╝"
echo ""

# Check if repository exists locally
if [ ! -d "$REPO_NAME" ]; then
    echo "📦 Repository doesn't exist locally."
    echo ""
    echo "Please create it on GitHub first:"
    echo "  1. Go to: https://github.com/new"
    echo "  2. Repository name: $REPO_NAME"
    echo "  3. Description: Homebrew tap for Delta CLI"
    echo "  4. Make it PUBLIC"
    echo "  5. DO NOT initialize with README, .gitignore, or license"
    echo ""
    read -p "Press Enter after creating the repository on GitHub..."
    
    if [ ! -d "$REPO_NAME" ]; then
        echo "Cloning repository..."
        git clone "https://github.com/$GITHUB_USER/$REPO_NAME.git"
    fi
fi

cd "$REPO_NAME"

# Create Formula directory
echo "📁 Creating Formula directory..."
mkdir -p Formula

# Copy formula
echo "📋 Copying formula file..."
cp "$DELTA_REPO_PATH/packaging/homebrew/delta-cli.rb" Formula/delta-cli.rb

# Create README if it doesn't exist
if [ ! -f README.md ]; then
    echo "📝 Creating README.md..."
    cat > README.md << 'EOF'
# homebrew-delta-cli

Homebrew tap for [Delta CLI](https://github.com/nile-agi/delta) - Offline AI Assistant powered by llama.cpp

## Installation

```bash
brew install nile-agi/delta-cli/delta-cli
```

## What is Delta CLI?

Delta CLI is an open-source, offline-first AI assistant that runs large language models (LLMs) directly on your device. Built on top of llama.cpp, it provides a simple command-line interface to interact with AI models without requiring internet connectivity.

## Features

- 🔒 100% Offline
- ⚡ High Performance (GPU acceleration)
- 🌐 Cross-Platform
- 🎨 Beautiful Terminal UI
- 📦 Easy Model Management

## Documentation

- [Main Repository](https://github.com/nile-agi/delta)
- [Installation Guide](https://github.com/nile-agi/delta/blob/main/INSTALL.md)
- [Quick Start](https://github.com/nile-agi/delta/blob/main/QUICK_START.md)

## License

MIT
EOF
fi

echo ""
echo "✅ Tap setup complete!"
echo ""
echo "📋 Next steps:"
echo ""
echo "1. Review the formula:"
echo "   cat Formula/delta-cli.rb"
echo ""
echo "2. If using a release version, update SHA256:"
echo "   # Get SHA256 from release tarball"
echo "   shasum -a 256 /path/to/delta-1.0.0.tar.gz"
echo ""
echo "3. Commit and push:"
echo "   git add Formula/delta-cli.rb README.md"
echo "   git commit -m 'Add delta-cli formula'"
echo "   git push origin main"
echo ""
echo "4. Test installation:"
echo "   brew tap nile-agi/delta-cli"
echo "   brew install delta-cli"
echo ""
echo "5. Verify:"
echo "   delta --version"
echo ""

