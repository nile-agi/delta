# Delta CLI Uninstall Script for Windows

param(
    [switch]$Force
)

$ErrorActionPreference = "Stop"

Write-Host "╔══════════════════════════════════════════════════════════════╗" -ForegroundColor Cyan
Write-Host "║              Delta CLI Uninstaller (Windows)                ║" -ForegroundColor Cyan
Write-Host "╚══════════════════════════════════════════════════════════════╝" -ForegroundColor Cyan
Write-Host ""

# Check if running as Administrator
$isAdmin = ([Security.Principal.WindowsPrincipal] [Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)

if (-not $isAdmin) {
    Write-Host "❌ This script requires Administrator privileges" -ForegroundColor Red
    Write-Host "   Right-click PowerShell and select 'Run as Administrator'" -ForegroundColor Yellow
    exit 1
}

$installDir = "C:\Program Files\Delta CLI"

if (-not (Test-Path $installDir)) {
    Write-Host "⚠️  Delta CLI not found at: $installDir" -ForegroundColor Yellow
    Write-Host "   It may have been uninstalled already or installed to a different location" -ForegroundColor Yellow
    exit 0
}

if (-not $Force) {
    Write-Host "⚠️  This will remove Delta CLI from your system" -ForegroundColor Yellow
    $response = Read-Host "Continue? (y/n)"
    if ($response -ne "y" -and $response -ne "Y") {
        Write-Host "Cancelled." -ForegroundColor Yellow
        exit 0
    }
}

Write-Host "🗑️  Removing Delta CLI..." -ForegroundColor Cyan

# Remove installation directory
if (Test-Path $installDir) {
    Write-Host "  Removing: $installDir" -ForegroundColor Gray
    Remove-Item -Recurse -Force $installDir
}

# Remove from PATH
Write-Host "  Removing from system PATH..." -ForegroundColor Gray
$currentPath = [Environment]::GetEnvironmentVariable("Path", "Machine")
if ($currentPath -like "*$installDir*") {
    $newPath = $currentPath -replace [regex]::Escape(";$installDir"), ""
    [Environment]::SetEnvironmentVariable("Path", $newPath, "Machine")
    Write-Host "  ✅ Removed from PATH" -ForegroundColor Green
}

# Remove desktop shortcut
$shortcutPath = "$env:USERPROFILE\Desktop\Delta CLI.lnk"
if (Test-Path $shortcutPath) {
    Write-Host "  Removing desktop shortcut..." -ForegroundColor Gray
    Remove-Item $shortcutPath -Force
}

# Remove user data
$userDataPaths = @(
    "$env:USERPROFILE\.delta",
    "$env:APPDATA\delta-cli"
)

foreach ($path in $userDataPaths) {
    if (Test-Path $path) {
        Write-Host "  Removing user data: $path" -ForegroundColor Gray
        Remove-Item -Recurse -Force $path
    }
}

Write-Host ""
Write-Host "✅ Delta CLI uninstalled successfully!" -ForegroundColor Green
Write-Host ""
Write-Host "💡 Note: You may need to restart your terminal for PATH changes to take effect" -ForegroundColor Yellow

