# Installation Without Git

All installers now use **pre-built binaries only** - no git, no cloning, no building required!

## 🍎 macOS

```bash
brew tap nile-agi/delta && brew install delta
```

**What it does:**
- ✅ Downloads pre-built binary from GitHub releases
- ✅ No git, no cloning, no building
- ✅ Installs in seconds
- ✅ Automatically configures PATH

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
- ✅ Only installs minimal dependencies (curl, wget, tar)
- ✅ Downloads pre-built binary from GitHub releases
- ✅ No git, no cloning, no building
- ✅ Automatically configures PATH

## 🪟 Windows

```powershell
Set-ExecutionPolicy Bypass -Scope Process -Force; Invoke-WebRequest -Uri "https://raw.githubusercontent.com/nile-agi/delta/main/packaging/windows/install.ps1" -OutFile install.ps1; .\install.ps1
```

**What it does:**
- ✅ Uses PowerShell built-in features (no extra dependencies)
- ✅ Downloads pre-built binary from GitHub releases
- ✅ No git, no cloning, no building
- ✅ Automatically configures PATH

## ✅ What You Get

All installers:
- **No git required** - Downloads pre-built binaries
- **No cloning** - Direct download from releases
- **No building** - Pre-compiled binaries ready to use
- **Fast installation** - Seconds instead of minutes
- **Automatic PATH** - Works immediately after install

## 📦 Requirements

**macOS:**
- Homebrew (installed automatically if needed)

**Linux:**
- curl or wget (usually pre-installed)
- tar (usually pre-installed)
- sudo access

**Windows:**
- PowerShell (built-in)
- Administrator privileges

## 🚀 After Installation

Just use:
```bash
delta --version
delta pull qwen2.5:0.5b
delta server
```

No configuration needed!

