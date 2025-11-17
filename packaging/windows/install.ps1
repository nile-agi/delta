# Delta CLI Installation Script for Windows
# Handles dependencies, installation, and PATH configuration

param(
    [switch]$SkipDependencies
)

$ErrorActionPreference = "Stop"

Write-Host "╔══════════════════════════════════════════════════════════════╗" -ForegroundColor Cyan
Write-Host "║           Delta CLI Installation (Windows)                 ║" -ForegroundColor Cyan
Write-Host "╚══════════════════════════════════════════════════════════════╝" -ForegroundColor Cyan
Write-Host ""

# Check if running as Administrator
$isAdmin = ([Security.Principal.WindowsPrincipal] [Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)

if (-not $isAdmin) {
    Write-Host "❌ This script requires Administrator privileges" -ForegroundColor Red
    Write-Host "   Right-click PowerShell and select 'Run as Administrator'" -ForegroundColor Yellow
    exit 1
}

# Detect architecture
$arch = $env:PROCESSOR_ARCHITECTURE
if ($arch -eq "AMD64") {
    $platform = "windows-x64"
} elseif ($arch -eq "ARM64") {
    $platform = "windows-arm64"
} else {
    Write-Host "❌ Unsupported architecture: $arch" -ForegroundColor Red
    exit 1
}

Write-Host "📦 Detected platform: $platform" -ForegroundColor Green
Write-Host ""

# No build dependencies needed - we're installing pre-built binaries
# PowerShell has built-in support for downloading and extracting

# Download and install Delta CLI
Write-Host "⬇️  Downloading Delta CLI..." -ForegroundColor Cyan
$tempDir = New-TemporaryFile | ForEach-Object { Remove-Item $_; New-Item -ItemType Directory -Path $_ }
$downloadUrl = "https://github.com/nile-agi/delta/releases/latest/download/delta-cli-${platform}.zip"

try {
    $response = Invoke-WebRequest -Uri $downloadUrl -OutFile "$tempDir\delta-cli.zip" -ErrorAction Stop
} catch {
    Write-Host "❌ Failed to download Delta CLI from: $downloadUrl" -ForegroundColor Red
    Write-Host "" -ForegroundColor Red
    Write-Host "⚠️  This usually means:" -ForegroundColor Yellow
    Write-Host "   1. No release has been created yet on GitHub" -ForegroundColor Yellow
    Write-Host "   2. The release doesn't include a Windows package" -ForegroundColor Yellow
    Write-Host "" -ForegroundColor Yellow
    Write-Host "📝 Alternative installation methods:" -ForegroundColor Cyan
    Write-Host "   • Build from source: See README.md for build instructions" -ForegroundColor White
    Write-Host "   • Check GitHub releases: https://github.com/nile-agi/delta/releases" -ForegroundColor White
    Write-Host "   • Use winget (when available): winget install DeltaCLI.DeltaCLI" -ForegroundColor White
    Write-Host "" -ForegroundColor White
    exit 1
}

Write-Host "📦 Extracting..." -ForegroundColor Cyan
Expand-Archive -Path "$tempDir\delta-cli.zip" -DestinationPath $tempDir -Force

Write-Host "📋 Installing binaries..." -ForegroundColor Cyan
$installDir = "C:\Program Files\Delta CLI"
New-Item -ItemType Directory -Force -Path $installDir | Out-Null

$extractedDir = Get-ChildItem -Path $tempDir -Directory | Where-Object { $_.Name -like "delta-cli-*" } | Select-Object -First 1
Copy-Item "$($extractedDir.FullName)\delta.exe" "$installDir\delta.exe" -Force
if (Test-Path "$($extractedDir.FullName)\delta-server.exe") {
    Copy-Item "$($extractedDir.FullName)\delta-server.exe" "$installDir\delta-server.exe" -Force
}

# Install web UI if present
if (Test-Path "$($extractedDir.FullName)\webui") {
    $webuiDir = "$installDir\webui"
    Write-Host "📋 Installing web UI..." -ForegroundColor Cyan
    New-Item -ItemType Directory -Force -Path $webuiDir | Out-Null
    Copy-Item "$($extractedDir.FullName)\webui\*" $webuiDir -Recurse -Force
}

# Cleanup
Remove-Item $tempDir -Recurse -Force

# Configure PATH
Write-Host "🔧 Configuring PATH..." -ForegroundColor Cyan
$currentPath = [Environment]::GetEnvironmentVariable("Path", "Machine")
if ($currentPath -notlike "*$installDir*") {
    $newPath = "$currentPath;$installDir"
    [Environment]::SetEnvironmentVariable("Path", $newPath, "Machine")
    Write-Host "✅ Added $installDir to system PATH" -ForegroundColor Green
    
    # Update current session PATH
    $env:Path += ";$installDir"
}

# Create desktop shortcut (optional)
$desktop = [Environment]::GetFolderPath("Desktop")
$shortcutPath = "$desktop\Delta CLI.lnk"
$shell = New-Object -ComObject WScript.Shell
$shortcut = $shell.CreateShortcut($shortcutPath)
$shortcut.TargetPath = "$installDir\delta.exe"
$shortcut.WorkingDirectory = $installDir
$shortcut.Description = "Delta CLI - Offline AI Assistant"
$shortcut.Save()

Write-Host ""
Write-Host "✅ Delta CLI installed successfully!" -ForegroundColor Green
Write-Host ""
Write-Host "📝 Quick Start:" -ForegroundColor Cyan
Write-Host "   delta --version           # Check version"
Write-Host "   delta pull qwen2.5:0.5b   # Download a model"
Write-Host "   delta server              # Start web server"
Write-Host ""
Write-Host "💡 Note: You may need to restart your terminal for PATH changes to take effect" -ForegroundColor Yellow

