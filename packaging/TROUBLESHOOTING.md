# Troubleshooting Installation Issues

## macOS Homebrew Installation

### Error: Build Tools Required

**Error Message:**
```
Error: The following flag:
  --HEAD
requires building tools, but none are installed.
Install the Command Line Tools:
  xcode-select --install
```

**Problem:** The `--HEAD` flag in Homebrew builds from source, which requires Xcode Command Line Tools.

**Solutions:**

#### Solution 1: Install Xcode Command Line Tools (Recommended for Source Builds)

```bash
xcode-select --install
```

This will open a dialog to install the Command Line Tools. After installation completes, try the Homebrew installation again:

```bash
brew tap nile-agi/delta-cli && brew install --HEAD nile-agi/delta-cli/delta-cli
```

#### Solution 2: Use Pre-built Binary (No Build Tools Required)

If you don't want to install build tools, use the installation script instead:

```bash
curl -fsSL https://raw.githubusercontent.com/nile-agi/delta/main/install.sh | bash
```

This downloads and installs pre-built binaries without requiring any build tools.

#### Solution 3: Wait for Stable Release

Once a stable release is available, you'll be able to install without `--HEAD`:

```bash
brew tap nile-agi/delta-cli && brew install nile-agi/delta-cli/delta-cli
```

(Note: This requires the formula to be updated with a stable release URL and SHA256)

### Other Common Issues

#### PATH Issues

If `delta` command is not found after installation:

```bash
# Reload your shell configuration
source ~/.zshrc  # for zsh
# or
source ~/.bash_profile  # for bash

# Or restart your terminal
```

#### Conflicts with Other `delta` Commands

If you have another `delta` command (like from llvm), the installer creates an alias. Make sure to reload your shell:

```bash
source ~/.zshrc
```

You can also use the full path:
```bash
/opt/homebrew/bin/delta
```

## Linux Installation

### Missing Dependencies

If installation fails, make sure you have required packages:

**Debian/Ubuntu:**
```bash
sudo apt-get update
sudo apt-get install -y curl wget tar
```

**RHEL/CentOS/Fedora:**
```bash
sudo yum install -y curl wget tar
# or for newer versions
sudo dnf install -y curl wget tar
```

## Windows Installation

### PowerShell Execution Policy

If you get an execution policy error:

```powershell
Set-ExecutionPolicy -ExecutionPolicy RemoteSigned -Scope CurrentUser
```

### Missing Dependencies

Make sure you have:
- PowerShell 5.1 or later
- Administrator privileges (for system-wide installation)

## General Troubleshooting

### Verify Installation

After installation, verify it works:

```bash
delta --version
```

### Check Installation Location

**macOS (Homebrew):**
```bash
which delta
# Should show: /opt/homebrew/bin/delta
```

**Linux:**
```bash
which delta
# Should show: /usr/local/bin/delta
```

**Windows:**
```powershell
where.exe delta
# Should show: C:\Program Files\Delta CLI\delta.exe
```

### Reinstall

If you need to reinstall:

**macOS (Homebrew):**
```bash
brew uninstall delta-cli
brew tap nile-agi/delta-cli && brew install --HEAD nile-agi/delta-cli/delta-cli
```

**Linux/Windows:**
Run the installation script again - it will overwrite the existing installation.

## Getting Help

If you continue to experience issues:

1. Check the [main README](../README.md) for detailed installation instructions
2. Check [INSTALL_SIMPLE.md](INSTALL_SIMPLE.md) for alternative installation methods
3. Open an issue on GitHub: https://github.com/nile-agi/delta/issues

