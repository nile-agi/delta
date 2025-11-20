# Building .deb Packages for Multiple Architectures

This guide explains how to create separate .deb packages for different CPU architectures (amd64, arm64, etc.).

## Quick Start

To build packages for all supported architectures:

```bash
./installers/package_all_architectures.sh
```

This will create separate .deb files:
- `delta-cli_1.0.0_amd64.deb` - For Intel/AMD 64-bit systems
- `delta-cli_1.0.0_arm64.deb` - For ARM 64-bit systems (Apple Silicon, Raspberry Pi 4/5, etc.)

## Step-by-Step Process

### 1. Build for Each Architecture

You need to build the application for each target architecture:

#### For amd64 (Intel/AMD 64-bit):
```bash
# On an amd64 system or using cross-compilation
ARCH=amd64 ./installers/build_linux.sh
# Or rename build directory
mv build_linux_release build_linux_release_amd64
```

#### For arm64 (ARM 64-bit):
```bash
# On an arm64 system (Apple Silicon, Raspberry Pi, etc.)
ARCH=arm64 ./installers/build_linux.sh
# Or rename build directory
mv build_linux_release build_linux_release_arm64
```

### 2. Build Packages

#### Option A: Build All Architectures at Once

```bash
./installers/package_all_architectures.sh
```

This script will:
- Look for builds in directories like `build_linux_release_amd64`, `build_linux_release_arm64`
- Create a separate .deb package for each architecture found
- Output all packages to `installers/packages/`

#### Option B: Build Individual Packages

```bash
# For amd64
BUILD_DIR=build_linux_release_amd64 ARCH=amd64 ./installers/package_linux_deb.sh

# For arm64
BUILD_DIR=build_linux_release_arm64 ARCH=arm64 ./installers/package_linux_deb.sh
```

## Directory Structure

After building, you should have:

```
delta/
├── build_linux_release_amd64/    # amd64 build
│   └── delta
├── build_linux_release_arm64/    # arm64 build
│   └── delta
└── installers/
    └── packages/
        ├── delta-cli_1.0.0_amd64.deb
        └── delta-cli_1.0.0_arm64.deb
```

## Installation

Users should install the package matching their system architecture:

### Check System Architecture

```bash
# On Debian/Ubuntu
dpkg --print-architecture

# On any Linux
uname -m
```

### Install the Correct Package

```bash
# For amd64 systems
sudo dpkg -i delta-cli_1.0.0_amd64.deb

# For arm64 systems
sudo dpkg -i delta-cli_1.0.0_arm64.deb
```

## Common Issues

### "package architecture does not match system"

**Error:**
```
dpkg: error processing archive delta-cli_1.0.0_arm64.deb (--install):
 package architecture (arm64) does not match system (amd64)
```

**Solution:**
- Install the package matching your system architecture
- Check your architecture: `dpkg --print-architecture`
- Download the correct .deb file for your system

### "tar: Ignoring unknown extended header keyword"

**Warning:**
```
tar: Ignoring unknown extended header keyword 'LIBARCHIVE.xattr.com.apple.provenance'
```

**Solution:**
- This is fixed in the latest version of the packaging script
- The script now removes macOS extended attributes before creating the package
- If you see this, rebuild the package with the updated script

### Cross-Compilation

To build for a different architecture than your current system:

#### Using Docker (Recommended)

```bash
# Build for amd64 on any system
docker run --rm -v $(pwd):/workspace -w /workspace \
  ubuntu:22.04 bash -c "apt-get update && apt-get install -y build-essential cmake && ./installers/build_linux.sh"

# Build for arm64 on any system
docker run --rm --platform linux/arm64 -v $(pwd):/workspace -w /workspace \
  ubuntu:22.04 bash -c "apt-get update && apt-get install -y build-essential cmake && ./installers/build_linux.sh"
```

#### Using QEMU (Linux only)

```bash
# Install QEMU
sudo apt-get install qemu-user-static binfmt-support

# Build for arm64 on amd64 system
sudo update-binfmts --enable qemu-arm
ARCH=arm64 ./installers/build_linux.sh
```

## Distribution

When distributing packages:

1. **Upload both architectures** to your release page:
   - `delta-cli_1.0.0_amd64.deb`
   - `delta-cli_1.0.0_arm64.deb`

2. **Provide clear instructions**:
   - How to check system architecture
   - Which package to download
   - Installation commands

3. **Include checksums**:
   ```bash
   sha256sum delta-cli_1.0.0_*.deb > checksums.txt
   ```

## Supported Architectures

Currently supported:
- **amd64** (x86_64) - Intel/AMD 64-bit processors
- **arm64** (aarch64) - ARM 64-bit processors (Apple Silicon, Raspberry Pi 4/5, etc.)

To add more architectures, update the `ARCHITECTURES` array in `package_all_architectures.sh`.

## Verification

After building, verify the package architecture:

```bash
# Check package info
dpkg-deb -I delta-cli_1.0.0_amd64.deb | grep Architecture

# Should show: Architecture: amd64
```

## Best Practices

1. **Build on native systems** when possible for best compatibility
2. **Test packages** on the target architecture before distribution
3. **Use consistent versioning** across all architectures
4. **Include architecture in filename** (already done automatically)
5. **Provide checksums** for verification
6. **Document architecture requirements** clearly

---

For more information, see:
- [Ubuntu Installation Guide](./UBUNTU_INSTALLATION.md)
- [Package Linux DEB Script](./package_linux_deb.sh)

