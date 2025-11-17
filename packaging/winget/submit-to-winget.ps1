# Automated Winget Submission Script for Delta CLI
# This script automates as much of the winget submission process as possible

param(
    [string]$Version = "1.0.0",
    [string]$GitHubUsername = "",
    [switch]$SkipValidation,
    [switch]$DryRun
)

$ErrorActionPreference = "Stop"

Write-Host "╔══════════════════════════════════════════════════════════════╗" -ForegroundColor Cyan
Write-Host "║     Delta CLI - Automated Winget Submission                ║" -ForegroundColor Cyan
Write-Host "╚══════════════════════════════════════════════════════════════╝" -ForegroundColor Cyan
Write-Host ""

# Get script directory
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$ProjectRoot = Split-Path -Parent (Split-Path -Parent $ScriptDir)
$ManifestPath = Join-Path $ScriptDir "delta-cli.yaml"
$ReleaseZip = Join-Path (Join-Path $ProjectRoot "release") "delta-cli-windows-x64.zip"

# Step 1: Check prerequisites
Write-Host "📋 Step 1: Checking prerequisites..." -ForegroundColor Cyan
Write-Host ""

# Check if winget is available
try {
    $wingetVersion = winget --version 2>&1
    Write-Host "✅ Winget found: $wingetVersion" -ForegroundColor Green
} catch {
    Write-Host "❌ Winget not found. Please install App Installer from Microsoft Store." -ForegroundColor Red
    exit 1
}

# Check if git is available
try {
    $gitVersion = git --version 2>&1
    Write-Host "✅ Git found: $gitVersion" -ForegroundColor Green
} catch {
    Write-Host "❌ Git not found. Please install Git." -ForegroundColor Red
    exit 1
}

# Check if GitHub CLI is available (optional but helpful)
$hasGhCli = $false
try {
    $ghVersion = gh --version 2>&1
    Write-Host "✅ GitHub CLI found: $ghVersion" -ForegroundColor Green
    $hasGhCli = $true
} catch {
    Write-Host "⚠️  GitHub CLI not found (optional, but recommended)" -ForegroundColor Yellow
    Write-Host "   Install from: https://cli.github.com/" -ForegroundColor Gray
}

Write-Host ""

# Step 2: Check if release ZIP exists
Write-Host "📦 Step 2: Checking release package..." -ForegroundColor Cyan
Write-Host ""

if (-not (Test-Path $ReleaseZip)) {
    Write-Host "❌ Release package not found: $ReleaseZip" -ForegroundColor Red
    Write-Host "" -ForegroundColor Red
    Write-Host "💡 To create the release package, run:" -ForegroundColor Yellow
    Write-Host "   .\packaging\build-scripts\build-windows.ps1 Release x64" -ForegroundColor White
    Write-Host "   .\packaging\release\package-windows.ps1 $Version x64" -ForegroundColor White
    Write-Host ""
    exit 1
}

Write-Host "✅ Release package found: $ReleaseZip" -ForegroundColor Green

# Calculate SHA256
Write-Host "📋 Calculating SHA256 hash..." -ForegroundColor Cyan
$sha256 = (Get-FileHash $ReleaseZip -Algorithm SHA256).Hash.ToLower()
Write-Host "✅ SHA256: $sha256" -ForegroundColor Green
Write-Host ""

# Step 3: Update manifest
Write-Host "📝 Step 3: Updating manifest..." -ForegroundColor Cyan
Write-Host ""

if (-not (Test-Path $ManifestPath)) {
    Write-Host "❌ Manifest not found: $ManifestPath" -ForegroundColor Red
    exit 1
}

# Read and update manifest
$manifestContent = Get-Content $ManifestPath -Raw

# Update version
$manifestContent = $manifestContent -replace "PackageVersion: \d+\.\d+\.\d+", "PackageVersion: $Version"

# Update SHA256
$manifestContent = $manifestContent -replace "InstallerSha256: [0-9a-f]{64}", "InstallerSha256: $sha256"

# Update URL
$manifestContent = $manifestContent -replace "releases/download/v\d+\.\d+\.\d+", "releases/download/v$Version"

# Write back
$manifestContent | Set-Content $ManifestPath -NoNewline

Write-Host "✅ Manifest updated with:" -ForegroundColor Green
Write-Host "   Version: $Version" -ForegroundColor Gray
Write-Host "   SHA256: $sha256" -ForegroundColor Gray
Write-Host "   URL: https://github.com/nile-agi/delta/releases/download/v$Version/delta-cli-windows-x64.zip" -ForegroundColor Gray
Write-Host ""

# Step 4: Validate manifest
if (-not $SkipValidation) {
    Write-Host "✅ Step 4: Validating manifest..." -ForegroundColor Cyan
    Write-Host ""
    
    try {
        $validationResult = winget validate $ManifestPath 2>&1
        if ($LASTEXITCODE -eq 0) {
            Write-Host "✅ Manifest validation passed!" -ForegroundColor Green
        } else {
            Write-Host "❌ Manifest validation failed:" -ForegroundColor Red
            Write-Host $validationResult -ForegroundColor Red
            Write-Host ""
            Write-Host "Please fix the errors above before submitting." -ForegroundColor Yellow
            exit 1
        }
    } catch {
        Write-Host "⚠️  Could not validate manifest (winget validate may not be available)" -ForegroundColor Yellow
        Write-Host "   Continuing anyway..." -ForegroundColor Gray
    }
    Write-Host ""
}

# Step 5: Check GitHub release
Write-Host "🔍 Step 5: Checking GitHub release..." -ForegroundColor Cyan
Write-Host ""

$releaseUrl = "https://github.com/nile-agi/delta/releases/download/v$Version/delta-cli-windows-x64.zip"
$releaseExists = $false

try {
    $response = Invoke-WebRequest -Uri $releaseUrl -Method Head -ErrorAction SilentlyContinue
    if ($response.StatusCode -eq 200) {
        $releaseExists = $true
        Write-Host "✅ GitHub release found: $releaseUrl" -ForegroundColor Green
    }
} catch {
    Write-Host "⚠️  GitHub release not found: $releaseUrl" -ForegroundColor Yellow
    Write-Host "   The release must exist before submitting to winget." -ForegroundColor Yellow
    Write-Host ""
    Write-Host "💡 To create the release:" -ForegroundColor Cyan
    Write-Host "   1. Go to: https://github.com/nile-agi/delta/releases/new" -ForegroundColor White
    Write-Host "   2. Tag: v$Version" -ForegroundColor White
    Write-Host "   3. Title: Delta CLI v$Version" -ForegroundColor White
    Write-Host "   4. Upload: $ReleaseZip" -ForegroundColor White
    Write-Host "   5. Publish the release" -ForegroundColor White
    Write-Host ""
    
    if (-not $DryRun) {
        $continue = Read-Host "Continue anyway? (y/n)"
        if ($continue -ne "y" -and $continue -ne "Y") {
            exit 0
        }
    }
}
Write-Host ""

# Step 6: Prepare winget-pkgs submission
Write-Host "📦 Step 6: Preparing winget-pkgs submission..." -ForegroundColor Cyan
Write-Host ""

$wingetPkgsDir = Join-Path (Split-Path -Parent $ProjectRoot) "winget-pkgs"
$manifestDir = Join-Path $wingetPkgsDir "manifests\d\DeltaCLI\DeltaCLI\$Version"

if ($DryRun) {
    Write-Host "🔍 DRY RUN MODE - Would create:" -ForegroundColor Yellow
    Write-Host "   Directory: $manifestDir" -ForegroundColor Gray
    Write-Host "   Manifest: $manifestDir\delta-cli.yaml" -ForegroundColor Gray
    Write-Host ""
    Write-Host "Next steps (manual):" -ForegroundColor Cyan
    Write-Host "   1. Fork https://github.com/microsoft/winget-pkgs" -ForegroundColor White
    Write-Host "   2. Clone your fork" -ForegroundColor White
    Write-Host "   3. Create directory: manifests\d\DeltaCLI\DeltaCLI\$Version\" -ForegroundColor White
    Write-Host "   4. Copy $ManifestPath to that directory" -ForegroundColor White
    Write-Host "   5. Commit and push" -ForegroundColor White
    Write-Host "   6. Create PR on GitHub" -ForegroundColor White
    exit 0
}

# Check if winget-pkgs directory exists
if (-not (Test-Path $wingetPkgsDir)) {
    Write-Host "📥 Cloning winget-pkgs repository..." -ForegroundColor Cyan
    
    if ([string]::IsNullOrEmpty($GitHubUsername)) {
        $GitHubUsername = Read-Host "Enter your GitHub username (to clone your fork)"
    }
    
    if ([string]::IsNullOrEmpty($GitHubUsername)) {
        Write-Host "❌ GitHub username required to clone fork" -ForegroundColor Red
        Write-Host ""
        Write-Host "💡 First, fork the repository:" -ForegroundColor Yellow
        Write-Host "   1. Go to: https://github.com/microsoft/winget-pkgs" -ForegroundColor White
        Write-Host "   2. Click 'Fork' button" -ForegroundColor White
        Write-Host "   3. Then run this script again with: -GitHubUsername YOUR_USERNAME" -ForegroundColor White
        exit 1
    }
    
    $forkUrl = "https://github.com/$GitHubUsername/winget-pkgs.git"
    Write-Host "   Cloning from: $forkUrl" -ForegroundColor Gray
    
    try {
        git clone $forkUrl $wingetPkgsDir
        Write-Host "✅ Repository cloned" -ForegroundColor Green
    } catch {
        Write-Host "❌ Failed to clone repository: $_" -ForegroundColor Red
        Write-Host ""
        Write-Host "💡 Make sure you have:" -ForegroundColor Yellow
        Write-Host "   1. Forked https://github.com/microsoft/winget-pkgs" -ForegroundColor White
        Write-Host "   2. Entered the correct GitHub username" -ForegroundColor White
        exit 1
    }
} else {
    Write-Host "✅ winget-pkgs directory exists: $wingetPkgsDir" -ForegroundColor Green
    Write-Host "   Updating repository..." -ForegroundColor Gray
    Push-Location $wingetPkgsDir
    git pull origin main
    Pop-Location
}

Write-Host ""

# Create manifest directory
Write-Host "📁 Creating manifest directory..." -ForegroundColor Cyan
New-Item -ItemType Directory -Force -Path $manifestDir | Out-Null
Write-Host "✅ Created: $manifestDir" -ForegroundColor Green

# Copy manifest
Write-Host "📋 Copying manifest..." -ForegroundColor Cyan
$targetManifest = Join-Path $manifestDir "delta-cli.yaml"
Copy-Item $ManifestPath $targetManifest -Force
Write-Host "✅ Copied manifest to: $targetManifest" -ForegroundColor Green
Write-Host ""

# Step 7: Validate in winget-pkgs structure
Write-Host "✅ Step 7: Validating in winget-pkgs structure..." -ForegroundColor Cyan
Write-Host ""

try {
    Push-Location $wingetPkgsDir
    $validationResult = winget validate $targetManifest 2>&1
    if ($LASTEXITCODE -eq 0) {
        Write-Host "✅ Validation passed in winget-pkgs structure!" -ForegroundColor Green
    } else {
        Write-Host "❌ Validation failed:" -ForegroundColor Red
        Write-Host $validationResult -ForegroundColor Red
        Pop-Location
        exit 1
    }
    Pop-Location
} catch {
    Write-Host "⚠️  Could not validate (continuing anyway)" -ForegroundColor Yellow
    Pop-Location
}
Write-Host ""

# Step 8: Prepare git commit
Write-Host "📝 Step 8: Preparing git commit..." -ForegroundColor Cyan
Write-Host ""

Push-Location $wingetPkgsDir

# Check git status
$gitStatus = git status --porcelain
if ($gitStatus -match "manifests\\d\\DeltaCLI\\DeltaCLI\\$Version") {
    Write-Host "✅ Changes detected" -ForegroundColor Green
    
    # Stage changes
    git add "manifests\d\DeltaCLI\DeltaCLI\$Version\"
    Write-Host "✅ Changes staged" -ForegroundColor Green
    
    # Create commit
    $commitMessage = "Add DeltaCLI.DeltaCLI version $Version"
    Write-Host "📝 Commit message: $commitMessage" -ForegroundColor Gray
    
    $createCommit = Read-Host "Create commit? (y/n)"
    if ($createCommit -eq "y" -or $createCommit -eq "Y") {
        git commit -m $commitMessage
        Write-Host "✅ Commit created" -ForegroundColor Green
        
        # Push to fork
        $push = Read-Host "Push to your fork? (y/n)"
        if ($push -eq "y" -or $push -eq "Y") {
            Write-Host "📤 Pushing to origin..." -ForegroundColor Cyan
            git push origin main
            Write-Host "✅ Pushed to fork" -ForegroundColor Green
            Write-Host ""
            Write-Host "🎉 Success! Next step:" -ForegroundColor Green
            Write-Host "   Create a Pull Request at:" -ForegroundColor Cyan
            Write-Host "   https://github.com/$GitHubUsername/winget-pkgs/compare/main...main" -ForegroundColor White
            Write-Host ""
            Write-Host "   Or use GitHub CLI:" -ForegroundColor Cyan
            if ($hasGhCli) {
                Write-Host "   gh pr create --title 'Add DeltaCLI.DeltaCLI version $Version' --body 'Adds Delta CLI package to winget repository'" -ForegroundColor White
            } else {
                Write-Host "   (Install GitHub CLI first: https://cli.github.com/)" -ForegroundColor Gray
            }
        }
    } else {
        Write-Host "⏭️  Skipped commit. You can commit manually later." -ForegroundColor Yellow
    }
} else {
    Write-Host "⚠️  No changes detected. Manifest may already exist." -ForegroundColor Yellow
}

Pop-Location

Write-Host ""
Write-Host "✅ Submission preparation complete!" -ForegroundColor Green
Write-Host ""
Write-Host "📋 Summary:" -ForegroundColor Cyan
Write-Host "   • Manifest updated with SHA256: $sha256" -ForegroundColor White
Write-Host "   • Manifest copied to: $targetManifest" -ForegroundColor White
Write-Host "   • Next: Create PR at https://github.com/microsoft/winget-pkgs" -ForegroundColor White
Write-Host ""

