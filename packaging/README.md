# Delta CLI Packaging

This directory contains packaging definitions and scripts for distributing Delta CLI through various package managers.

## Package Managers Supported

| Platform | Package Manager | Status |
|----------|----------------|--------|
| Windows  | Winget         | ✅     |
| macOS    | Homebrew       | ✅     |
| macOS    | MacPorts       | ✅     |
| macOS   | Nix            | ✅     |
| Linux    | Homebrew       | ✅     |
| Linux    | Nix            | ✅     |

## Directory Structure

```
packaging/
├── build-scripts/      # Platform-specific build scripts
├── winget/            # Windows Package Manager (Winget) manifest
├── homebrew/          # Homebrew formula (macOS/Linux)
├── macports/          # MacPorts Portfile (macOS)
├── nix/               # Nix package definition (macOS/Linux)
├── release/           # Release packaging scripts
└── README.md          # This file
```

## Building for Release

### Prerequisites

- CMake 3.14 or higher
- C++17 compatible compiler
- curl development libraries
- (macOS) Xcode Command Line Tools

### Build Scripts

#### Windows
```powershell
.\packaging\build-scripts\build-windows.ps1 [Release|Debug] [x64|x86]
```

#### macOS
```bash
./packaging/build-scripts/build-macos.sh [Release|Debug] [arm64|x86_64]
```

#### Linux
```bash
./packaging/build-scripts/build-linux.sh [Release|Debug] [x86_64|aarch64]
```

## Creating Release Packages

### Individual Platforms

#### Windows
```powershell
.\packaging\release\package-windows.ps1 [version] [arch]
```

#### macOS
```bash
./packaging/release/package-macos.sh [version] [arch]
```

#### Linux
```bash
./packaging/release/package-linux.sh [version] [arch]
```

### All Platforms
```bash
./packaging/release/package-all.sh [version]
```

This will create packages for all built architectures in the `release/` directory.

## Package Manager Installation Instructions

### Winget (Windows)

1. **Submit to Winget-Pkgs repository:**
   ```powershell
   # Fork https://github.com/microsoft/winget-pkgs
   # Copy packaging/winget/delta-cli.yaml to manifests/d/DeltaCLI/DeltaCLI/1.0.0/
   # Update InstallerUrl and InstallerSha256 with actual release values
   # Submit PR
   ```

2. **Install via Winget:**
   ```powershell
   winget install DeltaCLI.DeltaCLI
   ```

### Homebrew (macOS/Linux)

1. **Create Homebrew tap:**
   ```bash
   # Create a new repository: homebrew-delta-cli
   # Copy packaging/homebrew/delta-cli.rb to Formula/delta-cli.rb
   # Update url and sha256 with actual release values
   ```

2. **Install via Homebrew:**
   ```bash
   brew install oderoi/delta-cli/delta-cli
   ```

   Or add to official Homebrew:
   ```bash
   # Submit PR to homebrew-core
   brew install delta-cli
   ```

### MacPorts (macOS)

1. **Submit to MacPorts:**
   ```bash
   # Copy packaging/macports/Portfile to a new port
   # Submit PR to https://github.com/macports/macports-ports
   ```

2. **Install via MacPorts:**
   ```bash
   sudo port install delta-cli
   ```

### Nix (macOS/Linux)

1. **Add to Nixpkgs:**
   ```bash
   # Copy packaging/nix/default.nix to nixpkgs/pkgs/applications/misc/delta-cli/default.nix
   # Update sha256 with actual release value
   # Submit PR to https://github.com/NixOS/nixpkgs
   ```

2. **Install via Nix:**
   ```bash
   nix-env -iA nixpkgs.delta-cli
   ```

   Or with Nix Flakes:
   ```bash
   nix profile install nixpkgs#delta-cli
   ```

## Release Checklist

- [ ] Update version numbers in all package definitions
- [ ] Build binaries for all target platforms
- [ ] Create release packages using packaging scripts
- [ ] Calculate and update SHA256 hashes in package definitions
- [ ] Update download URLs in package definitions
- [ ] Test installation on each platform
- [ ] Submit package definitions to respective repositories
- [ ] Create GitHub release with all packages
- [ ] Update documentation with installation instructions

## Version Management

When releasing a new version:

1. Update `CMakeLists.txt` version
2. Update all package definition files with new version
3. Update SHA256 hashes after building
4. Tag release in Git: `git tag v1.0.0`
5. Push tag: `git push origin v1.0.0`

## Notes

- All package definitions use placeholder SHA256 values that must be updated after building
- Download URLs should point to GitHub releases
- Web UI is included in all packages if available
- Binaries are statically linked where possible for portability

