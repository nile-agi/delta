# Delta CLI - Complete Installation Guide

This comprehensive guide covers all installation methods for Delta CLI across different platforms.

## 📋 Table of Contents

1. [Quick Start](#quick-start)
2. [Platform-Specific Installation](#platform-specific-installation)
3. [Package Manager Installation](#package-manager-installation)
4. [Manual Installation](#manual-installation)
5. [Build from Source](#build-from-source)
6. [Verification](#verification)
7. [Troubleshooting](#troubleshooting)
8. [Uninstallation](#uninstallation)

---

## 🚀 Quick Start

### Windows
```powershell
winget install DeltaCLI.DeltaCLI
```

### macOS
```bash
brew install nile-agi/delta/delta-cli
```

### Linux
```bash
brew install nile-agi/delta/delta-cli
# OR
curl -fsSL https://raw.githubusercontent.com/nile-agi/delta/main/install.sh | bash
```

---

## 💻 Platform-Specific Installation

### 🪟 Windows

#### Method 1: Winget (Recommended)
```powershell
# Install
winget install DeltaCLI.DeltaCLI

# Update
winget upgrade DeltaCLI.DeltaCLI

# Verify
delta --version
```

**Requirements:**
- Windows 10/11
- Winget (included in Windows 10 1809+ and Windows 11)

#### Method 2: PowerShell Installation Script
```powershell
# Download and run
Invoke-WebRequest -Uri https://raw.githubusercontent.com/nile-agi/delta/main/install.ps1 -OutFile install.ps1
.\install.ps1
```

#### Method 3: Manual Installation
1. Download `delta-cli-windows-x64.zip` from [Releases](https://github.com/nile-agi/delta/releases)
2. Extract to `C:\Program Files\Delta CLI\`
3. Add to PATH:
   ```powershell
   [Environment]::SetEnvironmentVariable("Path", "$env:Path;C:\Program Files\Delta CLI", "Machine")
   ```
4. Restart terminal and verify: `delta --version`

---

### 🍎 macOS

#### Method 1: Homebrew (Recommended)
```bash
# Install from tap
brew tap nile-agi/delta
brew install delta-cli

# Or direct install
brew install nile-agi/delta/delta-cli

# Update
brew upgrade delta-cli

# Verify
delta --version
```

**Requirements:**
- macOS 10.15+ (Catalina or later)
- Homebrew installed

#### Method 2: MacPorts
```bash
# Install
sudo port install delta-cli

# Update
sudo port upgrade delta-cli

# Verify
delta --version
```

**Requirements:**
- macOS with MacPorts installed
- Xcode Command Line Tools

#### Method 3: Nix
```bash
# Using nix-env
nix-env -iA nixpkgs.delta-cli

# Or using Nix Flakes
nix profile install nixpkgs#delta-cli

# Verify
delta --version
```

#### Method 4: Installation Script
```bash
# One-line install
curl -fsSL https://raw.githubusercontent.com/nile-agi/delta/main/install.sh | bash

# Or download first
curl -L -o install.sh https://raw.githubusercontent.com/nile-agi/delta/main/install.sh
chmod +x install.sh
./install.sh
```

#### Method 5: Manual Installation
1. Download `delta-cli-macos-arm64.tar.gz` (Apple Silicon) or `delta-cli-macos-x86_64.tar.gz` (Intel)
2. Extract: `tar -xzf delta-cli-macos-*.tar.gz`
3. Install:
   ```bash
   sudo cp delta-cli-*/delta /usr/local/bin/delta
   sudo cp delta-cli-*/delta-server /usr/local/bin/delta-server
   sudo chmod +x /usr/local/bin/delta /usr/local/bin/delta-server
   ```
4. Verify: `delta --version`

---

### 🐧 Linux

#### Method 1: Homebrew (Recommended)
```bash
# Install Homebrew for Linux first (if not installed)
/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"

# Install Delta CLI
brew install nile-agi/delta/delta-cli

# Update
brew upgrade delta-cli

# Verify
delta --version
```

#### Method 2: Nix
```bash
# Using nix-env
nix-env -iA nixpkgs.delta-cli

# Or using Nix Flakes
nix profile install nixpkgs#delta-cli

# Verify
delta --version
```

#### Method 3: Installation Script
```bash
# One-line install
curl -fsSL https://raw.githubusercontent.com/nile-agi/delta/main/install.sh | bash

# Or download first
curl -L -o install.sh https://raw.githubusercontent.com/nile-agi/delta/main/install.sh
chmod +x install.sh
./install.sh
```

#### Method 4: Manual Installation
1. Download appropriate package:
   - `delta-cli-linux-x86_64.tar.gz` (64-bit Intel/AMD)
   - `delta-cli-linux-aarch64.tar.gz` (ARM64)
2. Extract: `tar -xzf delta-cli-linux-*.tar.gz`
3. Install:
   ```bash
   sudo cp delta-cli-*/delta /usr/local/bin/delta
   sudo cp delta-cli-*/delta-server /usr/local/bin/delta-server
   sudo chmod +x /usr/local/bin/delta /usr/local/bin/delta-server
   ```
4. Verify: `delta --version`

---

## 📦 Package Manager Installation Details

### Winget (Windows)

**Install:**
```powershell
winget install DeltaCLI.DeltaCLI
```

**Update:**
```powershell
winget upgrade DeltaCLI.DeltaCLI
```

**Uninstall:**
```powershell
winget uninstall DeltaCLI.DeltaCLI
```

**List installed:**
```powershell
winget list DeltaCLI.DeltaCLI
```

---

### Homebrew (macOS/Linux)

**Install:**
```bash
# From tap
brew tap nile-agi/delta
brew install delta-cli

# Or direct
brew install nile-agi/delta/delta-cli
```

**Update:**
```bash
brew upgrade delta-cli
```

**Uninstall:**
```bash
brew uninstall delta-cli
```

**Info:**
```bash
brew info delta-cli
```

**Note:** Homebrew automatically updates the llama.cpp submodule on each installation.

---

### MacPorts (macOS)

**Install:**
```bash
sudo port install delta-cli
```

**Update:**
```bash
sudo port upgrade delta-cli
```

**Uninstall:**
```bash
sudo port uninstall delta-cli
```

**Info:**
```bash
port info delta-cli
```

---

### Nix (macOS/Linux)

**Install (nix-env):**
```bash
nix-env -iA nixpkgs.delta-cli
```

**Install (Nix Flakes):**
```bash
nix profile install nixpkgs#delta-cli
```

**Update:**
```bash
# nix-env
nix-env -u delta-cli

# Flakes
nix profile upgrade delta-cli
```

**Uninstall:**
```bash
# nix-env
nix-env -e delta-cli

# Flakes
nix profile remove delta-cli
```

---

## 🔨 Build from Source

Building from source ensures you get the latest features and the most recent llama.cpp version.

### Prerequisites

**All Platforms:**
- Git
- CMake 3.14+
- C++17 compatible compiler
- curl development libraries

**macOS:**
- Xcode Command Line Tools: `xcode-select --install`

**Linux:**
- Build essentials: `sudo apt-get install build-essential` (Debian/Ubuntu)
- Or: `sudo dnf groupinstall "Development Tools"` (Fedora/RHEL)

**Windows:**
- Visual Studio 2019+ with C++ tools
- CMake
- Git for Windows

### Build Steps

#### macOS
```bash
# Clone with submodules
git clone --recurse-submodules https://github.com/nile-agi/delta.git
cd delta

# Build for Apple Silicon
./packaging/build-scripts/build-macos.sh Release arm64

# Build for Intel
./packaging/build-scripts/build-macos.sh Release x86_64

# Install
sudo cp build_macos_arm64/delta /usr/local/bin/delta
sudo cp build_macos_arm64/delta-server /usr/local/bin/delta-server
```

#### Linux
```bash
# Clone with submodules
git clone --recurse-submodules https://github.com/nile-agi/delta.git
cd delta

# Build for x86_64
./packaging/build-scripts/build-linux.sh Release x86_64

# Build for ARM64
./packaging/build-scripts/build-linux.sh Release aarch64

# Install
sudo cp build_linux_x86_64/delta /usr/local/bin/delta
sudo cp build_linux_x86_64/delta-server /usr/local/bin/delta-server
```

#### Windows
```powershell
# Clone with submodules
git clone --recurse-submodules https://github.com/nile-agi/delta.git
cd delta

# Build
.\packaging\build-scripts\build-windows.ps1 Release x64

# Install (copy to PATH directory)
New-Item -ItemType Directory -Path "C:\Program Files\Delta CLI" -Force
Copy-Item build_windows\Release\delta.exe "C:\Program Files\Delta CLI\delta.exe"
Copy-Item build_windows\Release\delta-server.exe "C:\Program Files\Delta CLI\delta-server.exe"
```

**Note:** All build scripts automatically update the llama.cpp submodule before building.

---

## ✅ Verification

After installation, verify it works:

```bash
# Check version
delta --version

# Expected output: Delta CLI v1.0.0

# Check help
delta --help

# List models
delta --list-models
```

---

## 🔧 Troubleshooting

### Command Not Found

**macOS/Linux:**
```bash
# Check if in PATH
which delta

# If not found, add to PATH
export PATH="/usr/local/bin:$PATH"

# Make permanent (add to ~/.bashrc or ~/.zshrc)
echo 'export PATH="/usr/local/bin:$PATH"' >> ~/.zshrc
source ~/.zshrc
```

**Windows:**
```powershell
# Check PATH
$env:Path -split ';' | Select-String "Delta"

# Add to PATH (User)
[Environment]::SetEnvironmentVariable("Path", "$env:Path;C:\Program Files\Delta CLI", "User")

# Restart terminal
```

### Permission Denied

```bash
# Make executable
chmod +x delta delta-server

# Or install with sudo
sudo ./install.sh
```

### Installation Script Fails

1. **Check internet connection**
2. **Verify GitHub is accessible**
3. **Try manual installation**
4. **Check for error messages**
5. **Report issues:** [GitHub Issues](https://github.com/nile-agi/delta/issues)

### Submodule Issues (Build from Source)

If submodules aren't initialized:
```bash
git submodule update --init --recursive
```

### Port Already in Use

If port 8080 is already in use:
```bash
# Find process
lsof -i :8080  # macOS/Linux
netstat -ano | findstr :8080  # Windows

# Kill process
kill -9 <PID>  # macOS/Linux
taskkill /PID <PID> /F  # Windows

# Or use different port
delta server --port 8081
```

---

## 🗑️ Uninstallation

### Package Managers

**Winget:**
```powershell
winget uninstall DeltaCLI.DeltaCLI
```

**Homebrew:**
```bash
brew uninstall delta-cli
```

**MacPorts:**
```bash
sudo port uninstall delta-cli
```

**Nix:**
```bash
# nix-env
nix-env -e delta-cli

# Flakes
nix profile remove delta-cli
```

### Manual Uninstallation

**macOS/Linux:**
```bash
sudo rm /usr/local/bin/delta
sudo rm /usr/local/bin/delta-server
sudo rm -rf /usr/local/share/delta-cli
sudo rm -rf ~/.delta-cli  # User data
```

**Windows:**
```powershell
Remove-Item "C:\Program Files\Delta CLI" -Recurse -Force
Remove-Item "$env:USERPROFILE\.delta-cli" -Recurse -Force
# Remove from PATH manually via System Properties
```

---

## 📚 Next Steps

After successful installation:

1. **Download a model:**
   ```bash
   delta pull qwen2.5:0.5b
   ```

2. **Start the server:**
   ```bash
   delta server
   ```

3. **Or use interactive mode:**
   ```bash
   delta
   ```

4. **Get help:**
   ```bash
   delta --help
   /help  # In interactive mode
   ```

---

## 🔗 Additional Resources

- **Main README:** [README.md](README.md)
- **Quick Start:** [QUICK_START.md](QUICK_START.md)
- **Build Guide:** [BUILD_GUIDE.md](BUILD_GUIDE.md)
- **GitHub Repository:** https://github.com/nile-agi/delta
- **Issues:** https://github.com/nile-agi/delta/issues
- **Releases:** https://github.com/nile-agi/delta/releases

---

## 💡 Tips

1. **Always use package managers** when possible - they handle updates automatically
2. **Keep llama.cpp updated** - All installers automatically use the latest version
3. **Check system requirements** - Ensure you have enough RAM for your chosen model
4. **Start with small models** - Try `qwen2.5:0.5b` first before larger models
5. **Use `--help`** - Delta CLI has extensive help documentation

---

**Need help?** Open an issue on [GitHub](https://github.com/nile-agi/delta/issues) or check the [documentation](README.md).

