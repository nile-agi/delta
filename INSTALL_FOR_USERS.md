# Installing Delta CLI - For End Users (No Git/GitHub Required)

This guide is for **regular users** who just want to install and use Delta CLI without needing to know about git, GitHub, or development tools.

## 🚀 Quick Installation (Recommended)

### macOS / Linux

Simply run this one command:

```bash
curl -fsSL https://raw.githubusercontent.com/nile-agi/delta/main/install.sh | bash
```

That's it! The script will:
- ✅ Automatically detect your platform
- ✅ Download the pre-built binary
- ✅ Install delta and delta-server
- ✅ Set up everything for you

### Windows

```powershell
Invoke-WebRequest -Uri https://raw.githubusercontent.com/nile-agi/delta/main/install.ps1 -OutFile install.ps1
.\install.ps1
```

## 📦 Package Manager Installation (Easiest)

### macOS - Homebrew

```bash
# Tap the repository
brew tap nile-agi/delta-cli

# Install
brew install --HEAD delta-cli
```

**Note:** This builds from source. For faster installation, use the installation script above.

### macOS - MacPorts

```bash
sudo port install delta-cli
```

### Linux - Homebrew

```bash
# Install Homebrew for Linux first (if not installed)
/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"

# Tap and install
brew tap nile-agi/delta-cli
brew install --HEAD delta-cli
```

### Windows - Winget

```powershell
winget install DeltaCLI.DeltaCLI
```

## 📥 Manual Installation (Download Pre-built Binary)

If you prefer to download and install manually:

### Step 1: Download

1. Go to: https://github.com/nile-agi/delta/releases
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
sudo cp delta-cli-*/delta /usr/local/bin/delta
sudo cp delta-cli-*/delta-server /usr/local/bin/delta-server
sudo chmod +x /usr/local/bin/delta /usr/local/bin/delta-server
```

**Windows:**
```powershell
# Copy to Program Files
Copy-Item delta-cli\* "C:\Program Files\Delta CLI\"

# Add to PATH (requires admin)
[Environment]::SetEnvironmentVariable("Path", "$env:Path;C:\Program Files\Delta CLI", "Machine")
```

### Step 4: Verify

```bash
delta --version
```

You should see: `Delta CLI v1.0.0`

## ✅ What You Get

After installation, you'll have:
- `delta` - The main CLI application
- `delta-server` - The web server component
- Original llama.cpp web UI (automatically included)

## 🎯 First Steps

1. **Check version:**
   ```bash
   delta --version
   ```

2. **Download a model:**
   ```bash
   delta pull qwen2.5:0.5b
   ```

3. **Start the server:**
   ```bash
   delta server
   ```

4. **Or use interactive mode:**
   ```bash
   delta
   ```

## ❓ Troubleshooting

### "Command not found"

**macOS / Linux:**
```bash
# Add to PATH
export PATH="/usr/local/bin:$PATH"

# Make permanent (add to ~/.zshrc or ~/.bashrc)
echo 'export PATH="/usr/local/bin:$PATH"' >> ~/.zshrc
source ~/.zshrc
```

**Windows:**
- Restart your terminal after installation
- Or manually add `C:\Program Files\Delta CLI` to your PATH

### Installation Script Fails

1. Check your internet connection
2. Try downloading manually from GitHub Releases
3. Check [GitHub Issues](https://github.com/nile-agi/delta/issues) for known problems

### Permission Denied

```bash
# Make executable
chmod +x delta delta-server

# Or install with sudo
sudo ./install.sh
```

## 📚 More Information

- **Main README:** https://github.com/nile-agi/delta
- **Installation Guide:** https://github.com/nile-agi/delta/blob/main/INSTALL.md
- **Quick Start:** https://github.com/nile-agi/delta/blob/main/QUICK_START.md

## 🎉 That's It!

You don't need to know anything about git, GitHub, or development tools. Just install and use Delta CLI!

