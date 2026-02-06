# PowerShell script to calculate SHA256 hash of the ZIP file
# Usage: .\installers\calculate-sha256.ps1 [-FilePath "path\to\file.zip"]

param(
    [Parameter(Mandatory=$false)]
    [string]$FilePath = "",
    
    [string]$Version = "1.0.0",
    [string]$OutputDir = "installers\packages",
    [string]$ZipName = "delta-cli-windows-x64.zip"
)

$ErrorActionPreference = "Stop"

Write-Host "Calculating SHA256 hash..." -ForegroundColor Cyan
Write-Host ""

# If no file path provided, use default
if ([string]::IsNullOrEmpty($FilePath)) {
    $scriptPath = Split-Path -Parent $MyInvocation.MyCommand.Path
    $repoRoot = Split-Path -Parent $scriptPath
    $outputPath = Join-Path $repoRoot $OutputDir
    $FilePath = Join-Path $outputPath $ZipName
}

if (-not (Test-Path $FilePath)) {
    Write-Host "ERROR: File not found: $FilePath" -ForegroundColor Red
    Write-Host ""
    Write-Host "Usage:" -ForegroundColor Yellow
    Write-Host "  .\installers\calculate-sha256.ps1 -FilePath `"path\to\delta-cli-windows-x64.zip`"" -ForegroundColor Gray
    Write-Host ""
    Write-Host "Or create the ZIP first:" -ForegroundColor Yellow
    Write-Host "  .\installers\create-winget-zip.ps1" -ForegroundColor Gray
    exit 1
}

Write-Host "File: $FilePath" -ForegroundColor Gray
Write-Host ""

# Calculate SHA256
$hash = Get-FileHash -Path $FilePath -Algorithm SHA256

Write-Host "SHA256 Hash:" -ForegroundColor Green
Write-Host $hash.Hash -ForegroundColor White
Write-Host ""

Write-Host "Copy this hash to the InstallerSha256 field in:" -ForegroundColor Cyan
Write-Host "  manifests/d/DeltaCLI/DeltaCLI/1.0.0/DeltaCLI.DeltaCLI.1.0.0.installer.yaml" -ForegroundColor Gray
Write-Host ""
Write-Host "Or update the winget manifest:" -ForegroundColor Cyan
Write-Host "  .\packaging\winget\update-sha256.ps1 -Version $Version -Sha256 $($hash.Hash)" -ForegroundColor Gray
