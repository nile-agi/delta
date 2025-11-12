# Installing Delta CLI via Homebrew

## Current Status

Delta CLI is not yet available in Homebrew core. Here are your installation options:

## Option 1: Install from Local Formula (Recommended for Now)

You can install directly from the formula file in this repository:

```bash
# Clone the repository
git clone --recurse-submodules https://github.com/nile-agi/delta.git
cd delta

# Install from local formula
brew install --build-from-source packaging/homebrew/delta-cli.rb
```

This will:
- Build delta-cli from source
- Automatically update the llama.cpp submodule
- Install `delta` and `delta-server` binaries

## Option 2: Use Installation Script (Easiest)

```bash
curl -fsSL https://raw.githubusercontent.com/nile-agi/delta/main/install.sh | bash
```

This downloads and installs pre-built binaries.

## Option 3: Build from Source

```bash
# Clone with submodules
git clone --recurse-submodules https://github.com/nile-agi/delta.git
cd delta

# Build
./packaging/build-scripts/build-macos.sh Release arm64

# Install manually
sudo cp build_macos_arm64/delta /usr/local/bin/delta
sudo cp build_macos_arm64/delta-server /usr/local/bin/delta-server
sudo chmod +x /usr/local/bin/delta /usr/local/bin/delta-server
```

## Option 4: Create Homebrew Tap (For Future)

To make `brew install delta-cli` work, you need to:

1. **Create a Homebrew tap repository:**
   - Create a new GitHub repository: `homebrew-delta-cli`
   - It must be named exactly `homebrew-delta-cli`

2. **Add the formula:**
   ```bash
   git clone https://github.com/nile-agi/homebrew-delta-cli.git
   cd homebrew-delta-cli
   mkdir -p Formula
   cp /path/to/delta/packaging/homebrew/delta-cli.rb Formula/delta-cli.rb
   # Update sha256 in the formula with actual release value
   git add Formula/delta-cli.rb
   git commit -m "Add delta-cli formula"
   git push origin main
   ```

3. **Install from tap:**
   ```bash
   brew install nile-agi/delta-cli/delta-cli
   ```

## Verification

After installation, verify it works:

```bash
delta --version
```

Expected output: `Delta CLI v1.0.0`

## Next Steps

1. Download a model: `delta pull qwen2.5:0.5b`
2. Start the server: `delta server`
3. Or use interactive mode: `delta`

