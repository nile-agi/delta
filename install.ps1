# Delta CLI Installation Script for Windows
# For end users to install Delta CLI easily

param(
    [string]$Version = "latest",
    [string]$InstallDir = "$env:ProgramFiles\Delta CLI"
)

$ErrorActionPreference = "Stop"

Write-Host "╔══════════════════════════════════════════════════════════════╗" -ForegroundColor Cyan
Write-Host "║           Delta CLI Installation Script                      ║" -ForegroundColor Cyan
Write-Host "╚══════════════════════════════════════════════════════════════╝" -ForegroundColor Cyan
Write-Host ""

# Detect architecture
$Arch = $env:PROCESSOR_ARCHITECTURE
if ($Arch -eq "AMD64") {
    $Platform = "windows-x64"
} else {
    Write-Host "❌ Unsupported architecture: $Arch" -ForegroundColor Red
    exit 1
}

Write-Host "📦 Detected platform: $Platform" -ForegroundColor Green
Write-Host "📁 Installation directory: $InstallDir" -ForegroundColor Green
Write-Host ""

# Create installation directory
if (-not (Test-Path $InstallDir)) {
    Write-Host "📁 Creating installation directory..." -ForegroundColor Yellow
    New-Item -ItemType Directory -Path $InstallDir -Force | Out-Null
}

# Download URL
$RepoUrl = "https://github.com/oderoi/delta-cli"
if ($Version -eq "latest") {
    $DownloadUrl = "$RepoUrl/releases/latest/download/delta-cli-$Platform.zip"
} else {
    $DownloadUrl = "$RepoUrl/releases/download/v$Version/delta-cli-$Platform.zip"
}

Write-Host "⬇️  Downloading Delta CLI..." -ForegroundColor Yellow

$TempDir = Join-Path $env:TEMP "delta-cli-install"
if (Test-Path $TempDir) {
    Remove-Item -Recurse -Force $TempDir
}
New-Item -ItemType Directory -Path $TempDir | Out-Null

$ZipPath = Join-Path $TempDir "delta-cli.zip"

try {
    Invoke-WebRequest -Uri $DownloadUrl -OutFile $ZipPath
} catch {
    Write-Host "❌ Error downloading: $_" -ForegroundColor Red
    exit 1
}

Write-Host "📦 Extracting..." -ForegroundColor Yellow
Expand-Archive -Path $ZipPath -DestinationPath $TempDir -Force

Write-Host "📋 Installing binaries..." -ForegroundColor Yellow
$ExtractedDir = Get-ChildItem -Path $TempDir -Directory | Select-Object -First 1

Copy-Item "$($ExtractedDir.FullName)\delta.exe" "$InstallDir\delta.exe"
Copy-Item "$($ExtractedDir.FullName)\delta-server.exe" "$InstallDir\delta-server.exe"

# Install web UI if present
if (Test-Path "$($ExtractedDir.FullName)\webui") {
    $WebUIDir = Join-Path $InstallDir "webui"
    Write-Host "📋 Installing web UI..." -ForegroundColor Yellow
    Copy-Item -Recurse "$($ExtractedDir.FullName)\webui" $WebUIDir
}

# Add to PATH (requires admin)
$PathEnv = [Environment]::GetEnvironmentVariable("Path", "Machine")
if ($PathEnv -notlike "*$InstallDir*") {
    Write-Host "🔧 Adding to PATH..." -ForegroundColor Yellow
    try {
        [Environment]::SetEnvironmentVariable("Path", "$PathEnv;$InstallDir", "Machine")
        Write-Host "✅ Added to PATH. Please restart your terminal." -ForegroundColor Green
    } catch {
        Write-Host "⚠️  Could not add to PATH automatically. Please add $InstallDir to your PATH manually." -ForegroundColor Yellow
    }
}

# Cleanup
Remove-Item -Recurse -Force $TempDir

Write-Host ""
Write-Host "✅ Delta CLI installed successfully!" -ForegroundColor Green
Write-Host ""
Write-Host "📝 Usage:" -ForegroundColor Cyan
Write-Host "   delta --version    # Check version" -ForegroundColor White
Write-Host "   delta              # Start Delta CLI" -ForegroundColor White
Write-Host ""
Write-Host "📚 For more information, visit: $RepoUrl" -ForegroundColor Cyan
Write-Host ""

