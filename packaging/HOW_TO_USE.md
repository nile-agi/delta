# How to Use the Packaging System

This guide walks you through using the packaging infrastructure to build and distribute Delta CLI.

## Table of Contents

1. [Building Binaries](#building-binaries)
2. [Creating Release Packages](#creating-release-packages)
3. [Updating Package Definitions](#updating-package-definitions)
4. [Submitting to Package Managers](#submitting-to-package-managers)
5. [Complete Workflow Example](#complete-workflow-example)

## Building Binaries

### Step 1: Build for Your Platform

#### macOS (ARM64 - Apple Silicon)

```bash
cd /Users/suzanodero/Downloads/delta-cli
./packaging/build-scripts/build-macos.sh Release arm64
```

This will:
- Clean any previous build
- Configure CMake with Metal support
- Build Delta CLI and delta-server
- Output binaries to `build_macos_arm64/`

#### macOS (x86_64 - Intel)

```bash
./packaging/build-scripts/build-macos.sh Release x86_64
```

#### Linux (x86_64)

```bash
./packaging/build-scripts/build-linux.sh Release x86_64
```

#### Linux (aarch64)

```bash
./packaging/build-scripts/build-linux.sh Release aarch64
```

#### Windows (x64)

```powershell
cd C:\path\to\delta-cli
.\packaging\build-scripts\build-windows.ps1 Release x64
```

### Step 2: Verify Build

After building, verify the binaries exist:

```bash
# macOS/Linux
ls -lh build_macos_arm64/delta build_macos_arm64/delta-server

# Windows
dir build_windows\Release\delta.exe
dir build_windows\Release\delta-server.exe
```

Test the binary:

```bash
./build_macos_arm64/delta --version
```

## Creating Release Packages

### Step 1: Package Individual Platform

#### macOS

```bash
./packaging/release/package-macos.sh 1.0.0 arm64
```

This creates:
- `release/delta-cli-macos-arm64.tar.gz`
- Displays SHA256 hash

#### Linux

```bash
./packaging/release/package-linux.sh 1.0.0 x86_64
```

#### Windows

```powershell
.\packaging\release\package-windows.ps1 1.0.0 x64
```

### Step 2: Package All Platforms

If you've built for multiple platforms:

```bash
./packaging/release/package-all.sh 1.0.0
```

This packages all available builds and shows:
- Package locations
- SHA256 hashes for each

### Step 3: Verify Packages

```bash
# List all packages
ls -lh release/

# Extract and test (macOS example)
cd /tmp
tar -xzf /path/to/release/delta-cli-macos-arm64.tar.gz
./delta-cli-macos-arm64/delta --version
```

## Updating Package Definitions

### Step 1: Update Version Numbers

Edit these files and update version from `1.0.0` to your new version:

```bash
# Winget
packaging/winget/delta-cli.yaml

# Homebrew
packaging/homebrew/delta-cli.rb

# MacPorts
packaging/macports/Portfile

# Nix
packaging/nix/default.nix
```

### Step 2: Update SHA256 Hashes

After creating packages, copy the SHA256 hashes and update:

#### Winget (`delta-cli.yaml`)

```yaml
InstallerSha256: "YOUR_ACTUAL_SHA256_HASH_HERE"
```

#### Homebrew (`delta-cli.rb`)

```ruby
sha256 "YOUR_ACTUAL_SHA256_HASH_HERE"
```

#### Nix (`default.nix`)

```nix
sha256 = "YOUR_ACTUAL_SHA256_HASH_HERE";
```

### Step 3: Update Download URLs

Update URLs to point to your GitHub release:

#### Winget

```yaml
InstallerUrl: https://github.com/oderoi/delta-cli/releases/download/v1.0.0/delta-cli-windows-x64.zip
```

#### Homebrew

```ruby
url "https://github.com/oderoi/delta-cli/archive/refs/tags/v1.0.0.tar.gz"
```

## Submitting to Package Managers

### Winget (Windows)

1. **Fork the repository:**
   ```bash
   # Fork https://github.com/microsoft/winget-pkgs
   ```

2. **Clone your fork:**
   ```bash
   git clone https://github.com/YOUR_USERNAME/winget-pkgs.git
   cd winget-pkgs
   ```

3. **Create manifest directory:**
   ```bash
   mkdir -p manifests/d/DeltaCLI/DeltaCLI/1.0.0
   ```

4. **Copy manifest:**
   ```bash
   cp /path/to/delta-cli/packaging/winget/delta-cli.yaml \
      manifests/d/DeltaCLI/DeltaCLI/1.0.0/delta-cli.yaml
   ```

5. **Validate:**
   ```powershell
   winget validate manifests/d/DeltaCLI/DeltaCLI/1.0.0/delta-cli.yaml
   ```

6. **Submit PR:**
   ```bash
   git add manifests/d/DeltaCLI/DeltaCLI/1.0.0/
   git commit -m "Add Delta CLI 1.0.0"
   git push origin main
   # Create PR on GitHub
   ```

### Homebrew (macOS/Linux)

#### Option 1: Create a Tap (Recommended for testing)

1. **Create new repository:** `homebrew-delta-cli`

2. **Add formula:**
   ```bash
   mkdir -p Formula
   cp packaging/homebrew/delta-cli.rb Formula/delta-cli.rb
   ```

3. **Install from tap:**
   ```bash
   brew install oderoi/delta-cli/delta-cli
   ```

#### Option 2: Submit to Homebrew Core

1. **Fork homebrew-core:**
   ```bash
   # Fork https://github.com/Homebrew/homebrew-core
   ```

2. **Add formula:**
   ```bash
   git clone https://github.com/YOUR_USERNAME/homebrew-core.git
   cd homebrew-core
   cp /path/to/delta-cli/packaging/homebrew/delta-cli.rb Formula/delta-cli.rb
   ```

3. **Test locally:**
   ```bash
   brew install --build-from-source Formula/delta-cli.rb
   ```

4. **Submit PR:**
   ```bash
   git add Formula/delta-cli.rb
   git commit -m "delta-cli 1.0.0"
   git push origin main
   # Create PR on GitHub
   ```

### MacPorts (macOS)

1. **Fork macports-ports:**
   ```bash
   # Fork https://github.com/macports/macports-ports
   ```

2. **Create port directory:**
   ```bash
   git clone https://github.com/YOUR_USERNAME/macports-ports.git
   cd macports-ports
   mkdir -p ai/delta-cli
   ```

3. **Copy Portfile:**
   ```bash
   cp /path/to/delta-cli/packaging/macports/Portfile ai/delta-cli/Portfile
   ```

4. **Test locally:**
   ```bash
   sudo port install delta-cli
   ```

5. **Submit PR:**
   ```bash
   git add ai/delta-cli/
   git commit -m "Add delta-cli port"
   git push origin main
   # Create PR on GitHub
   ```

### Nix (macOS/Linux)

1. **Fork nixpkgs:**
   ```bash
   # Fork https://github.com/NixOS/nixpkgs
   ```

2. **Add package:**
   ```bash
   git clone https://github.com/YOUR_USERNAME/nixpkgs.git
   cd nixpkgs
   mkdir -p pkgs/applications/misc/delta-cli
   cp /path/to/delta-cli/packaging/nix/default.nix \
      pkgs/applications/misc/delta-cli/default.nix
   ```

3. **Test locally:**
   ```bash
   nix-build -A delta-cli
   ```

4. **Submit PR:**
   ```bash
   git add pkgs/applications/misc/delta-cli/
   git commit -m "delta-cli: init at 1.0.0"
   git push origin main
   # Create PR on GitHub
   ```

## Complete Workflow Example

Here's a complete example for releasing version 1.0.0:

### 1. Build Binaries

```bash
# Build for macOS ARM64
./packaging/build-scripts/build-macos.sh Release arm64

# Build for Linux x86_64 (if you have access)
# ./packaging/build-scripts/build-linux.sh Release x86_64
```

### 2. Create Packages

```bash
# Package macOS
./packaging/release/package-macos.sh 1.0.0 arm64

# Output will show:
# ✅ Package created: release/delta-cli-macos-arm64.tar.gz
# 📋 SHA256: abc123def456...
```

### 3. Create GitHub Release

1. Go to GitHub repository
2. Click "Releases" → "Create a new release"
3. Tag: `v1.0.0`
4. Title: `Delta CLI 1.0.0`
5. Upload all packages from `release/` directory
6. Publish release

### 4. Update Package Definitions

```bash
# Copy SHA256 from package output
# Update packaging/winget/delta-cli.yaml
# Update packaging/homebrew/delta-cli.rb
# Update packaging/nix/default.nix
```

### 5. Submit to Package Managers

Follow the submission steps above for each package manager.

### 6. Test Installation

After packages are accepted:

```bash
# Winget
winget install DeltaCLI.DeltaCLI

# Homebrew
brew install oderoi/delta-cli/delta-cli

# MacPorts
sudo port install delta-cli

# Nix
nix-env -iA nixpkgs.delta-cli
```

## Troubleshooting

### Build Fails

- Check CMake version: `cmake --version` (need 3.14+)
- Check dependencies: `curl`, `pkg-config`
- macOS: Ensure Xcode Command Line Tools installed
- Linux: Install build essentials

### Package Creation Fails

- Ensure binaries exist in build directory
- Check file permissions: `chmod +x packaging/release/*.sh`
- Verify paths are correct

### Package Manager Rejection

- Ensure SHA256 hashes are correct
- Verify URLs point to actual releases
- Check package manager guidelines
- Ensure version format matches requirements

## Quick Reference

```bash
# Build
./packaging/build-scripts/build-macos.sh Release arm64

# Package
./packaging/release/package-macos.sh 1.0.0 arm64

# Get SHA256
shasum -a 256 release/delta-cli-macos-arm64.tar.gz

# Update package definition with SHA256
# Submit to package manager
```

For more details, see:
- `packaging/README.md` - Full documentation
- `packaging/QUICK_START.md` - Quick reference
- `packaging/INSTALLATION.md` - Installation instructions

