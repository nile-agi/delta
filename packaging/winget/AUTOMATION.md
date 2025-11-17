# Automated Winget Submission

This guide explains how to use the automated submission scripts to submit Delta CLI to the winget repository.

## Quick Start

### Prerequisites

1. **Windows release package created:**
   ```powershell
   .\packaging\build-scripts\build-windows.ps1 Release x64
   .\packaging\release\package-windows.ps1 1.0.0 x64
   ```

2. **GitHub release created:**
   - Go to https://github.com/nile-agi/delta/releases/new
   - Upload `delta-cli-windows-x64.zip`
   - Tag: `v1.0.0`
   - Publish release

3. **Fork winget-pkgs repository:**
   - Go to https://github.com/microsoft/winget-pkgs
   - Click "Fork"

### Automated Submission (Windows)

```powershell
# Basic usage
.\packaging\winget\submit-to-winget.ps1 -Version 1.0.0 -GitHubUsername YOUR_USERNAME

# Dry run (see what would happen without making changes)
.\packaging\winget\submit-to-winget.ps1 -Version 1.0.0 -DryRun

# Skip validation (if you're sure the manifest is correct)
.\packaging\winget\submit-to-winget.ps1 -Version 1.0.0 -GitHubUsername YOUR_USERNAME -SkipValidation
```

### What the Script Does

1. **Checks Prerequisites:**
   - Verifies winget is installed
   - Verifies git is installed
   - Checks for GitHub CLI (optional)

2. **Validates Release Package:**
   - Checks if `release/delta-cli-windows-x64.zip` exists
   - Calculates SHA256 hash

3. **Updates Manifest:**
   - Updates `PackageVersion`
   - Updates `InstallerSha256`
   - Updates `InstallerUrl`

4. **Validates Manifest:**
   - Runs `winget validate` on the manifest

5. **Checks GitHub Release:**
   - Verifies the release URL is accessible

6. **Prepares Submission:**
   - Clones your fork of winget-pkgs (if needed)
   - Creates manifest directory structure
   - Copies manifest to correct location
   - Validates in winget-pkgs structure

7. **Git Operations:**
   - Stages changes
   - Creates commit (with your approval)
   - Pushes to your fork (with your approval)

8. **Final Steps:**
   - Provides PR creation instructions
   - Optionally uses GitHub CLI to create PR

### Cross-Platform Preparation (macOS/Linux)

If you're on macOS or Linux, you can prepare the manifest:

```bash
# Prepare manifest (updates SHA256, version, URL)
./packaging/winget/submit-to-winget.sh 1.0.0
```

Then on Windows, run the PowerShell script to complete the submission.

## Step-by-Step Example

### 1. Build and Package

```powershell
# Build Delta CLI
.\packaging\build-scripts\build-windows.ps1 Release x64

# Create release package
.\packaging\release\package-windows.ps1 1.0.0 x64
```

Output:
```
✅ Package created: release/delta-cli-windows-x64.zip
📋 SHA256: abc123def456...
```

### 2. Create GitHub Release

1. Go to https://github.com/nile-agi/delta/releases/new
2. Tag: `v1.0.0`
3. Title: `Delta CLI v1.0.0`
4. Upload: `release/delta-cli-windows-x64.zip`
5. Publish

### 3. Fork winget-pkgs

1. Go to https://github.com/microsoft/winget-pkgs
2. Click "Fork"
3. Wait for fork to complete

### 4. Run Automated Script

```powershell
.\packaging\winget\submit-to-winget.ps1 -Version 1.0.0 -GitHubUsername YOUR_USERNAME
```

The script will:
- ✅ Check prerequisites
- ✅ Calculate SHA256
- ✅ Update manifest
- ✅ Validate manifest
- ✅ Check GitHub release
- ✅ Clone your fork
- ✅ Create directory structure
- ✅ Copy manifest
- ✅ Validate in winget-pkgs
- ✅ Stage changes
- ✅ Ask to create commit
- ✅ Ask to push to fork

### 5. Create Pull Request

After the script completes, you'll see:

```
🎉 Success! Next step:
   Create a Pull Request at:
   https://github.com/YOUR_USERNAME/winget-pkgs/compare/main...main
```

**Option A: Using GitHub CLI (if installed)**
```powershell
cd ..\winget-pkgs
gh pr create --title "Add DeltaCLI.DeltaCLI version 1.0.0" --body "Adds Delta CLI package to winget repository"
```

**Option B: Using GitHub Web Interface**
1. Go to https://github.com/YOUR_USERNAME/winget-pkgs
2. You should see a banner suggesting to create a PR
3. Click "Compare & pull request"
4. Fill in PR description:
   ```
   ## Package Request
   
   - Package: DeltaCLI.DeltaCLI
   - Version: 1.0.0
   - Description: Offline AI Assistant powered by llama.cpp
   
   ## Checklist
   - [x] Manifest validated with `winget validate`
   - [x] GitHub release created
   - [x] SHA256 hash verified
   - [x] Installer URL verified
   ```
5. Submit PR

### 6. Wait for Review

- Winget maintainers will review your PR
- Typical review time: 1-3 business days
- Once merged, the package will be available via `winget install DeltaCLI.DeltaCLI`

## Troubleshooting

### Script Fails at "Cloning Repository"

**Problem:** Script can't clone your fork

**Solution:**
1. Make sure you've forked https://github.com/microsoft/winget-pkgs
2. Verify your GitHub username is correct
3. Check that you have access to the repository

### Script Fails at "GitHub Release Not Found"

**Problem:** The release doesn't exist yet

**Solution:**
1. Create the GitHub release first
2. Upload the Windows ZIP file
3. Publish the release
4. Run the script again

### Manifest Validation Fails

**Problem:** `winget validate` reports errors

**Solution:**
1. Check that SHA256 hash is correct (64 hex characters)
2. Verify InstallerUrl points to actual release
3. Ensure PackageVersion follows semantic versioning
4. Check all required fields are present

### Git Push Fails

**Problem:** Can't push to fork

**Solution:**
1. Check your git credentials are configured
2. Verify you have write access to your fork
3. Try pushing manually: `git push origin main`

## Advanced Usage

### Custom Version

```powershell
.\packaging\winget\submit-to-winget.ps1 -Version 1.1.0 -GitHubUsername YOUR_USERNAME
```

### Skip Validation

If you're sure the manifest is correct:

```powershell
.\packaging\winget\submit-to-winget.ps1 -Version 1.0.0 -GitHubUsername YOUR_USERNAME -SkipValidation
```

### Dry Run

See what would happen without making changes:

```powershell
.\packaging\winget\submit-to-winget.ps1 -Version 1.0.0 -DryRun
```

## Manual Override

If the automated script doesn't work for you, see [SUBMIT.md](SUBMIT.md) for manual submission steps.

## Next Steps After Submission

1. **Monitor PR:** Watch for comments or requested changes
2. **Respond to Feedback:** Address any issues raised by maintainers
3. **Wait for Merge:** PR will be merged once approved
4. **Test Installation:** After merge, wait 24-48 hours, then test:
   ```powershell
   winget source update
   winget install DeltaCLI.DeltaCLI
   ```

## Updating to New Version

For version updates, follow the same process:

```powershell
# Build new version
.\packaging\release\package-windows.ps1 1.1.0 x64

# Create GitHub release with new version

# Submit to winget
.\packaging\winget\submit-to-winget.ps1 -Version 1.1.0 -GitHubUsername YOUR_USERNAME
```

The script will create a new directory: `manifests\d\DeltaCLI\DeltaCLI\1.1.0\`

