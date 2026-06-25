# Guide: Creating delta-cli-windows-x64.zip for Winget

This guide explains how to create the `delta-cli-windows-x64.zip` file required for Windows Package Manager (winget) installation.

## Overview

The winget installer requires a ZIP file with a specific structure:
- Executables (`delta.exe`, `delta-server.exe`) must be at the **root level**
- Any required DLLs should be included
- The ZIP file is uploaded to GitHub releases

## Prerequisites

- Windows 10/11
- Visual Studio 2019+ or Build Tools
- CMake
- PowerShell 5.1+ (for PowerShell scripts)
- Git (for submodules)

## Step-by-Step Process

### Step 1: Prepare the Build Environment

1. **Open Visual Studio Developer Command Prompt**:
   - Search for "Developer Command Prompt for VS" in Start menu
   - Or run: `"C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\Tools\VsDevCmd.bat"`

2. **Navigate to the delta repository**:
   ```batch
   cd C:\path\to\delta
   ```

3. **Initialize submodules** (if not already done):
   ```batch
   git submodule update --init --recursive
   ```

### Step 2: Build Delta CLI

Build the project using the provided script:

```batch
installers\build_windows.bat
```

This will:
- Configure CMake with Release build
- Build `delta.exe` and `delta-server.exe`
- Run tests
- Output binaries to `build_windows\Release\`

**Expected output:**
- `build_windows\Release\delta.exe`
- `build_windows\Release\delta-server.exe`

**If build fails:**
- Ensure you're in Visual Studio Developer Command Prompt
- Check that CMake is installed and in PATH
- Verify submodules are initialized

### Step 3: Create the ZIP Package

Use the provided script to create the winget-compatible ZIP:

**PowerShell (Recommended):**
```powershell
.\installers\create-winget-zip.ps1 -Version "1.0.0"
```

**Batch:**
```batch
installers\create-winget-zip.bat
```

This script will:
- Copy `delta.exe` and `delta-server.exe` to a temp directory
- Copy any required DLL files
- Create `delta-cli-windows-x64.zip` in `installers\packages\`
- Ensure executables are at root level (required for winget)

**Expected output:**
- `installers\packages\delta-cli-windows-x64.zip`

### Step 4: Verify ZIP Structure

Verify the ZIP has the correct structure:

```powershell
# Using the winget repository script (if available)
cd ..\winget-delta-cli
.\scripts\verify-zip-structure.ps1 -FilePath "..\delta\installers\packages\delta-cli-windows-x64.zip"
```

Or manually check:
```powershell
$zipPath = "installers\packages\delta-cli-windows-x64.zip"
$tempDir = "$env:TEMP\delta-check"
Expand-Archive -Path $zipPath -DestinationPath $tempDir -Force
Get-ChildItem -Path $tempDir
Remove-Item -Recurse -Force $tempDir
```

**Expected structure:**
```
delta-cli-windows-x64.zip
├── delta.exe          ← At root level ✓
├── delta-server.exe  ← At root level ✓
└── (any DLLs)
```

### Step 5: Calculate SHA256 Hash

Calculate the SHA256 hash for the winget manifest:

```powershell
.\installers\calculate-sha256.ps1
```

Or specify the file path:
```powershell
.\installers\calculate-sha256.ps1 -FilePath "installers\packages\delta-cli-windows-x64.zip"
```

**Output:** SHA256 hash (64-character hexadecimal string)

**Example:**
```
SHA256 Hash:
a1b2c3d4e5f6g7h8i9j0k1l2m3n4o5p6q7r8s9t0u1v2w3x4y5z6a7b8c9d0e1f2
```

### Step 6: Update Winget Manifest

Update the winget manifest with the SHA256 hash:

1. **Open the installer manifest:**
   ```
   winget-delta-cli\manifests\d\DeltaCLI\DeltaCLI\1.0.0\DeltaCLI.DeltaCLI.1.0.0.installer.yaml
   ```

2. **Replace `PLACEHOLDER_SHA256`** with the actual hash:
   ```yaml
   InstallerSha256: a1b2c3d4e5f6g7h8i9j0k1l2m3n4o5p6q7r8s9t0u1v2w3x4y5z6a7b8c9d0e1f2
   ```

### Step 7: Create GitHub Release

1. **Create a new release** on GitHub:
   - Go to https://github.com/nile-agi/delta/releases
   - Click "Draft a new release"
   - Tag: `v1.0.0` (must match version in manifest)
   - Title: `Delta CLI v1.0.0`
   - Description: Release notes

2. **Upload the ZIP file**:
   - Drag and drop `delta-cli-windows-x64.zip` to the release
   - Or click "Attach binaries" and select the file

3. **Publish the release**

4. **Verify the URL**:
   The ZIP should be accessible at:
   ```
   https://github.com/nile-agi/delta/releases/download/v1.0.0/delta-cli-windows-x64.zip
   ```

### Step 8: Test Installation (Optional)

Test the winget installation locally:

```powershell
# Enable local manifest files
winget settings --enable LocalManifestFiles

# Test installation
cd winget-delta-cli
.\scripts\test-install.ps1 -UninstallFirst
```

## Complete Example

Here's a complete example workflow:

```powershell
# 1. Build
cd C:\path\to\delta
installers\build_windows.bat

# 2. Create ZIP
.\installers\create-winget-zip.ps1 -Version "1.0.0"

# 3. Calculate hash
.\installers\calculate-sha256.ps1
# Copy the hash

# 4. Update manifest (in winget-delta-cli repo)
cd ..\winget-delta-cli
# Edit manifests/d/DeltaCLI/DeltaCLI/1.0.0/DeltaCLI.DeltaCLI.1.0.0.installer.yaml
# Replace PLACEHOLDER_SHA256 with the hash

# 5. Test (optional)
.\scripts\validate-manifest.ps1
.\scripts\test-install.ps1 -UninstallFirst

# 6. Create GitHub release
# Upload installers\packages\delta-cli-windows-x64.zip to release v1.0.0
```

## Troubleshooting

### Build Fails

**Error: "MSVC compiler not found"**
- Run from Visual Studio Developer Command Prompt
- Or set up environment variables manually

**Error: "llama.cpp not found"**
- Initialize submodules: `git submodule update --init --recursive`

**Error: "delta-server.exe not found"**
- Ensure `BUILD_SERVER=ON` in CMake configuration
- Check `build_windows\Release\` directory

### ZIP Creation Fails

**Error: "delta.exe not found"**
- Build the project first: `installers\build_windows.bat`
- Check that build completed successfully

**Error: PowerShell execution policy**
- Run: `Set-ExecutionPolicy -ExecutionPolicy RemoteSigned -Scope CurrentUser`
- Or use the batch script: `installers\create-winget-zip.bat`

### ZIP Structure Incorrect

**Executables in subdirectories**
- Use the provided scripts - they ensure correct structure
- Don't manually create the ZIP

**Missing DLLs**
- Check if DLLs are in `build_windows\Release\`
- The script automatically includes them
- Some DLLs (like Visual C++ Runtime) may need to be installed separately

### SHA256 Calculation Fails

**File not found**
- Ensure ZIP was created successfully
- Check path: `installers\packages\delta-cli-windows-x64.zip`

## ZIP File Requirements Summary

✅ **Required:**
- `delta.exe` at root level
- `delta-server.exe` at root level
- ZIP file named `delta-cli-windows-x64.zip`

✅ **Optional but Recommended:**
- Required DLL files (if redistributable)
- Visual C++ Runtime DLLs (if not system-installed)

❌ **Not Required:**
- Subdirectories for executables
- Installer scripts
- Documentation files (can be in separate package)

## Next Steps

After creating the ZIP:

1. ✅ Update winget manifest with SHA256 hash
2. ✅ Create GitHub release with ZIP file
3. ✅ Test installation locally
4. ✅ Submit to Microsoft winget-pkgs (see CONTRIBUTING.md in winget-delta-cli repo)

## Related Documentation

- `installers\README.md` - Script documentation
- `winget-delta-cli\README.md` - Winget repository documentation
- `winget-delta-cli\CONTRIBUTING.md` - Submission guide
- `winget-delta-cli\REVIEW.md` - Code review checklist

## Scripts Reference

| Script | Purpose | Output |
|--------|---------|--------|
| `build_windows.bat` | Build Delta CLI | `build_windows\Release\*.exe` |
| `create-winget-zip.ps1` | Create ZIP package | `installers\packages\delta-cli-windows-x64.zip` |
| `create-winget-zip.bat` | Create ZIP package (batch) | `installers\packages\delta-cli-windows-x64.zip` |
| `calculate-sha256.ps1` | Calculate hash | SHA256 hash (console output) |

---

**Last Updated:** 2026-02-06
