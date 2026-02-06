# PowerShell script to create Windows ZIP package for winget installation
# Usage: .\installers\create-winget-zip.ps1 [-Version "1.0.0"] [-BuildDir "build_windows\Release"]
# Note: This script is designed for Windows. For cross-platform use, ensure Windows executables are built.

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

# Check if running on Windows
if ($PSVersionTable.Platform -ne "Win32NT" -and -not $IsWindows) {
    Write-Host "WARNING: This script is designed for Windows." -ForegroundColor Yellow
    Write-Host "You are running on: $($PSVersionTable.Platform)" -ForegroundColor Yellow
    Write-Host ""
    Write-Host "To create a Windows ZIP package:" -ForegroundColor Cyan
    Write-Host "  1. Build Delta CLI on Windows (or cross-compile)" -ForegroundColor Gray
    Write-Host "  2. Run this script on Windows, OR" -ForegroundColor Gray
    Write-Host "  3. Manually create ZIP with executables at root level" -ForegroundColor Gray
    Write-Host ""
    Write-Host "Continuing anyway (assuming Windows executables exist)..." -ForegroundColor Yellow
    Write-Host ""
}

# Resolve paths
$scriptPath = Split-Path -Parent $MyInvocation.MyCommand.Path
$repoRoot = Split-Path -Parent $scriptPath

# Normalize path separators for cross-platform compatibility
if ($IsWindows -or $PSVersionTable.Platform -eq "Win32NT") {
    $BuildDir = $BuildDir -replace '/', '\'
    $OutputDir = $OutputDir -replace '/', '\'
} else {
    $BuildDir = $BuildDir -replace '\\', '/'
    $OutputDir = $OutputDir -replace '\\', '/'
}

$buildPath = Join-Path $repoRoot $BuildDir
$outputPath = Join-Path $repoRoot $OutputDir
$zipPath = Join-Path $outputPath $ZipName

# Determine temp directory (cross-platform)
if ($IsWindows -or $PSVersionTable.Platform -eq "Win32NT") {
    $tempBase = $env:TEMP
} elseif ($env:TMPDIR) {
    $tempBase = $env:TMPDIR
} elseif ($env:TMP) {
    $tempBase = $env:TMP
} else {
    $tempBase = "/tmp"
}

if ([string]::IsNullOrEmpty($tempBase)) {
    $tempBase = [System.IO.Path]::GetTempPath()
}

$tempDir = Join-Path $tempBase "delta-cli-package-$(Get-Random)"

# Check if binaries exist
$deltaExe = Join-Path $buildPath "delta.exe"
$deltaServerExe = Join-Path $buildPath "delta-server.exe"

# Normalize paths for cross-platform
$deltaExe = [System.IO.Path]::GetFullPath((Join-Path $repoRoot (Join-Path $BuildDir "delta.exe")))
$deltaServerExe = [System.IO.Path]::GetFullPath((Join-Path $repoRoot (Join-Path $BuildDir "delta-server.exe")))

if (-not (Test-Path $deltaExe)) {
    Write-Host "ERROR: delta.exe not found in $buildPath" -ForegroundColor Red
    Write-Host "Expected location: $deltaExe" -ForegroundColor Gray
    Write-Host ""
    Write-Host "Please build the project first:" -ForegroundColor Yellow
    if ($IsWindows -or $PSVersionTable.Platform -eq "Win32NT") {
        Write-Host "  installers\build_windows.bat" -ForegroundColor Gray
    } else {
        Write-Host "  Build on Windows, or ensure Windows executables exist at:" -ForegroundColor Gray
        Write-Host "  $deltaExe" -ForegroundColor Gray
    }
    exit 1
}

if (-not (Test-Path $deltaServerExe)) {
    Write-Host "ERROR: delta-server.exe not found in $buildPath" -ForegroundColor Red
    Write-Host "Expected location: $deltaServerExe" -ForegroundColor Gray
    Write-Host ""
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
    
    # Use normalized paths
    $deltaExeNormalized = [System.IO.Path]::GetFullPath($deltaExe)
    $deltaServerExeNormalized = [System.IO.Path]::GetFullPath($deltaServerExe)
    
    Copy-Item -Path $deltaExeNormalized -Destination (Join-Path $tempDir "delta.exe") -Force
    Copy-Item -Path $deltaServerExeNormalized -Destination (Join-Path $tempDir "delta-server.exe") -Force
    
    Write-Host "  ✓ delta.exe" -ForegroundColor Green
    Write-Host "  ✓ delta-server.exe" -ForegroundColor Green
    
    # Check for required DLLs and copy them
    Write-Host ""
    Write-Host "Checking for required DLLs..." -ForegroundColor Cyan
    
    # Use normalized build path
    $normalizedBuildPath = [System.IO.Path]::GetFullPath((Join-Path $repoRoot $BuildDir))
    $dlls = Get-ChildItem -Path $normalizedBuildPath -Filter "*.dll" -ErrorAction SilentlyContinue
    if ($dlls) {
        Write-Host "  Found $($dlls.Count) DLL file(s)" -ForegroundColor Yellow
        foreach ($dll in $dlls) {
            Copy-Item -Path $dll.FullName -Destination (Join-Path $tempDir $dll.Name) -Force
            Write-Host "  ✓ $($dll.Name)" -ForegroundColor Green
        }
    } else {
        Write-Host "  No DLL files found (may require Visual C++ Runtime)" -ForegroundColor Yellow
        Write-Host "  Note: DLLs are typically only needed on Windows" -ForegroundColor Gray
    }
    
    # Check for llama.cpp server executable (if built separately)
    $normalizedBuildPath = [System.IO.Path]::GetFullPath((Join-Path $repoRoot $BuildDir))
    $serverExe = Join-Path $normalizedBuildPath "server.exe"
    if (Test-Path $serverExe) {
        Copy-Item -Path $serverExe -Destination (Join-Path $tempDir "server.exe") -Force
        Write-Host "  ✓ server.exe" -ForegroundColor Green
    }
    
    $llamaServerExe = Join-Path $normalizedBuildPath "llama-server.exe"
    if (Test-Path $llamaServerExe) {
        Copy-Item -Path $llamaServerExe -Destination (Join-Path $tempDir "llama-server.exe") -Force
        Write-Host "  ✓ llama-server.exe" -ForegroundColor Green
    }
    
    # Create ZIP file
    Write-Host ""
    Write-Host "Creating ZIP archive..." -ForegroundColor Cyan
    
    # Normalize zip path
    $zipPathNormalized = [System.IO.Path]::GetFullPath($zipPath)
    
    if (Test-Path $zipPathNormalized) {
        Remove-Item $zipPathNormalized -Force
    }
    
    # Ensure output directory exists
    $outputDirNormalized = [System.IO.Path]::GetDirectoryName($zipPathNormalized)
    if (-not (Test-Path $outputDirNormalized)) {
        New-Item -ItemType Directory -Path $outputDirNormalized -Force | Out-Null
    }
    
    # Get all files in temp directory
    $filesToZip = Get-ChildItem -Path $tempDir -File
    
    Compress-Archive -Path ($filesToZip.FullName) -DestinationPath $zipPathNormalized -CompressionLevel Optimal -Force
    
    Write-Host "  ✓ ZIP created successfully" -ForegroundColor Green
    
    # Display package info
    Write-Host ""
    Write-Host "═══════════════════════════════════════════════════════════════" -ForegroundColor Cyan
    Write-Host "Package created successfully!" -ForegroundColor Green
    Write-Host ""
    Write-Host "File: $zipPathNormalized" -ForegroundColor White
    
    $zipInfo = Get-Item $zipPathNormalized
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
    Write-Host "     .\installers\calculate-sha256.ps1 -FilePath `"$zipPathNormalized`"" -ForegroundColor DarkGray
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
