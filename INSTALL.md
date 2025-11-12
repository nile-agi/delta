# Installation Guide for Delta CLI

This guide shows how to install Delta CLI on different platforms using various methods.

## 📦 Quick Installation by Platform

### 🪟 Windows

#### Option 1: Winget (Recommended)
```powershell
winget install DeltaCLI.DeltaCLI
```

#### Option 2: Manual Installation Script
```powershell
# Download and run
Invoke-WebRequest -Uri https://raw.githubusercontent.com/nile-agi/delta/main/install.ps1 -OutFile install.ps1
.\install.ps1
```

#### Option 3: Download Pre-built Binary
1. Go to [GitHub Releases](https://github.com/nile-agi/delta/releases)
2. Download `delta-cli-windows-x64.zip`
3. Extract and add to PATH

---

### 🍎 macOS

#### Option 1: Homebrew (Recommended)
```bash
# Install from tap
brew install nile-agi/delta/delta-cli

# Or if added to homebrew-core
brew install delta-cli
```

#### Option 2: MacPorts
```bash
sudo port install delta-cli
```

#### Option 3: Nix
```bash
# Using nix-env
nix-env -iA nixpkgs.delta-cli

# Or using Nix Flakes
nix profile install nixpkgs#delta-cli
```

#### Option 4: Installation Script
```bash
# Download and run
curl -fsSL https://raw.githubusercontent.com/nile-agi/delta/main/install.sh | bash

# Or manually
curl -L -o install.sh https://raw.githubusercontent.com/nile-agi/delta/main/install.sh
chmod +x install.sh
./install.sh
```

---

### 🐧 Linux

#### Option 1: Homebrew (Recommended)
```bash
# Install from tap
brew install nile-agi/delta/delta-cli

# Or if added to homebrew-core
brew install delta-cli
```

#### Option 2: Nix
```bash
# Using nix-env
nix-env -iA nixpkgs.delta-cli

# Or using Nix Flakes
nix profile install nixpkgs#delta-cli
```

#### Option 3: Installation Script
```bash
# Download and run
curl -fsSL https://raw.githubusercontent.com/nile-agi/delta/main/install.sh | bash

# Or manually
curl -L -o install.sh https://raw.githubusercontent.com/nile-agi/delta/main/install.sh
chmod +x install.sh
./install.sh
```

---

## 📋 Installation Methods Comparison

| Method | Windows | macOS | Linux | Auto-Updates | Notes |
|--------|---------|-------|-------|--------------|-------|
| **Winget** | ✅ | ❌ | ❌ | ✅ | Easiest on Windows |
| **Homebrew** | ❌ | ✅ | ✅ | ✅ | Most popular on macOS/Linux |
| **MacPorts** | ❌ | ✅ | ❌ | ✅ | Alternative on macOS |
| **Nix** | ❌ | ✅ | ✅ | ✅ | Reproducible builds |
| **Install Script** | ✅ | ✅ | ✅ | ❌ | Quick one-time install |
| **Manual** | ✅ | ✅ | ✅ | ❌ | Full control |
| **Build from Source** | ✅ | ✅ | ✅ | ❌ | Latest features |

---

## Manual Installation

### Step 1: Download Pre-built Binary

1. Go to [GitHub Releases](https://github.com/nile-agi/delta/releases)
2. Download the package for your platform:
   - **macOS ARM64**: `delta-cli-macos-arm64.tar.gz`
   - **macOS Intel**: `delta-cli-macos-x86_64.tar.gz`
   - **Linux x86_64**: `delta-cli-linux-x86_64.tar.gz`
   - **Linux ARM64**: `delta-cli-linux-aarch64.tar.gz`
   - **Windows x64**: `delta-cli-windows-x64.zip`

### Step 2: Extract

**macOS / Linux:**
```bash
tar -xzf delta-cli-macos-arm64.tar.gz
```

**Windows:**
```powershell
Expand-Archive -Path delta-cli-windows-x64.zip -DestinationPath delta-cli
```

### Step 3: Install

**macOS / Linux:**
```bash
# Copy to a directory in your PATH
sudo cp delta-cli-*/delta /usr/local/bin/delta
sudo cp delta-cli-*/delta-server /usr/local/bin/delta-server

# Make executable
sudo chmod +x /usr/local/bin/delta /usr/local/bin/delta-server
```

**Windows:**
```powershell
# Copy to Program Files
Copy-Item delta-cli\* "C:\Program Files\Delta CLI\"

# Add to PATH (requires admin)
[Environment]::SetEnvironmentVariable("Path", "$env:Path;C:\Program Files\Delta CLI", "Machine")
```

### Step 4: Verify Installation

```bash
delta --version
```

You should see the Delta CLI version information.

---

## Build from Source

If you prefer to build from source:

### macOS

```bash
# Clone repository with submodules
git clone --recurse-submodules https://github.com/nile-agi/delta.git
cd delta

# Build for your architecture (arm64 for Apple Silicon, x86_64 for Intel)
./packaging/build-scripts/build-macos.sh Release arm64

# Install
sudo cp build_macos_arm64/delta /usr/local/bin/delta
sudo cp build_macos_arm64/delta-server /usr/local/bin/delta-server
sudo chmod +x /usr/local/bin/delta /usr/local/bin/delta-server
```

### Linux

```bash
# Clone repository with submodules
git clone --recurse-submodules https://github.com/nile-agi/delta.git
cd delta

# Build for your architecture
./packaging/build-scripts/build-linux.sh Release x86_64

# Install
sudo cp build_linux_x86_64/delta /usr/local/bin/delta
sudo cp build_linux_x86_64/delta-server /usr/local/bin/delta-server
sudo chmod +x /usr/local/bin/delta /usr/local/bin/delta-server
```

### Windows

```powershell
# Clone repository with submodules
git clone --recurse-submodules https://github.com/nile-agi/delta.git
cd delta

# Build
.\packaging\build-scripts\build-windows.ps1 Release x64

# Copy to a directory in PATH (e.g., C:\Program Files\Delta CLI\)
New-Item -ItemType Directory -Path "C:\Program Files\Delta CLI" -Force
Copy-Item build_windows\Release\delta.exe "C:\Program Files\Delta CLI\delta.exe"
Copy-Item build_windows\Release\delta-server.exe "C:\Program Files\Delta CLI\delta-server.exe"

# Add to PATH (requires admin)
[Environment]::SetEnvironmentVariable("Path", "$env:Path;C:\Program Files\Delta CLI", "Machine")
```

---

## Troubleshooting

### Command Not Found

**macOS / Linux:**
```bash
# Check if in PATH
which delta

# If not, add to PATH
export PATH="$PATH:/usr/local/bin"

# Or add to ~/.bashrc or ~/.zshrc
echo 'export PATH="$PATH:/usr/local/bin"' >> ~/.bashrc
```

**Windows:**
```powershell
# Check if in PATH
$env:Path -split ';' | Select-String "Delta"

# Add to PATH
[Environment]::SetEnvironmentVariable("Path", "$env:Path;C:\Program Files\Delta CLI", "User")
```

### Permission Denied

```bash
# Make executable
chmod +x delta delta-server

# Or install with sudo
sudo ./install.sh
```

### Installation Script Fails

1. Check internet connection
2. Verify GitHub releases are accessible
3. Try manual installation instead
4. Check [GitHub Issues](https://github.com/nile-agi/delta/issues)

---

## Next Steps

After installation:

1. **Run Delta CLI:**
   ```bash
   delta
   ```

2. **Download your first model:**
   ```
   /download llama3.2:1b
   ```

3. **Start chatting:**
   ```
   Hello! How can I help you?
   ```

4. **Get help:**
   ```
   /help
   ```

---

## Uninstallation

### Package Manager

```bash
# Homebrew
brew uninstall delta-cli

# MacPorts
sudo port uninstall delta-cli

# Nix
nix-env -e delta-cli

# Winget
winget uninstall DeltaCLI.DeltaCLI
```

### Manual

**macOS / Linux:**
```bash
sudo rm /usr/local/bin/delta
sudo rm /usr/local/bin/delta-server
sudo rm -rf /usr/local/share/delta-cli
```

**Windows:**
```powershell
Remove-Item "C:\Program Files\Delta CLI" -Recurse -Force
# Remove from PATH manually
```

---

For more information, visit the [main README](README.md).

