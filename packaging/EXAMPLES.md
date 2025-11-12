# Practical Examples - Using the Packaging Scripts

This guide shows you exactly how to use each script with real examples.

## Example 1: Build and Package for macOS (Most Common)

### Step 1: Build the Binary

```bash
# Navigate to your project
cd /Users/suzanodero/Downloads/delta-cli

# Build for macOS ARM64 (Apple Silicon)
./packaging/build-scripts/build-macos.sh Release arm64
```

**What this does:**
- Cleans any old build
- Configures CMake with Metal support
- Compiles Delta CLI
- Outputs to `build_macos_arm64/delta` and `build_macos_arm64/delta-server`

**Expected output:**
```
🔨 Building Delta CLI for macOS (arm64)...
✅ Build completed successfully!
📦 Binaries are in: /Users/suzanodero/Downloads/delta-cli/build_macos_arm64
```

### Step 2: Test the Binary

```bash
# Test it works
./build_macos_arm64/delta --version

# Or run it
./build_macos_arm64/delta
```

### Step 3: Create Release Package

```bash
# Package it for distribution
./packaging/release/package-macos.sh 1.0.0 arm64
```

**What this does:**
- Creates `release/delta-cli-macos-arm64/` directory
- Copies binaries and web UI
- Creates `release/delta-cli-macos-arm64.tar.gz`
- Shows SHA256 hash

**Expected output:**
```
📦 Packaging Delta CLI for macOS (arm64)...
✅ Package created: /Users/suzanodero/Downloads/delta-cli/release/delta-cli-macos-arm64.tar.gz
📋 SHA256: abc123def4567890...
```

### Step 4: Use the SHA256 Hash

Copy the SHA256 hash and update your package definitions:

```bash
# View the package
ls -lh release/delta-cli-macos-arm64.tar.gz

# Get SHA256 again if needed
shasum -a 256 release/delta-cli-macos-arm64.tar.gz
```

Then update `packaging/homebrew/delta-cli.rb`:
```ruby
sha256 "abc123def4567890..."  # Replace PLACEHOLDER_SHA256
```

---

## Example 2: Build for Multiple Architectures

### Build macOS ARM64

```bash
./packaging/build-scripts/build-macos.sh Release arm64
```

### Build macOS x86_64 (Intel)

```bash
./packaging/build-scripts/build-macos.sh Release x86_64
```

### Package Both

```bash
# Package ARM64
./packaging/release/package-macos.sh 1.0.0 arm64

# Package x86_64
./packaging/release/package-macos.sh 1.0.0 x86_64
```

### Package All at Once

```bash
# This packages all available builds
./packaging/release/package-all.sh 1.0.0
```

---

## Example 3: Complete Release Workflow

Here's a complete example for releasing version 1.0.0:

```bash
# 1. Build
cd /Users/suzanodero/Downloads/delta-cli
./packaging/build-scripts/build-macos.sh Release arm64

# 2. Test
./build_macos_arm64/delta --version

# 3. Package
./packaging/release/package-macos.sh 1.0.0 arm64

# 4. Note the SHA256 from output, then update:
#    - packaging/homebrew/delta-cli.rb
#    - packaging/winget/delta-cli.yaml
#    - packaging/nix/default.nix

# 5. Create GitHub release and upload:
#    release/delta-cli-macos-arm64.tar.gz

# 6. Submit package definitions to package managers
```

---

## Example 4: Windows Build (if you have Windows)

```powershell
# Navigate to project
cd C:\path\to\delta-cli

# Build
.\packaging\build-scripts\build-windows.ps1 Release x64

# Package
.\packaging\release\package-windows.ps1 1.0.0 x64
```

---

## Example 5: Linux Build

```bash
# Build for Linux x86_64
./packaging/build-scripts/build-linux.sh Release x86_64

# Package
./packaging/release/package-linux.sh 1.0.0 x86_64
```

---

## Common Commands Reference

### Build Commands

```bash
# macOS ARM64
./packaging/build-scripts/build-macos.sh Release arm64

# macOS Intel
./packaging/build-scripts/build-macos.sh Release x86_64

# Linux x86_64
./packaging/build-scripts/build-linux.sh Release x86_64

# Linux ARM64
./packaging/build-scripts/build-linux.sh Release aarch64

# Debug build (for testing)
./packaging/build-scripts/build-macos.sh Debug arm64
```

### Package Commands

```bash
# Package specific platform
./packaging/release/package-macos.sh 1.0.0 arm64
./packaging/release/package-linux.sh 1.0.0 x86_64

# Package all built platforms
./packaging/release/package-all.sh 1.0.0
```

### Verification Commands

```bash
# Check build exists
ls -lh build_macos_arm64/delta

# Test binary
./build_macos_arm64/delta --version

# List packages
ls -lh release/

# Get SHA256
shasum -a 256 release/delta-cli-macos-arm64.tar.gz
```

---

## Troubleshooting

### Script Not Executable

```bash
chmod +x packaging/build-scripts/*.sh
chmod +x packaging/release/*.sh
```

### Build Fails

```bash
# Check CMake version
cmake --version  # Need 3.14+

# Check dependencies
which curl
which pkg-config

# macOS: Install Xcode Command Line Tools
xcode-select --install
```

### Package Creation Fails

```bash
# Ensure build completed successfully
ls build_macos_arm64/delta

# Check release directory exists
mkdir -p release
```

### Wrong Architecture

```bash
# Check what you built
file build_macos_arm64/delta

# Should show: Mach-O 64-bit executable arm64
```

---

## Quick Test Workflow

Want to test everything quickly? Run this:

```bash
cd /Users/suzanodero/Downloads/delta-cli

# Build
./packaging/build-scripts/build-macos.sh Release arm64

# Test
./build_macos_arm64/delta --version

# Package
./packaging/release/package-macos.sh 1.0.0 arm64

# Verify
ls -lh release/
tar -tzf release/delta-cli-macos-arm64.tar.gz | head -5
```

---

## Next Steps After Packaging

1. **Update package definitions** with SHA256 hashes
2. **Create GitHub release** and upload packages
3. **Submit to package managers** (see `HOW_TO_USE.md`)
4. **Test installation** via package managers

For detailed submission instructions, see `packaging/HOW_TO_USE.md`.

