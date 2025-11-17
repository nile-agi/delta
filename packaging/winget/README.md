# Winget Package Manifest for Delta CLI

This directory contains the Windows Package Manager (winget) manifest for Delta CLI.

## Current Status

⚠️ **The package is not yet published to the winget repository.**

The manifest file exists but needs to be submitted to the [winget-pkgs](https://github.com/microsoft/winget-pkgs) repository before `winget install DeltaCLI.DeltaCLI` will work.

## Files

- `delta-cli.yaml` - Winget manifest file
- `update-sha256.ps1` - Helper script to update SHA256 hash in manifest
- `submit-to-winget.ps1` - **Automated submission script (recommended)**
- `submit-to-winget.sh` - Cross-platform preparation script
- `SUBMIT.md` - Detailed manual submission guide
- `README.md` - This file

## Quick Start

### For Windows Users

```powershell
# Build Delta CLI for Windows
.\packaging\build-scripts\build-windows.ps1 Release x64

# Create release package
.\packaging\release\package-windows.ps1 1.0.0 x64

# Automated submission
.\packaging\winget\submit-to-winget.ps1 -Version 1.0.0 -GitHubUsername YOUR_USERNAME
```

This will create `release/delta-cli-windows-x64.zip` and display the SHA256 hash.

### For macOS/Linux Users

**See [MACOS_GUIDE.md](MACOS_GUIDE.md) for complete instructions.**

Quick steps:
1. Get the Windows ZIP file (from CI/CD or Windows build)
2. Prepare manifest: `./packaging/winget/submit-to-winget.sh 1.0.0`
3. Follow the instructions shown by the script

### 2. Update Manifest with SHA256

```powershell
cd packaging\winget
.\update-sha256.ps1 ..\..\release\delta-cli-windows-x64.zip 1.0.0
```

This script will:
- Calculate SHA256 hash of the ZIP file
- Update `InstallerSha256` in the manifest
- Update `PackageVersion` in the manifest
- Update the download URL version

### 3. Validate Manifest

```powershell
winget validate delta-cli.yaml
```

### 4. Submit to Winget Repository

**Option A: Automated Submission (Recommended)**

```powershell
# Run the automated submission script
.\packaging\winget\submit-to-winget.ps1 -Version 1.0.0 -GitHubUsername YOUR_USERNAME
```

This script will:
- ✅ Update manifest with SHA256 hash
- ✅ Validate the manifest
- ✅ Check if GitHub release exists
- ✅ Clone/fork winget-pkgs repository
- ✅ Create manifest directory structure
- ✅ Copy manifest to correct location
- ✅ Validate in winget-pkgs structure
- ✅ Prepare git commit
- ✅ Guide you through creating the PR

**Option B: Manual Submission**

See [SUBMIT.md](SUBMIT.md) for detailed manual steps.

### 4. Submit to Winget Repository (Manual)

1. **Fork the winget-pkgs repository:**
   - Go to https://github.com/microsoft/winget-pkgs
   - Click "Fork"

2. **Clone your fork:**
   ```powershell
   git clone https://github.com/YOUR_USERNAME/winget-pkgs.git
   cd winget-pkgs
   ```

3. **Create manifest directory:**
   ```powershell
   mkdir -p manifests\d\DeltaCLI\DeltaCLI\1.0.0
   ```

4. **Copy manifest:**
   ```powershell
   copy ..\..\delta\packaging\winget\delta-cli.yaml manifests\d\DeltaCLI\DeltaCLI\1.0.0\delta-cli.yaml
   ```

5. **Validate:**
   ```powershell
   winget validate manifests\d\DeltaCLI\DeltaCLI\1.0.0\delta-cli.yaml
   ```

6. **Commit and push:**
   ```powershell
   git add manifests\d\DeltaCLI\DeltaCLI\1.0.0\
   git commit -m "Add DeltaCLI.DeltaCLI version 1.0.0"
   git push origin main
   ```

7. **Create Pull Request:**
   - Go to your fork on GitHub
   - Click "New Pull Request"
   - Submit the PR

## Manifest Structure

The manifest follows winget schema version 1.4.0 and includes:

- **PackageIdentifier**: `DeltaCLI.DeltaCLI`
- **PackageVersion**: Version number (e.g., `1.0.0`)
- **InstallerType**: `zip`
- **InstallerUrl**: GitHub release URL
- **InstallerSha256**: SHA256 hash of the ZIP file (required)
- **InstallLocation**: `C:\Program Files\Delta CLI`
- **Scope**: `machine` (system-wide installation)

## Troubleshooting

### "No package found matching input criteria"

This error means the package hasn't been published to the winget repository yet. You need to:

1. Ensure the manifest is correct (validate with `winget validate`)
2. Submit the manifest to the winget-pkgs repository
3. Wait for the PR to be merged and the package to be indexed

### Manifest Validation Errors

If `winget validate` reports errors:

1. Check that `InstallerSha256` is a valid 64-character hexadecimal string
2. Verify `InstallerUrl` points to an actual release
3. Ensure `PackageVersion` follows semantic versioning (e.g., `1.0.0`)
4. Check that all required fields are present

### Testing Locally

You can test the manifest locally before submitting:

```powershell
# Validate manifest
winget validate delta-cli.yaml

# Test install from local manifest (requires manifest to be in winget-pkgs structure)
winget install --manifest delta-cli.yaml --location "C:\Program Files\Delta CLI"
```

## Updating for New Versions

When releasing a new version:

1. Build and package the new version:
   ```powershell
   .\packaging\release\package-windows.ps1 1.1.0 x64
   ```

2. Update the manifest:
   ```powershell
   cd packaging\winget
   .\update-sha256.ps1 ..\..\release\delta-cli-windows-x64.zip 1.1.0
   ```

3. Submit to winget-pkgs following the same process as above, but use the new version directory:
   ```powershell
   mkdir -p manifests\d\DeltaCLI\DeltaCLI\1.1.0
   copy delta-cli.yaml manifests\d\DeltaCLI\DeltaCLI\1.1.0\
   ```

## Resources

- [Winget Manifest Documentation](https://learn.microsoft.com/en-us/windows/package-manager/package/manifest)
- [Winget-Pkgs Repository](https://github.com/microsoft/winget-pkgs)
- [Winget Validation Tool](https://github.com/microsoft/winget-cli)

