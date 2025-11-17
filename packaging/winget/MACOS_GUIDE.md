# Submitting Delta CLI to Winget from macOS

This guide helps you submit Delta CLI to the Windows Package Manager (winget) repository when working from macOS.

## Prerequisites

1. **GitHub account** - You'll need to fork the winget-pkgs repository
2. **Git installed** - Should already be on macOS
3. **Windows release package** - `delta-cli-windows-x64.zip` file
4. **GitHub release created** - The Windows ZIP must be uploaded to a GitHub release

## Overview

Since you're on macOS, you have two options:

### Option A: You Have the Windows ZIP File

If you already have the Windows release package (from CI/CD, a Windows machine, or a colleague):

1. **Prepare the manifest** (this script helps with this)
2. **Fork winget-pkgs repository**
3. **Submit the PR**

### Option B: You Need to Build Windows Package

If you don't have the Windows package yet, you need to:

1. **Build on Windows** (or use CI/CD)
2. **Create GitHub release** with the Windows ZIP
3. **Then follow Option A**

## Step-by-Step Process

### Step 1: Get the Windows Release Package

You need `release/delta-cli-windows-x64.zip`. This file should contain:
- `delta.exe`
- `delta-server.exe`
- `webui/` directory (if applicable)
- `README.txt` (optional)

**If you don't have it:**
- Build it on a Windows machine using: `.\packaging\build-scripts\build-windows.ps1 Release x64` and `.\packaging\release\package-windows.ps1 1.0.0 x64`
- Or set up CI/CD to build Windows packages automatically

### Step 2: Prepare the Manifest

Use the preparation script to update the manifest with the correct SHA256 and version:

```bash
cd /Users/suzanodero/io/GITHUB/delta
./packaging/winget/submit-to-winget.sh 1.0.0
```

This script will:
- ✅ Check if the Windows ZIP file exists
- ✅ Calculate SHA256 hash
- ✅ Update the manifest with version and SHA256
- ✅ Check if GitHub release exists
- ✅ Show you what needs to be done next

**What version to use?**
- Use the version you're releasing (e.g., `1.0.0`, `1.1.0`, etc.)
- This should match the GitHub release tag

### Step 3: Create GitHub Release (if not done)

Before submitting to winget, you must have a GitHub release:

1. Go to: https://github.com/nile-agi/delta/releases/new
2. **Tag:** `v1.0.0` (match your version)
3. **Title:** `Delta CLI v1.0.0`
4. **Upload:** `release/delta-cli-windows-x64.zip`
5. **Publish** the release

The release URL should be:
```
https://github.com/nile-agi/delta/releases/download/v1.0.0/delta-cli-windows-x64.zip
```

### Step 4: Fork winget-pkgs Repository

1. Go to: https://github.com/microsoft/winget-pkgs
2. Click the **"Fork"** button (top right)
3. Wait for the fork to complete

### Step 5: Clone Your Fork

```bash
# Clone your fork (replace YOUR_USERNAME with your GitHub username)
cd ~
git clone https://github.com/YOUR_USERNAME/winget-pkgs.git
cd winget-pkgs
```

### Step 6: Create Manifest Directory Structure

Winget requires a specific directory structure:
```
manifests/{first-letter}/{publisher}/{package}/{version}/
```

For Delta CLI:
```bash
# Create the directory structure
mkdir -p manifests/d/DeltaCLI/DeltaCLI/1.0.0
```

### Step 7: Copy the Manifest

```bash
# Copy the manifest from your delta repository
cp /Users/suzanodero/io/GITHUB/delta/packaging/winget/delta-cli.yaml \
   manifests/d/DeltaCLI/DeltaCLI/1.0.0/delta-cli.yaml
```

### Step 8: Commit and Push

```bash
# Stage the changes
git add manifests/d/DeltaCLI/DeltaCLI/1.0.0/

# Commit
git commit -m "Add DeltaCLI.DeltaCLI version 1.0.0"

# Push to your fork
git push origin main
```

### Step 9: Create Pull Request

1. Go to: https://github.com/YOUR_USERNAME/winget-pkgs
2. You should see a banner suggesting to create a PR
3. Click **"Compare & pull request"**
4. Fill in the PR description:

```markdown
## Package Request

- Package: DeltaCLI.DeltaCLI
- Version: 1.0.0
- Description: Offline AI Assistant powered by llama.cpp

## Checklist
- [x] Manifest validated (prepared on macOS)
- [x] GitHub release created: https://github.com/nile-agi/delta/releases/tag/v1.0.0
- [x] SHA256 hash verified
- [x] Installer URL verified
```

5. **Submit** the PR

### Step 10: Wait for Review

- Winget maintainers will review your PR
- Typical review time: 1-3 business days
- They may request changes or ask questions
- Once approved and merged, the package will be available via:
  ```powershell
  winget install DeltaCLI.DeltaCLI
  ```

## Quick Reference Commands

```bash
# 1. Prepare manifest (from delta repository)
cd /Users/suzanodero/io/GITHUB/delta
./packaging/winget/submit-to-winget.sh 1.0.0

# 2. Fork winget-pkgs on GitHub (do this in browser)

# 3. Clone your fork
cd ~
git clone https://github.com/YOUR_USERNAME/winget-pkgs.git
cd winget-pkgs

# 4. Create directory and copy manifest
mkdir -p manifests/d/DeltaCLI/DeltaCLI/1.0.0
cp /Users/suzanodero/io/GITHUB/delta/packaging/winget/delta-cli.yaml \
   manifests/d/DeltaCLI/DeltaCLI/1.0.0/delta-cli.yaml

# 5. Commit and push
git add manifests/d/DeltaCLI/DeltaCLI/1.0.0/
git commit -m "Add DeltaCLI.DeltaCLI version 1.0.0"
git push origin main

# 6. Create PR on GitHub (do this in browser)
```

## Troubleshooting

### "Release package not found"

**Problem:** The script can't find `release/delta-cli-windows-x64.zip`

**Solution:**
- Make sure you have the Windows ZIP file
- Place it in the `release/` directory
- Or provide the full path to the script

### "GitHub release not found"

**Problem:** The release doesn't exist yet

**Solution:**
1. Create the GitHub release first
2. Upload the Windows ZIP file
3. Publish the release
4. Run the preparation script again

### "Manifest validation errors"

**Problem:** The manifest has errors

**Solution:**
- Check that SHA256 is 64 hex characters
- Verify InstallerUrl points to actual release
- Ensure PackageVersion follows semantic versioning (e.g., `1.0.0`)
- Check all required fields are present

**Note:** You can't run `winget validate` on macOS, but the script will check the format. For full validation, you'll need Windows or wait for the PR review.

### Git Push Fails

**Problem:** Can't push to fork

**Solution:**
1. Check your git credentials are configured
2. Verify you have write access to your fork
3. Try pushing manually: `git push origin main`
4. If using SSH, make sure your SSH key is added to GitHub

## Updating to New Version

When releasing a new version:

1. **Prepare new manifest:**
   ```bash
   ./packaging/winget/submit-to-winget.sh 1.1.0
   ```

2. **Create new directory:**
   ```bash
   cd ~/winget-pkgs
   mkdir -p manifests/d/DeltaCLI/DeltaCLI/1.1.0
   cp /Users/suzanodero/io/GITHUB/delta/packaging/winget/delta-cli.yaml \
      manifests/d/DeltaCLI/DeltaCLI/1.1.0/delta-cli.yaml
   ```

3. **Commit and push:**
   ```bash
   git add manifests/d/DeltaCLI/DeltaCLI/1.1.0/
   git commit -m "Add DeltaCLI.DeltaCLI version 1.1.0"
   git push origin main
   ```

4. **Create new PR**

## Alternative: Use Windows Machine or CI/CD

If you have access to a Windows machine or CI/CD:

1. **On Windows:** Use the PowerShell script:
   ```powershell
   .\packaging\winget\submit-to-winget.ps1 -Version 1.0.0 -GitHubUsername YOUR_USERNAME
   ```
   This automates more of the process including validation.

2. **With CI/CD:** Set up GitHub Actions to:
   - Build Windows package
   - Create GitHub release
   - Automatically submit to winget-pkgs

## Resources

- [Winget-Pkgs Repository](https://github.com/microsoft/winget-pkgs)
- [Winget Manifest Documentation](https://learn.microsoft.com/en-us/windows/package-manager/package/manifest)
- [Winget Submission Guidelines](https://github.com/microsoft/winget-pkgs/blob/master/policies/PULL_REQUEST_TEMPLATE.md)

## Need Help?

- Check the main submission guide: [SUBMIT.md](SUBMIT.md)
- Check the automation guide: [AUTOMATION.md](AUTOMATION.md)
- Open an issue on the Delta CLI repository

