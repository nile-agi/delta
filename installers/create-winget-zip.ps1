# PowerShell script to create Windows ZIP package for winget installation
# Usage: .\installers\create-winget-zip.ps1 [-Version "1.0.0"] [-BuildDir "build_windows\Release"]

param(
    [string]$Version = "1.0.0",
    [string]$BuildDir = "build_windows\Release",
    [string]$OutputDir = "installers\packages",
    [string]$ZipName = "delta-cli-windows-x64.zip"
)

$ErrorActionPreference = "Stop"

Write-Host "╔══════════════════════════════════════════════════════════════╗" -ForegroundColor Cyan
Write-Host "║    Delta CLI - Create Winget ZIP Package                    ║" -ForegroundColor Cyan
Write-Host "╚══════════════════════════════════════════════════════════════╝" -ForegroundColor Cyan
Write-Host ""

# Resolve paths
$scriptPath = Split-Path -Parent $MyInvocation.MyCommand.Path
$repoRoot = Split-Path -Parent $scriptPath
$buildPath = Join-Path $repoRoot $BuildDir
$outputPath = Join-Path $repoRoot $OutputDir
$zipPath = Join-Path $outputPath $ZipName
$tempDir = Join-Path $env:TEMP "delta-cli-package-$(Get-Random)"

# Check if binaries exist
$deltaExe = Join-Path $buildPath "delta.exe"
$deltaServerExe = Join-Path $buildPath "delta-server.exe"

if (-not (Test-Path $deltaExe)) {
    Write-Host "ERROR: delta.exe not found in $buildPath" -ForegroundColor Red
    Write-Host "Please build the project first using: installers\build_windows.bat" -ForegroundColor Yellow
    exit 1
}

if (-not (Test-Path $deltaServerExe)) {
    Write-Host "ERROR: delta-server.exe not found in $buildPath" -ForegroundColor Red
    Write-Host "Please ensure BUILD_SERVER=ON when building" -ForegroundColor Yellow
    exit 1
}

Write-Host "Building package..." -ForegroundColor Cyan
Write-Host "  Version: $Version" -ForegroundColor Gray
Write-Host "  Build directory: $buildPath" -ForegroundColor Gray
Write-Host "  Output: $zipPath" -ForegroundColor Gray
Write-Host ""

# Create output directory
if (-not (Test-Path $outputPath)) {
    New-Item -ItemType Directory -Path $outputPath -Force | Out-Null
}

# Create temporary directory
if (Test-Path $tempDir) {
    Remove-Item -Recurse -Force $tempDir
}
New-Item -ItemType Directory -Path $tempDir -Force | Out-Null

try {
    # Copy executables to temp directory (at root level for winget)
    Write-Host "Copying executables..." -ForegroundColor Cyan
    
    Copy-Item -Path $deltaExe -Destination (Join-Path $tempDir "delta.exe") -Force
    Copy-Item -Path $deltaServerExe -Destination (Join-Path $tempDir "delta-server.exe") -Force
    
    Write-Host "  ✓ delta.exe" -ForegroundColor Green
    Write-Host "  ✓ delta-server.exe" -ForegroundColor Green
    
    # Check for required DLLs and copy them
    Write-Host ""
    Write-Host "Checking for required DLLs..." -ForegroundColor Cyan
    
    $dlls = Get-ChildItem -Path $buildPath -Filter "*.dll" -ErrorAction SilentlyContinue
    if ($dlls) {
        Write-Host "  Found $($dlls.Count) DLL file(s)" -ForegroundColor Yellow
        foreach ($dll in $dlls) {
            Copy-Item -Path $dll.FullName -Destination (Join-Path $tempDir $dll.Name) -Force
            Write-Host "  ✓ $($dll.Name)" -ForegroundColor Green
        }
    } else {
        Write-Host "  No DLL files found (may require Visual C++ Runtime)" -ForegroundColor Yellow
    }
    
    # Check for llama.cpp server executable (if built separately)
    $serverExe = Join-Path $buildPath "server.exe"
    if (Test-Path $serverExe) {
        Copy-Item -Path $serverExe -Destination (Join-Path $tempDir "server.exe") -Force
        Write-Host "  ✓ server.exe" -ForegroundColor Green
    }
    
    $llamaServerExe = Join-Path $buildPath "llama-server.exe"
    if (Test-Path $llamaServerExe) {
        Copy-Item -Path $llamaServerExe -Destination (Join-Path $tempDir "llama-server.exe") -Force
        Write-Host "  ✓ llama-server.exe" -ForegroundColor Green
    }
    
    # Create ZIP file
    Write-Host ""
    Write-Host "Creating ZIP archive..." -ForegroundColor Cyan
    
    if (Test-Path $zipPath) {
        Remove-Item $zipPath -Force
    }
    
    # Get all files in temp directory
    $filesToZip = Get-ChildItem -Path $tempDir -File
    
    Compress-Archive -Path ($filesToZip.FullName) -DestinationPath $zipPath -CompressionLevel Optimal -Force
    
    Write-Host "  ✓ ZIP created successfully" -ForegroundColor Green
    
    # Display package info
    Write-Host ""
    Write-Host "═══════════════════════════════════════════════════════════════" -ForegroundColor Cyan
    Write-Host "Package created successfully!" -ForegroundColor Green
    Write-Host ""
    Write-Host "File: $zipPath" -ForegroundColor White
    
    $zipInfo = Get-Item $zipPath
    $sizeMB = [math]::Round($zipInfo.Length / 1MB, 2)
    Write-Host "Size: $sizeMB MB" -ForegroundColor Gray
    
    Write-Host ""
    Write-Host "Package contents:" -ForegroundColor Cyan
    Write-Host "  - delta.exe (at root level)" -ForegroundColor Gray
    Write-Host "  - delta-server.exe (at root level)" -ForegroundColor Gray
    if ($dlls) {
        Write-Host "  - $($dlls.Count) DLL file(s)" -ForegroundColor Gray
    }
    
    Write-Host ""
    Write-Host "Next steps:" -ForegroundColor Cyan
    Write-Host "  1. Upload $ZipName to GitHub release v$Version" -ForegroundColor Gray
    Write-Host "  2. Calculate SHA256 hash:" -ForegroundColor Gray
    Write-Host "     .\installers\calculate-sha256.ps1 -FilePath `"$zipPath`"" -ForegroundColor DarkGray
    Write-Host "  3. Update winget manifest with SHA256 hash" -ForegroundColor Gray
    Write-Host ""
    Write-Host "═══════════════════════════════════════════════════════════════" -ForegroundColor Cyan
    
} catch {
    Write-Host ""
    Write-Host "ERROR: $($_.Exception.Message)" -ForegroundColor Red
    exit 1
} finally {
    # Clean up temp directory
    if (Test-Path $tempDir) {
        Remove-Item -Recurse -Force $tempDir -ErrorAction SilentlyContinue
    }
}
