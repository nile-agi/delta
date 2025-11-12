# Installation Guide for End Users

This guide shows how **end users** can install Delta CLI on their systems.

## Quick Installation

### macOS / Linux

```bash
# Download and run the installation script
curl -fsSL https://raw.githubusercontent.com/oderoi/delta-cli/main/install.sh | bash
```

Or manually:

```bash
# Download
curl -L -o install.sh https://raw.githubusercontent.com/oderoi/delta-cli/main/install.sh

# Make executable
chmod +x install.sh

# Run
./install.sh
```

### Windows

```powershell
# Download and run
Invoke-WebRequest -Uri https://raw.githubusercontent.com/oderoi/delta-cli/main/install.ps1 -OutFile install.ps1
.\install.ps1
```

---

## Package Manager Installation (Recommended)

### Windows - Winget

```powershell
winget install DeltaCLI.DeltaCLI
```

### macOS - Homebrew

```bash
# Using tap (before official inclusion)
brew install oderoi/delta-cli/delta-cli

# Or after official inclusion
brew install delta-cli
```

### macOS - MacPorts

```bash
sudo port install delta-cli
```

### macOS / Linux - Nix

```bash
# Using nix-env
nix-env -iA nixpkgs.delta-cli

# Or using Nix Flakes
nix profile install nixpkgs#delta-cli
```

### Linux - Homebrew

```bash
# Using tap (before official inclusion)
brew install oderoi/delta-cli/delta-cli

# Or after official inclusion
brew install delta-cli
```

---

## Manual Installation

### Step 1: Download Pre-built Binary

1. Go to [GitHub Releases](https://github.com/oderoi/delta-cli/releases)
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
git clone https://github.com/oderoi/delta-cli.git
cd delta-cli
./packaging/build-scripts/build-macos.sh Release arm64
sudo cp build_macos_arm64/delta /usr/local/bin/delta
sudo cp build_macos_arm64/delta-server /usr/local/bin/delta-server
```

### Linux

```bash
git clone https://github.com/oderoi/delta-cli.git
cd delta-cli
./packaging/build-scripts/build-linux.sh Release x86_64
sudo cp build_linux_x86_64/delta /usr/local/bin/delta
sudo cp build_linux_x86_64/delta-server /usr/local/bin/delta-server
```

### Windows

```powershell
git clone https://github.com/oderoi/delta-cli.git
cd delta-cli
.\packaging\build-scripts\build-windows.ps1 Release x64
# Copy build_windows\Release\delta.exe to a directory in PATH
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
4. Check [GitHub Issues](https://github.com/oderoi/delta-cli/issues)

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

