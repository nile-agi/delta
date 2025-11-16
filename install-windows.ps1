# Delta CLI - Complete Windows Installation Script
# Requires: PowerShell 5.1+ (Windows 10+)

# Error handling
$ErrorActionPreference = "Stop"

function Write-ColorOutput($ForegroundColor) {
    $fc = $host.UI.RawUI.ForegroundColor
    $host.UI.RawUI.ForegroundColor = $ForegroundColor
    if ($args) {
        Write-Output $args
    }
    $host.UI.RawUI.ForegroundColor = $fc
}

function Write-Success {
    Write-ColorOutput Green "✓ $args"
}

function Write-Info {
    Write-ColorOutput Cyan "ℹ $args"
}

function Write-Warning {
    Write-ColorOutput Yellow "⚠ $args"
}

function Write-Error {
    Write-ColorOutput Red "❌ Error: $args"
    exit 1
}

Write-Host "╔══════════════════════════════════════════════════════════════╗"
Write-Host "║         Delta CLI - Windows Complete Installation           ║"
Write-Host "╚══════════════════════════════════════════════════════════════╝"
Write-Host ""

# Check if running on Windows
if ($PSVersionTable.PSVersion.Major -lt 5) {
    Write-Error "PowerShell 5.1 or higher is required. Please update PowerShell."
}

# Check if running as Administrator
$isAdmin = ([Security.Principal.WindowsPrincipal] [Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)

# Configuration
$BuildType = "Release"
$InstallPrefix = "C:\Program Files\Delta CLI"
$BuildDir = "build_windows"

# Step 1: Check for Visual Studio / Build Tools
Write-Info "Step 1/8: Checking for Visual Studio / Build Tools..."

$vsPath = $null
$vsVersions = @("2022", "2019", "2017")

foreach ($version in $vsVersions) {
    $possiblePaths = @(
        "C:\Program Files\Microsoft Visual Studio\$version\Community",
        "C:\Program Files\Microsoft Visual Studio\$version\Professional",
        "C:\Program Files\Microsoft Visual Studio\$version\Enterprise",
        "C:\Program Files (x86)\Microsoft Visual Studio\$version\Community",
        "C:\Program Files (x86)\Microsoft Visual Studio\$version\Professional",
        "C:\Program Files (x86)\Microsoft Visual Studio\$version\Enterprise"
    )
    
    foreach ($path in $possiblePaths) {
        if (Test-Path "$path\VC\Auxiliary\Build\vcvarsall.bat") {
            $vsPath = $path
            break
        }
    }
    if ($vsPath) { break }
}

if (-not $vsPath) {
    # Check for standalone Build Tools
    $buildToolsPaths = @(
        "C:\Program Files (x86)\Microsoft Visual Studio\2019\BuildTools",
        "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools"
    )
    
    foreach ($path in $buildToolsPaths) {
        if (Test-Path "$path\VC\Auxiliary\Build\vcvarsall.bat") {
            $vsPath = $path
            break
        }
    }
}

if (-not $vsPath) {
    Write-Warning "Visual Studio or Build Tools not found."
    Write-Info "Please install Visual Studio 2019 or 2022 with C++ workload, or"
    Write-Info "download Build Tools from: https://visualstudio.microsoft.com/downloads/"
    Write-Info "Select 'Desktop development with C++' workload"
    Write-Error "Visual Studio / Build Tools required"
}

Write-Success "Visual Studio found at: $vsPath"

# Step 2: Check/Install Chocolatey (optional but recommended)
Write-Info "Step 2/8: Checking for package manager..."
$hasChoco = $false
if (Get-Command choco -ErrorAction SilentlyContinue) {
    Write-Success "Chocolatey found"
    $hasChoco = $true
} else {
    Write-Info "Chocolatey not found (optional, but recommended for easier dependency management)"
    Write-Info "To install: Set-ExecutionPolicy Bypass -Scope Process -Force; [System.Net.ServicePointManager]::SecurityProtocol = [System.Net.ServicePointManager]::SecurityProtocol -bor 3072; iex ((New-Object System.Net.WebClient).DownloadString('https://community.chocolatey.org/install.ps1'))"
}

# Step 3: Install dependencies
Write-Info "Step 3/8: Installing dependencies..."

$missingDeps = @()

# Check CMake
if (-not (Get-Command cmake -ErrorAction SilentlyContinue)) {
    $missingDeps += "cmake"
}

# Check Git
if (-not (Get-Command git -ErrorAction SilentlyContinue)) {
    $missingDeps += "git"
}

if ($missingDeps.Count -gt 0) {
    if ($hasChoco) {
        Write-Info "Installing missing dependencies: $($missingDeps -join ', ')"
        foreach ($dep in $missingDeps) {
            choco install $dep -y
        }
        Write-Success "Dependencies installed"
    } else {
        Write-Error "Missing dependencies: $($missingDeps -join ', '). Please install manually or install Chocolatey."
    }
} else {
    Write-Success "All dependencies already installed"
}

# Verify CMake version
$cmakeVersion = (cmake --version | Select-Object -First 1).ToString()
Write-Info "CMake version: $cmakeVersion"

# Step 4: Check for vendored llama-cpp
Write-Info "Step 4/8: Verifying project structure..."
if (-not (Test-Path "vendor\llama-cpp\CMakeLists.txt")) {
    Write-Error "llama-cpp not found in vendor\llama-cpp\. Please ensure you're in the delta-cli directory."
}
Write-Success "Project structure verified"

# Step 5: Setup build environment
Write-Info "Step 5/8: Setting up build environment..."

# Find vcvarsall.bat
$vcvarsPath = Join-Path $vsPath "VC\Auxiliary\Build\vcvarsall.bat"
if (-not (Test-Path $vcvarsPath)) {
    Write-Error "vcvarsall.bat not found at: $vcvarsPath"
}

# Determine architecture (default to x64)
$arch = "x64"
if ([Environment]::Is64BitOperatingSystem) {
    $arch = "x64"
} else {
    $arch = "x86"
}
Write-Info "Building for: $arch"

# Step 6: Build
Write-Info "Step 6/8: Building Delta CLI..."

# Create build directory
New-Item -ItemType Directory -Force -Path $BuildDir | Out-Null
Set-Location $BuildDir

# Configure
Write-Info "Configuring build..."
& cmd /c "`"$vcvarsPath`" $arch && cmake .. -DCMAKE_BUILD_TYPE=$BuildType -DCMAKE_INSTALL_PREFIX=`"$InstallPrefix`" -DBUILD_TESTS=ON -DBUILD_SERVER=ON"

if ($LASTEXITCODE -ne 0) {
    Set-Location ..
    Write-Error "CMake configuration failed"
}

# Build
Write-Info "Compiling (this may take several minutes)..."
$cpuCount = (Get-WmiObject Win32_Processor).NumberOfCores
& cmd /c "`"$vcvarsPath`" $arch && cmake --build . --config $BuildType -j $cpuCount"

if ($LASTEXITCODE -ne 0) {
    Set-Location ..
    Write-Error "Build failed"
}

# Verify binary exists
if (-not (Test-Path "delta.exe")) {
    Set-Location ..
    Write-Error "Build completed but delta.exe not found"
}

Write-Success "Build completed successfully"
Set-Location ..

# Step 6.5: Build web UI from assets/ if needed
Write-Info "Step 6.5/8: Building web UI from assets/..."
if (Test-Path "assets") {
    if (-not ((Test-Path "public\index.html.gz") -or (Test-Path "public\index.html"))) {
        Write-Info "Web UI not built, building now..."
        Set-Location assets
        
        if (-not (Test-Path "node_modules")) {
            if (-not (Get-Command npm -ErrorAction SilentlyContinue)) {
                Write-Warning "npm not found. Please install Node.js:"
                Write-Info "  Download from: https://nodejs.org/"
                Write-Info "  Or use Chocolatey: choco install nodejs"
                Write-Error "Node.js and npm are required to build the web UI"
            }
            Write-Info "Installing web UI dependencies..."
            npm install
            if ($LASTEXITCODE -ne 0) {
                Set-Location ..
                Write-Error "Failed to install web UI dependencies"
            }
        }
        Write-Info "Building web UI..."
        npm run build
        if ($LASTEXITCODE -ne 0) {
            Set-Location ..
            Write-Error "Failed to build web UI"
        }
        Set-Location ..
        Write-Success "Web UI built successfully"
    } else {
        Write-Success "Web UI already built"
    }
} else {
    Write-Warning "assets/ directory not found. Web UI will not be available."
}

# Step 7: Install system-wide
Write-Info "Step 7/8: Installing system-wide..."

if (-not $isAdmin) {
    Write-Warning "System-wide installation requires Administrator privileges"
    Write-Info "Installing to: $InstallPrefix"
    Write-Info "Please run PowerShell as Administrator, or install manually:"
    Write-Info "  Copy build_windows\delta.exe to a directory in your PATH"
} else {
    # Create install directory
    New-Item -ItemType Directory -Force -Path $InstallPrefix | Out-Null
    
    # Copy binaries
    Copy-Item "$BuildDir\delta.exe" "$InstallPrefix\delta.exe" -Force
    if (Test-Path "$BuildDir\delta-server.exe") {
        Copy-Item "$BuildDir\delta-server.exe" "$InstallPrefix\delta-server.exe" -Force
    }
    
    # Install web UI if built
    if ((Test-Path "public") -and ((Test-Path "public\index.html") -or (Test-Path "public\index.html.gz"))) {
        $webuiDir = Join-Path $InstallPrefix "webui"
        Write-Info "Installing web UI..."
        New-Item -ItemType Directory -Force -Path $webuiDir | Out-Null
        Copy-Item "public\*" $webuiDir -Recurse -Force
        Write-Success "Web UI installed to $webuiDir"
    } else {
        Write-Warning "Web UI not found. Server will use embedded UI or find files at runtime."
    }
    
    # Add to PATH (system-wide)
    $currentPath = [Environment]::GetEnvironmentVariable("Path", "Machine")
    if ($currentPath -notlike "*$InstallPrefix*") {
        [Environment]::SetEnvironmentVariable("Path", "$currentPath;$InstallPrefix", "Machine")
        Write-Success "Added $InstallPrefix to system PATH"
    }
    
    Write-Success "Delta CLI installed to $InstallPrefix"
}

# Step 8: Final verification
Write-Info "Step 8/8: Final verification..."
# Verify installation
if (Test-Path "$InstallPrefix\delta.exe") {
    Write-Success "Delta CLI installed successfully"
    
    # Refresh PATH for current session
    $env:Path = [System.Environment]::GetEnvironmentVariable("Path", "Machine") + ";" + [System.Environment]::GetEnvironmentVariable("Path", "User")
    
    if (Get-Command delta -ErrorAction SilentlyContinue) {
        Write-Success "Delta CLI is available in PATH"
        $deltaVersion = (delta --version 2>&1 | Select-Object -First 1).ToString()
        Write-Info "Installed version: $deltaVersion"
    } else {
        Write-Warning "Delta CLI may not be in your PATH"
        Write-Info "Restart PowerShell or add manually:"
        Write-Info "  `$env:Path += `";$InstallPrefix`""
    }
} else {
    Write-Warning "Installation verification failed. You may need to run as Administrator."
}

Write-Host ""
Write-Host "╔══════════════════════════════════════════════════════════════╗"
Write-Host "║              Installation Complete!                         ║"
Write-Host "╚══════════════════════════════════════════════════════════════╝"
Write-Host ""
Write-Success "Delta CLI has been successfully installed!"
Write-Host ""
Write-Host "Next steps:"
Write-Host "  1. Test installation: delta --version"
Write-Host "  2. Download a model: delta pull qwen3:0.6b"
Write-Host "  3. Start chatting: delta"
Write-Host ""
Write-Host "If 'delta' command is not found, restart PowerShell or run:"
Write-Host "  `$env:Path += `";$InstallPrefix`""
Write-Host ""
