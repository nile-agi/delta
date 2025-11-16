# Temporary Testing Guide

This guide shows you how to test Delta CLI changes without installing system-wide.

## Option 1: Run Directly from Build Directory (Recommended)

**No installation needed - just run from the build directory:**

```bash
# Navigate to your project
cd /Users/suzanodero/io/GITHUB/delta

# Build (if not already built)
cd build_macos
cmake .. -DCMAKE_BUILD_TYPE=Release -DGGML_METAL=ON -DBUILD_SERVER=ON
cmake --build . --config Release -j$(sysctl -n hw.ncpu)
cd ..

# Run directly
./build_macos/delta --version
./build_macos/delta

# Or start the server
./build_macos/delta-server --help
```

## Option 2: Create Temporary Shell Aliases

Add to your current shell session (temporary, lost when terminal closes):

```bash
# In your terminal
cd /Users/suzanodero/io/GITHUB/delta

# Create aliases
alias delta-test='$(pwd)/build_macos/delta'
alias delta-server-test='$(pwd)/build_macos/delta-server'

# Now use:
delta-test --version
delta-test
delta-server-test --help
```

## Option 3: Install to User Directory (No sudo)

Install to `~/.local/bin` which doesn't require sudo:

```bash
cd /Users/suzanodero/io/GITHUB/delta

# Build
cd build_macos
cmake .. \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=$HOME/.local \
    -DGGML_METAL=ON \
    -DBUILD_SERVER=ON
cmake --build . --config Release -j$(sysctl -n hw.ncpu)

# Install (no sudo needed)
cmake --install . --prefix $HOME/.local

# Add to PATH for this session
export PATH="$HOME/.local/bin:$PATH"

# Or add permanently to ~/.zshrc:
echo 'export PATH="$HOME/.local/bin:$PATH"' >> ~/.zshrc
source ~/.zshrc

# Now use:
delta --version
```

## Option 4: Create Temporary Symlink

Create a symlink in a directory already in your PATH:

```bash
# Create symlink (uses existing /usr/local/bin if in PATH)
ln -s $(pwd)/build_macos/delta /usr/local/bin/delta-test
ln -s $(pwd)/build_macos/delta-server /usr/local/bin/delta-server-test

# Use it
delta-test --version

# Remove when done testing
rm /usr/local/bin/delta-test
rm /usr/local/bin/delta-server-test
```

## Quick Test Script

Create a test script to make it easy:

```bash
# Create test-delta.sh
cat > test-delta.sh << 'SCRIPT'
#!/bin/bash
cd "$(dirname "$0")"
./build_macos/delta "$@"
SCRIPT

chmod +x test-delta.sh

# Use it
./test-delta.sh --version
./test-delta.sh
```

## Testing the Web UI

Make sure the web UI is built:

```bash
cd /Users/suzanodero/io/GITHUB/delta/assets
npm run build
```

Then start the server:

```bash
# From build directory
./build_macos/delta server

# Or if installed
delta-test server
```

## Clean Up After Testing

**If you used Option 1 (direct run):**
- No cleanup needed, just delete the build directory if desired

**If you used Option 2 (aliases):**
- Close terminal or run `unalias delta-test delta-server-test`

**If you used Option 3 (user install):**
```bash
rm $HOME/.local/bin/delta
rm $HOME/.local/bin/delta-server
```

**If you used Option 4 (symlink):**
```bash
rm /usr/local/bin/delta-test
rm /usr/local/bin/delta-server-test
```

## Recommended Workflow

For quick testing, use **Option 1** (direct run):

```bash
# Quick test
cd /Users/suzanodero/io/GITHUB/delta
./build_macos/delta --version
./build_macos/delta
```

This requires no installation and is completely isolated from your system.

