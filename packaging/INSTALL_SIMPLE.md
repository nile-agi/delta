# Simple Installation - One Command

Delta CLI can now be installed with a single command on all platforms!

## 🍎 macOS (Homebrew)

### Option 1: Install from Source (Requires Build Tools)

```bash
brew tap nile-agi/delta-cli && brew install --HEAD nile-agi/delta-cli/delta-cli
```

**⚠️ Important:** This requires Xcode Command Line Tools. If you get an error about missing build tools:

```bash
# Install Xcode Command Line Tools first
xcode-select --install

# Then try the installation again
brew tap nile-agi/delta-cli && brew install --HEAD nile-agi/delta-cli/delta-cli
```

**What it does:**
- ✅ Automatically clones the repository (git happens in background)
- ✅ Installs all dependencies automatically
- ✅ Builds and installs Delta CLI automatically
- ✅ Configures PATH automatically
- ✅ Creates alias to override conflicting commands

**Note:** Installation takes a few minutes to build, but everything is automatic.

### Option 2: Install Pre-built Binary (No Build Tools Required)

If you don't have Xcode Command Line Tools installed, you can use the installation script instead:

```bash
curl -fsSL https://raw.githubusercontent.com/nile-agi/delta/main/install.sh | bash
```

This downloads and installs pre-built binaries without requiring build tools.

After installation, just use:
```bash
delta --version
delta pull qwen2.5:0.5b
delta server
```

## 🐧 Linux

### Debian/Ubuntu

```bash
curl -fsSL https://raw.githubusercontent.com/nile-agi/delta/main/packaging/linux/install-deb.sh | sudo bash
```

### RHEL/CentOS/Fedora

```bash
curl -fsSL https://raw.githubusercontent.com/nile-agi/delta/main/packaging/linux/install-rpm.sh | sudo bash
```

**What it does:**
- ✅ Installs all dependencies (cmake, git, curl, etc.)
- ✅ Downloads and installs Delta CLI
- ✅ Configures system-wide PATH
- ✅ Works immediately after installation

## 🪟 Windows

### PowerShell (as Administrator)

```powershell
Set-ExecutionPolicy Bypass -Scope Process -Force; Invoke-WebRequest -Uri "https://raw.githubusercontent.com/nile-agi/delta/main/packaging/windows/install.ps1" -OutFile install.ps1; .\install.ps1
```

**What it does:**
- ✅ Installs Chocolatey (if needed)
- ✅ Installs all dependencies (cmake, git, curl)
- ✅ Downloads and installs Delta CLI
- ✅ Adds to system PATH automatically
- ✅ Creates desktop shortcut

## 📦 Alternative: Package Managers

### macOS - Homebrew (Tap)
```bash
brew tap nile-agi/delta
brew install delta
```

### Windows - Winget
```bash
winget install DeltaCLI.DeltaCLI
```

### Linux - Manual Download
```bash
# Download and extract
wget https://github.com/nile-agi/delta/releases/latest/download/delta-cli-linux-x86_64.tar.gz
tar -xzf delta-cli-linux-x86_64.tar.gz
sudo cp delta-cli-*/delta delta-cli-*/delta-server /usr/local/bin/
```

## ✅ Verification

After installation, verify it works:

```bash
delta --version
# Should show: Delta CLI v1.0.0
```

## 🔧 Troubleshooting

### Build Tools Error on macOS

If you see this error when using `--HEAD`:
```
Error: The following flag: --HEAD requires building tools, but none are installed.
```

**Solution 1: Install Xcode Command Line Tools**
```bash
xcode-select --install
```

**Solution 2: Use Pre-built Binary Instead**
```bash
# Skip Homebrew and use the installation script
curl -fsSL https://raw.githubusercontent.com/nile-agi/delta/main/install.sh | bash
```

### PATH Issues

If `delta` command is not found:

**macOS:**
```bash
source ~/.zshrc  # or restart terminal
# Or use: /opt/homebrew/bin/delta
```

**Linux:**
```bash
source /etc/profile.d/delta-cli.sh
# Or restart terminal
```

**Windows:**
- Restart PowerShell/Command Prompt
- Or use full path: `C:\Program Files\Delta CLI\delta.exe`

### Conflicts with Other `delta` Commands

The installers automatically configure PATH to prioritize Delta CLI. If you still have issues:

**macOS:**
- An alias is created: `alias delta='/opt/homebrew/bin/delta'`
- Run `source ~/.zshrc` to activate

**Linux/Windows:**
- `/usr/local/bin` (Linux) or `C:\Program Files\Delta CLI` (Windows) is added to PATH first

## 🎉 That's It!

No git, no GitHub, no technical knowledge needed. Just install and use!

