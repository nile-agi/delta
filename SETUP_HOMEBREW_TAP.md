# Setting Up Homebrew Tap for Delta CLI

This guide will help you create a Homebrew tap so users can install delta-cli with:
```bash
brew install nile-agi/delta-cli/delta-cli
```

## Step 1: Create GitHub Repository

1. Go to GitHub and create a new repository
2. **Repository name MUST be:** `homebrew-delta-cli`
3. **Description:** `Homebrew tap for Delta CLI - Offline AI Assistant`
4. Make it **public**
5. **DO NOT** initialize with README, .gitignore, or license (we'll add our own)

## Step 2: Clone and Set Up the Tap

```bash
# Clone the new repository
git clone https://github.com/nile-agi/homebrew-delta-cli.git
cd homebrew-delta-cli

# Create Formula directory
mkdir -p Formula

# Copy the formula file
cp /path/to/delta/packaging/homebrew/delta-cli.rb Formula/delta-cli.rb
```

## Step 3: Update the Formula

Before committing, you need to:

1. **For a release version:**
   - Update `url` with the actual GitHub release tarball URL
   - Update `sha256` with the actual SHA256 hash of the tarball
   - Remove or comment out the `head` line (or keep it for development)

2. **For development/head version:**
   - Keep the `head` line as is
   - The `url` and `sha256` can remain placeholders for now

Example for release v1.0.0:
```ruby
url "https://github.com/nile-agi/delta/archive/refs/tags/v1.0.0.tar.gz"
sha256 "ACTUAL_SHA256_HASH_HERE"  # Get this from: shasum -a 256 delta-1.0.0.tar.gz
```

## Step 4: Create README for the Tap

Create a `README.md` in the tap repository:

```markdown
# homebrew-delta-cli

Homebrew tap for [Delta CLI](https://github.com/nile-agi/delta) - Offline AI Assistant powered by llama.cpp

## Installation

```bash
brew install nile-agi/delta-cli/delta-cli
```

## What is Delta CLI?

Delta CLI is an open-source, offline-first AI assistant that runs large language models (LLMs) directly on your device.

## Documentation

- [Main Repository](https://github.com/nile-agi/delta)
- [Installation Guide](https://github.com/nile-agi/delta/blob/main/INSTALL.md)
- [Quick Start](https://github.com/nile-agi/delta/blob/main/QUICK_START.md)

## License

MIT
```

## Step 5: Commit and Push

```bash
# Add files
git add Formula/delta-cli.rb README.md

# Commit
git commit -m "Add delta-cli formula"

# Push
git push origin main
```

## Step 6: Test Installation

```bash
# Tap the repository
brew tap nile-agi/delta-cli

# Install
brew install delta-cli

# Verify
delta --version
```

## Step 7: Update Formula for New Releases

When releasing a new version:

1. **Get the release tarball SHA256:**
   ```bash
   # Download the release tarball
   curl -L -o delta-1.0.0.tar.gz https://github.com/nile-agi/delta/archive/refs/tags/v1.0.0.tar.gz
   
   # Calculate SHA256
   shasum -a 256 delta-1.0.0.tar.gz
   ```

2. **Update Formula/delta-cli.rb:**
   - Update `version` (e.g., `"1.0.0"`)
   - Update `url` with new release URL
   - Update `sha256` with calculated hash

3. **Commit and push:**
   ```bash
   git add Formula/delta-cli.rb
   git commit -m "delta-cli 1.0.0"
   git push origin main
   ```

## Formula File Location

The formula file should be at:
```
homebrew-delta-cli/
  └── Formula/
      └── delta-cli.rb
```

## Important Notes

1. **Repository naming:** The repository MUST be named `homebrew-delta-cli` (exactly)
2. **Formula naming:** The formula file MUST be named `delta-cli.rb` (matches the class name)
3. **Directory structure:** Formula files MUST be in the `Formula/` directory
4. **SHA256:** Always update SHA256 when releasing new versions
5. **Submodules:** The formula already includes `submodules: true` in the head version

## Troubleshooting

### "No available formula"
- Make sure the repository is named exactly `homebrew-delta-cli`
- Make sure the formula file is in `Formula/delta-cli.rb`
- Try: `brew tap nile-agi/delta-cli` first, then `brew install delta-cli`

### "SHA256 mismatch"
- Recalculate the SHA256 hash of the release tarball
- Update the formula with the correct hash

### Build fails
- Check that all dependencies are installed: `brew install cmake curl pkg-config`
- Check that submodules are being fetched correctly
- Review build logs: `brew install --verbose delta-cli`

## Quick Setup Script

Here's a quick script to set up the tap:

```bash
#!/bin/bash
set -e

REPO_NAME="homebrew-delta-cli"
GITHUB_USER="nile-agi"
DELTA_REPO_PATH="/path/to/delta"  # Update this path

echo "Setting up Homebrew tap for Delta CLI..."

# Clone tap repository (if it exists)
if [ ! -d "$REPO_NAME" ]; then
    echo "Repository doesn't exist locally. Please create it on GitHub first:"
    echo "  https://github.com/new"
    echo "  Name: $REPO_NAME"
    echo "  Public repository"
    echo ""
    read -p "Press Enter after creating the repository..."
    git clone "https://github.com/$GITHUB_USER/$REPO_NAME.git"
fi

cd "$REPO_NAME"

# Create Formula directory
mkdir -p Formula

# Copy formula
cp "$DELTA_REPO_PATH/packaging/homebrew/delta-cli.rb" Formula/delta-cli.rb

# Create README if it doesn't exist
if [ ! -f README.md ]; then
    cat > README.md << 'EOF'
# homebrew-delta-cli

Homebrew tap for Delta CLI - Offline AI Assistant

## Installation

\`\`\`bash
brew install nile-agi/delta-cli/delta-cli
\`\`\`

## Documentation

- [Main Repository](https://github.com/nile-agi/delta)
EOF
fi

echo "✅ Tap setup complete!"
echo ""
echo "Next steps:"
echo "1. Review Formula/delta-cli.rb"
echo "2. Update SHA256 if using a release version"
echo "3. git add Formula/delta-cli.rb README.md"
echo "4. git commit -m 'Add delta-cli formula'"
echo "5. git push origin main"
echo ""
echo "Then users can install with:"
echo "  brew install nile-agi/delta-cli/delta-cli"
```

Save this as `setup-homebrew-tap.sh`, make it executable, and run it.

