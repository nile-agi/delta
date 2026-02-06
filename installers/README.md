# Windows Installer Scripts

This directory contains scripts for building and packaging Delta CLI for Windows.

## Scripts

### `build_windows.bat`
Builds Delta CLI for Windows (x64).

**Usage:**
```batch
installers\build_windows.bat
```

**Requirements:**
- Visual Studio 2019+ or Build Tools
- CMake
- Run from Visual Studio Developer Command Prompt

**Output:**
- `build_windows\Release\delta.exe`
- `build_windows\Release\delta-server.exe`

### `create-winget-zip.ps1` / `create-winget-zip.bat`
Creates the Windows ZIP package for winget installation.

**Usage (PowerShell):**
```powershell
.\installers\create-winget-zip.ps1 [-Version "1.0.0"] [-BuildDir "build_windows\Release"]
```

**Usage (Batch):**
```batch
installers\create-winget-zip.bat
```

**Requirements:**
- Built executables in `build_windows\Release\`
- PowerShell 5.1+ (for .ps1 script)

**Output:**
- `installers\packages\delta-cli-windows-x64.zip`

**ZIP Structure:**
```
delta-cli-windows-x64.zip
├── delta.exe          # At root level (required for winget)
├── delta-server.exe  # At root level (required for winget)
└── *.dll             # Any required DLL files
```

### `calculate-sha256.ps1`
Calculates the SHA256 hash of the ZIP file for winget manifest.

**Usage:**
```powershell
.\installers\calculate-sha256.ps1 [-FilePath "path\to\delta-cli-windows-x64.zip"]
```

If no file path is provided, it uses the default location: `installers\packages\delta-cli-windows-x64.zip`

**Output:**
- SHA256 hash (copy to winget manifest)

### `package_windows.bat`
Creates an NSIS installer (legacy, for non-winget installations).

**Usage:**
```batch
installers\package_windows.bat
```

**Requirements:**
- NSIS (Nullsoft Scriptable Install System)
- If NSIS is not found, creates a portable ZIP instead

## Complete Workflow

### 1. Build Delta CLI

```batch
installers\build_windows.bat
```

### 2. Create Winget ZIP Package

```powershell
.\installers\create-winget-zip.ps1 -Version "1.0.0"
```

### 3. Calculate SHA256 Hash

```powershell
.\installers\calculate-sha256.ps1
```

### 4. Update Winget Manifest

Update `packaging\winget\delta-cli.yaml` or the winget repository manifest with the SHA256 hash.

### 5. Create GitHub Release

1. Create a new release tag: `v1.0.0`
2. Upload `delta-cli-windows-x64.zip` to the release
3. The ZIP file should be accessible at:
   `https://github.com/nile-agi/delta/releases/download/v1.0.0/delta-cli-windows-x64.zip`

## ZIP File Requirements for Winget

The ZIP file must have the following structure:

```
delta-cli-windows-x64.zip
├── delta.exe          # Main executable (at root)
├── delta-server.exe  # Server executable (at root)
└── (any required DLLs)
```

**Important:**
- Executables must be at the **root level** of the ZIP (not in subdirectories)
- The `Commands` field in the winget manifest tells winget which executables to add to PATH
- DLLs should be in the same directory as the executables

## Troubleshooting

### "delta-server.exe not found"

Ensure `BUILD_SERVER=ON` when building:
```batch
cmake .. -DBUILD_SERVER=ON
```

### "DLL files missing"

Some DLLs may be required at runtime:
- Visual C++ Runtime (usually installed on Windows)
- CUDA DLLs (if CUDA support is enabled)

These can be:
1. Included in the ZIP (if redistributable)
2. Listed as dependencies in the winget manifest
3. Documented for users to install separately

### ZIP structure incorrect

Use the provided scripts to ensure correct structure. The scripts automatically place executables at the root level.

## Related Files

- `packaging\winget\delta-cli.yaml` - Winget manifest (in delta repo)
- `winget-delta-cli\manifests\` - Winget repository manifests
