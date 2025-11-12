# Quick Start - Packaging Delta CLI

## Overview

Delta CLI is now packaged for distribution via multiple package managers:

| Platform | Package Manager | File Location |
|----------|----------------|---------------|
| Windows  | Winget         | `packaging/winget/delta-cli.yaml` |
| macOS    | Homebrew       | `packaging/homebrew/delta-cli.rb` |
| macOS    | MacPorts       | `packaging/macports/Portfile` |
| macOS/Linux | Nix        | `packaging/nix/default.nix` |

## Quick Build & Package

### 1. Build for your platform:

**macOS:**
```bash
./packaging/build-scripts/build-macos.sh Release arm64
```

**Linux:**
```bash
./packaging/build-scripts/build-linux.sh Release x86_64
```

**Windows:**
```powershell
.\packaging\build-scripts\build-windows.ps1 Release x64
```

### 2. Package for release:

**macOS:**
```bash
./packaging/release/package-macos.sh 1.0.0 arm64
```

**Linux:**
```bash
./packaging/release/package-linux.sh 1.0.0 x86_64
```

**Windows:**
```powershell
.\packaging\release\package-windows.ps1 1.0.0 x64
```

**All platforms:**
```bash
./packaging/release/package-all.sh 1.0.0
```

## Package Manager Submission

### Winget (Windows)
1. Update `packaging/winget/delta-cli.yaml` with:
   - Actual `InstallerUrl` (GitHub release URL)
   - Actual `InstallerSha256` (from package output)
2. Submit to: https://github.com/microsoft/winget-pkgs

### Homebrew (macOS/Linux)
1. Update `packaging/homebrew/delta-cli.rb` with:
   - Actual `url` (GitHub release tarball)
   - Actual `sha256` (from package output)
2. Create tap or submit to: https://github.com/Homebrew/homebrew-core

### MacPorts (macOS)
1. Update `packaging/macports/Portfile` with:
   - Actual version and checksums
2. Submit to: https://github.com/macports/macports-ports

### Nix (macOS/Linux)
1. Update `packaging/nix/default.nix` with:
   - Actual `sha256` (from GitHub release)
2. Submit to: https://github.com/NixOS/nixpkgs

## Testing Installation

After submitting packages, test installation:

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

## Release Workflow

1. **Build binaries** for all target platforms
2. **Package releases** using packaging scripts
3. **Update package definitions** with SHA256 hashes
4. **Create GitHub release** with all packages
5. **Submit package definitions** to respective repositories
6. **Update documentation** with installation instructions

## Files Created

```
packaging/
├── build-scripts/
│   ├── build-windows.ps1    # Windows build script
│   ├── build-macos.sh       # macOS build script
│   └── build-linux.sh       # Linux build script
├── winget/
│   └── delta-cli.yaml       # Winget manifest
├── homebrew/
│   └── delta-cli.rb         # Homebrew formula
├── macports/
│   └── Portfile             # MacPorts Portfile
├── nix/
│   └── default.nix          # Nix package definition
├── release/
│   ├── package-windows.ps1  # Windows packaging script
│   ├── package-macos.sh    # macOS packaging script
│   ├── package-linux.sh     # Linux packaging script
│   └── package-all.sh       # Package all platforms
├── README.md                # Full documentation
├── INSTALLATION.md          # Installation instructions
└── QUICK_START.md           # This file
```

## Next Steps

1. Review package definitions and update version numbers
2. Build binaries for all target platforms
3. Create release packages
4. Update SHA256 hashes in package definitions
5. Submit to package manager repositories
6. Announce availability!

For detailed information, see [README.md](README.md).

