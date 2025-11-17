# Update SHA256 hash in winget manifest
# Usage: .\update-sha256.ps1 [zip-file-path] [version]
# Example: .\update-sha256.ps1 ..\..\release\delta-cli-windows-x64.zip 1.0.0

param(
    [Parameter(Mandatory=$true)]
    [string]$ZipPath,
    
    [Parameter(Mandatory=$true)]
    [string]$Version
)

$ErrorActionPreference = "Stop"

if (-not (Test-Path $ZipPath)) {
    Write-Host "❌ Error: ZIP file not found: $ZipPath" -ForegroundColor Red
    exit 1
}

Write-Host "📦 Calculating SHA256 hash for: $ZipPath" -ForegroundColor Cyan
$hash = (Get-FileHash $ZipPath -Algorithm SHA256).Hash.ToLower()

Write-Host "✅ SHA256: $hash" -ForegroundColor Green
Write-Host ""

# Update manifest file
$manifestPath = Join-Path $PSScriptRoot "delta-cli.yaml"

if (-not (Test-Path $manifestPath)) {
    Write-Host "❌ Error: Manifest file not found: $manifestPath" -ForegroundColor Red
    exit 1
}

Write-Host "📝 Updating manifest: $manifestPath" -ForegroundColor Cyan

# Read manifest content
$content = Get-Content $manifestPath -Raw

# Update version
$content = $content -replace "PackageVersion: \d+\.\d+\.\d+", "PackageVersion: $Version"

# Update SHA256
$content = $content -replace "InstallerSha256: [0-9a-f]{64}", "InstallerSha256: $hash"

# Update URL if version is specified
if ($Version) {
    $content = $content -replace "releases/download/v\d+\.\d+\.\d+", "releases/download/v$Version"
}

# Write back
$content | Set-Content $manifestPath -NoNewline

Write-Host "✅ Manifest updated successfully!" -ForegroundColor Green
Write-Host ""
Write-Host "📋 Next steps:" -ForegroundColor Cyan
Write-Host "   1. Review the updated manifest: $manifestPath" -ForegroundColor Yellow
Write-Host "   2. Validate the manifest: winget validate $manifestPath" -ForegroundColor Yellow
Write-Host "   3. Submit to winget-pkgs repository" -ForegroundColor Yellow

