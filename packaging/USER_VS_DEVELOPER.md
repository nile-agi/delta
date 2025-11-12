# User vs Developer Scripts

## Important Distinction

There are **two types of scripts** in this project:

1. **Packaging Scripts** (for developers) - Create distribution packages
2. **Installation Scripts** (for end users) - Install Delta CLI

---

## For End Users (Installation)

End users should use these **installation scripts** to install Delta CLI:

### Quick Install

**macOS / Linux:**
```bash
curl -fsSL https://raw.githubusercontent.com/oderoi/delta-cli/main/install.sh | bash
```

**Windows:**
```powershell
Invoke-WebRequest -Uri https://raw.githubusercontent.com/oderoi/delta-cli/main/install.ps1 -OutFile install.ps1
.\install.ps1
```

### Package Manager Install (Recommended)

**Windows:**
```powershell
winget install DeltaCLI.DeltaCLI
```

**macOS:**
```bash
brew install oderoi/delta-cli/delta-cli
```

**Linux:**
```bash
brew install oderoi/delta-cli/delta-cli
# or
nix-env -iA nixpkgs.delta-cli
```

### Manual Install

1. Download from [GitHub Releases](https://github.com/oderoi/delta-cli/releases)
2. Extract the archive
3. Copy binaries to a directory in PATH

**See:** `INSTALL.md` for complete installation instructions

---

## For Developers (Packaging)

Developers use these **packaging scripts** to create distribution packages:

### Build Scripts

```bash
# Build Delta CLI
./packaging/build-scripts/build-macos.sh Release arm64
```

### Package Scripts

```bash
# Create release package
./packaging/release/package-macos.sh 1.0.0 arm64
```

**See:** `packaging/HOW_TO_USE.md` for developer instructions

---

## Summary

| Who | What They Use | Purpose |
|-----|---------------|---------|
| **End Users** | `install.sh` / `install.ps1` | Install Delta CLI on their system |
| **End Users** | Package managers (Winget, Homebrew, etc.) | Install Delta CLI via package manager |
| **Developers** | `packaging/build-scripts/*.sh` | Build Delta CLI from source |
| **Developers** | `packaging/release/*.sh` | Create distribution packages |

---

## File Locations

### End User Installation Files

- `install.sh` - Installation script for macOS/Linux
- `install.ps1` - Installation script for Windows
- `INSTALL.md` - Complete installation guide

### Developer Packaging Files

- `packaging/build-scripts/` - Build scripts
- `packaging/release/` - Packaging scripts
- `packaging/*/` - Package manager definitions
- `packaging/HOW_TO_USE.md` - Developer guide

---

## Quick Reference

**End users installing Delta:**
→ Use `install.sh` or package managers (see `INSTALL.md`)

**Developers creating packages:**
→ Use `packaging/build-scripts/` and `packaging/release/` (see `packaging/HOW_TO_USE.md`)

