# Installs the Delta CLI on Windows:
#   irm https://github.com/nile-agi/delta-release/releases/latest/download/install.ps1 | iex
# Set $env:DELTA_VERSION = '<tag>' (e.g. v1.0.18-rc.5) to install a specific release.
$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'

$repo = 'https://github.com/nile-agi/delta-release'
$base = if ($env:DELTA_VERSION) { "$repo/releases/download/$env:DELTA_VERSION" } else { "$repo/releases/latest/download" }
$programs = "$env:LOCALAPPDATA\Programs"
$dest = "$programs\delta-cli"
$temp = Join-Path $env:TEMP "delta-cli-$([guid]::NewGuid())"
$zip = "$temp.zip"

Write-Host 'Downloading Delta CLI (delta-cli-windows-x64.zip)...'
Invoke-WebRequest "$base/delta-cli-windows-x64.zip" -OutFile $zip -UseBasicParsing
try {
    New-Item $programs -ItemType Directory -Force | Out-Null
    Expand-Archive $zip $temp -Force
    Remove-Item $dest -Recurse -Force -ErrorAction SilentlyContinue
    Move-Item (Get-ChildItem "$temp\delta-cli-*" | Select-Object -First 1).FullName $dest
    Get-ChildItem $dest -Recurse | Unblock-File
} finally {
    Remove-Item $zip, $temp -Recurse -Force -ErrorAction SilentlyContinue
}

$path = [Environment]::GetEnvironmentVariable('Path', 'User')
if ($path -notlike "*$dest\bin*") {
    [Environment]::SetEnvironmentVariable('Path', "$path;$dest\bin", 'User')
}
# `irm | iex` runs in this session, so updating its PATH makes `delta` work right away.
if ($env:Path -notlike "*$dest\bin*") { $env:Path = "$env:Path;$dest\bin" }
Write-Host "Installed. Run: delta"
