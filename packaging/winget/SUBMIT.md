# How to Submit Delta CLI to Winget

This guide walks you through submitting the Delta CLI package to the Windows Package Manager (winget) repository.

## Prerequisites

1. A GitHub account
2. Git installed on your system
3. Winget CLI installed (comes with Windows 10/11)
4. A built Windows release package with SHA256 hash

## Step-by-Step Submission Process

### Step 1: Prepare the Release

1. **Build and package Windows release:**
   ```powershell
   # Build Delta CLI
   .\packaging\build-scripts\build-windows.ps1 Release x64
   
   # Create release package
   .\packaging\release\package-windows.ps1 1.0.0 x64
   ```

2. **Update manifest with SHA256:**
   ```powershell
   cd packaging\winget
   .\update-sha256.ps1 ..\..\release\delta-cli-windows-x64.zip 1.0.0
   ```

3. **Validate the manifest:**
   ```powershell
   winget validate delta-cli.yaml
   ```

   If validation fails, fix any errors before proceeding.

### Step 2: Fork and Clone winget-pkgs

1. **Fork the repository:**
   - Go to https://github.com/microsoft/winget-pkgs
   - Click the "Fork" button in the top right
   - Wait for the fork to complete

2. **Clone your fork:**
   ```powershell
   git clone https://github.com/YOUR_USERNAME/winget-pkgs.git
   cd winget-pkgs
   ```

### Step 3: Create Manifest Structure

1. **Create the directory structure:**
   ```powershell
   # The structure is: manifests/{first-letter}/{publisher}/{package}/{version}/
   mkdir -p manifests\d\DeltaCLI\DeltaCLI\1.0.0
   ```

2. **Copy the manifest:**
   ```powershell
   copy ..\..\delta\packaging\winget\delta-cli.yaml manifests\d\DeltaCLI\DeltaCLI\1.0.0\delta-cli.yaml
   ```

   Or if you're in the delta repository:
   ```powershell
   copy packaging\winget\delta-cli.yaml ..\winget-pkgs\manifests\d\DeltaCLI\DeltaCLI\1.0.0\delta-cli.yaml
   ```

### Step 4: Validate in winget-pkgs Structure

1. **Navigate to winget-pkgs:**
   ```powershell
   cd ..\winget-pkgs
   ```

2. **Validate the manifest:**
   ```powershell
   winget validate manifests\d\DeltaCLI\DeltaCLI\1.0.0\delta-cli.yaml
   ```

   This should pass if your manifest is correct.

### Step 5: Create GitHub Release

Before submitting to winget, ensure you have a GitHub release:

1. **Create a GitHub release:**
   - Go to https://github.com/nile-agi/delta/releases
   - Click "Create a new release"
   - Tag: `v1.0.0`
   - Title: `Delta CLI v1.0.0`
   - Upload `delta-cli-windows-x64.zip`
   - Publish the release

2. **Verify the release URL matches the manifest:**
   The `InstallerUrl` in the manifest should match:
   ```
   https://github.com/nile-agi/delta/releases/download/v1.0.0/delta-cli-windows-x64.zip
   ```

### Step 6: Submit Pull Request

1. **Stage the changes:**
   ```powershell
   cd winget-pkgs
   git add manifests\d\DeltaCLI\DeltaCLI\1.0.0\
   ```

2. **Commit:**
   ```powershell
   git commit -m "Add DeltaCLI.DeltaCLI version 1.0.0"
   ```

3. **Push to your fork:**
   ```powershell
   git push origin main
   ```

4. **Create Pull Request:**
   - Go to https://github.com/YOUR_USERNAME/winget-pkgs
   - You should see a banner suggesting to create a PR
   - Click "Compare & pull request"
   - Fill in the PR description:
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
   - Submit the PR

### Step 7: Wait for Review

- Winget maintainers will review your PR
- They may request changes or ask questions
- Once approved, the PR will be merged
- The package will be indexed and available via `winget install DeltaCLI.DeltaCLI`

**Typical review time:** 1-3 business days

## Troubleshooting

### PR Gets Rejected

Common reasons:
- **Invalid SHA256**: Ensure the hash matches the actual file
- **URL doesn't exist**: Verify the GitHub release exists and URL is correct
- **Manifest validation errors**: Run `winget validate` and fix all errors
- **Missing required fields**: Check the manifest has all required fields

### Package Not Found After Merge

After the PR is merged:
1. Wait 24-48 hours for indexing
2. Update winget sources: `winget source update`
3. Try installing: `winget install DeltaCLI.DeltaCLI`

### Updating to New Version

For version updates:
1. Follow the same process
2. Create a new directory: `manifests\d\DeltaCLI\DeltaCLI\1.1.0\`
3. Copy the updated manifest
4. Submit a new PR

## Resources

- [Winget-Pkgs Repository](https://github.com/microsoft/winget-pkgs)
- [Winget Manifest Documentation](https://learn.microsoft.com/en-us/windows/package-manager/package/manifest)
- [Winget Validation Tool](https://github.com/microsoft/winget-cli)
- [Winget-Pkgs Submission Guidelines](https://github.com/microsoft/winget-pkgs/blob/master/policies/PULL_REQUEST_TEMPLATE.md)

