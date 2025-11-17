# Winget Submission Checklist

Use this checklist to ensure you have everything ready before submitting to winget.

## Pre-Submission Checklist

### 1. Windows Release Package
- [ ] `release/delta-cli-windows-x64.zip` exists
- [ ] ZIP contains `delta.exe` and `delta-server.exe`
- [ ] ZIP contains `webui/` directory (if applicable)
- [ ] ZIP is properly structured

### 2. GitHub Release
- [ ] GitHub release created at: https://github.com/nile-agi/delta/releases
- [ ] Release tag matches version (e.g., `v1.0.0`)
- [ ] Windows ZIP file uploaded to release
- [ ] Release is published (not draft)
- [ ] Release URL accessible: `https://github.com/nile-agi/delta/releases/download/v1.0.0/delta-cli-windows-x64.zip`

### 3. Manifest Preparation
- [ ] Manifest updated with correct version
- [ ] SHA256 hash calculated and updated
- [ ] InstallerUrl points to correct GitHub release
- [ ] All required fields present in manifest

**To prepare manifest:**
```bash
./packaging/winget/submit-to-winget.sh 1.0.0
```

### 4. Repository Fork
- [ ] Forked https://github.com/microsoft/winget-pkgs
- [ ] Fork is up to date with upstream

### 5. Git Setup
- [ ] Git configured with your name and email
- [ ] GitHub credentials configured (SSH key or token)
- [ ] Can push to your fork

## Submission Steps

### Step 1: Prepare Manifest ✅
```bash
cd /Users/suzanodero/io/GITHUB/delta
./packaging/winget/submit-to-winget.sh 1.0.0
```

### Step 2: Fork Repository ✅
- [ ] Forked winget-pkgs on GitHub

### Step 3: Clone Fork
```bash
cd ~
git clone https://github.com/YOUR_USERNAME/winget-pkgs.git
cd winget-pkgs
```

### Step 4: Create Directory Structure
```bash
mkdir -p manifests/d/DeltaCLI/DeltaCLI/1.0.0
```

### Step 5: Copy Manifest
```bash
cp /Users/suzanodero/io/GITHUB/delta/packaging/winget/delta-cli.yaml \
   manifests/d/DeltaCLI/DeltaCLI/1.0.0/delta-cli.yaml
```

### Step 6: Commit and Push
```bash
git add manifests/d/DeltaCLI/DeltaCLI/1.0.0/
git commit -m "Add DeltaCLI.DeltaCLI version 1.0.0"
git push origin main
```

### Step 7: Create Pull Request
- [ ] Go to: https://github.com/YOUR_USERNAME/winget-pkgs
- [ ] Click "Compare & pull request"
- [ ] Fill in PR description
- [ ] Submit PR

## PR Description Template

```markdown
## Package Request

- Package: DeltaCLI.DeltaCLI
- Version: 1.0.0
- Description: Offline AI Assistant powered by llama.cpp

## Checklist
- [x] Manifest prepared and validated
- [x] GitHub release created: https://github.com/nile-agi/delta/releases/tag/v1.0.0
- [x] SHA256 hash verified
- [x] Installer URL verified
- [x] All required fields present

## Package Details

Delta CLI is an open-source, offline-first AI assistant that runs large language models (LLMs) directly on your device. Built on top of llama.cpp, Delta CLI provides a simple command-line interface to interact with AI models without requiring internet connectivity or cloud services.

**Features:**
- 100% Offline operation
- GPU acceleration support
- Cross-platform compatibility
- Easy model management
```

## After Submission

### Wait for Review
- [ ] PR submitted
- [ ] Monitoring PR for comments
- [ ] Ready to respond to feedback

### Typical Timeline
- Review: 1-3 business days
- Merge: After approval
- Indexing: 24-48 hours after merge
- Available: After indexing

### Testing After Merge

Once the PR is merged and indexed (wait 24-48 hours):

```powershell
# Update winget sources
winget source update

# Test installation
winget install DeltaCLI.DeltaCLI

# Verify installation
delta --version
```

## Troubleshooting

### PR Gets Rejected

Common reasons:
- Invalid SHA256 hash
- URL doesn't exist or is incorrect
- Manifest validation errors
- Missing required fields

**Solution:** Fix the issues and submit a new commit to the PR.

### Package Not Found After Merge

1. Wait 24-48 hours for indexing
2. Update winget sources: `winget source update`
3. Try installing again

### Need to Update Version

Follow the same process but:
- Use new version number
- Create new directory: `manifests/d/DeltaCLI/DeltaCLI/1.1.0/`
- Submit new PR

## Resources

- **macOS Guide:** [MACOS_GUIDE.md](MACOS_GUIDE.md)
- **Quick Start:** [QUICK_START.md](QUICK_START.md)
- **Detailed Guide:** [SUBMIT.md](SUBMIT.md)
- **Automation Guide:** [AUTOMATION.md](AUTOMATION.md)
- **Winget-Pkgs Repo:** https://github.com/microsoft/winget-pkgs

