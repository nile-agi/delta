# Quick Start: Submit Delta CLI to Winget

## Prerequisites Checklist

Before starting, make sure you have:

- [ ] Windows release package: `release/delta-cli-windows-x64.zip`
- [ ] GitHub account (to fork winget-pkgs)
- [ ] Git installed
- [ ] GitHub release created with the Windows ZIP file

## Quick Steps

### Step 1: Prepare the Manifest (macOS/Linux)

```bash
cd /Users/suzanodero/io/GITHUB/delta
./packaging/winget/submit-to-winget.sh 1.0.0
```

This will:
- ✅ Calculate SHA256 hash
- ✅ Update manifest with version and hash
- ✅ Check if GitHub release exists
- ✅ Show you next steps

### Step 2: Fork winget-pkgs

1. Go to: https://github.com/microsoft/winget-pkgs
2. Click **"Fork"** button

### Step 3: Clone and Submit

```bash
# Clone your fork (replace YOUR_USERNAME)
cd ~
git clone https://github.com/YOUR_USERNAME/winget-pkgs.git
cd winget-pkgs

# Create directory structure
mkdir -p manifests/d/DeltaCLI/DeltaCLI/1.0.0

# Copy manifest
cp /Users/suzanodero/io/GITHUB/delta/packaging/winget/delta-cli.yaml \
   manifests/d/DeltaCLI/DeltaCLI/1.0.0/delta-cli.yaml

# Commit and push
git add manifests/d/DeltaCLI/DeltaCLI/1.0.0/
git commit -m "Add DeltaCLI.DeltaCLI version 1.0.0"
git push origin main
```

### Step 4: Create Pull Request

1. Go to: https://github.com/YOUR_USERNAME/winget-pkgs
2. Click **"Compare & pull request"**
3. Fill in PR description and submit

## That's It! 🎉

Wait 1-3 business days for review. Once merged, users can install with:

```powershell
winget install DeltaCLI.DeltaCLI
```

## Need More Help?

- **macOS users:** See [MACOS_GUIDE.md](MACOS_GUIDE.md)
- **Windows users:** See [AUTOMATION.md](AUTOMATION.md) or [SUBMIT.md](SUBMIT.md)
- **General info:** See [README.md](README.md)

