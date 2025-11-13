# Quick Install - One Command

Install Delta CLI with a single command on any platform!

## 🍎 macOS

**Correct command (repository is `homebrew-delta-cli`):**
```bash
brew tap nile-agi/delta-cli && brew install delta
```

**Note:** Homebrew taps are git repositories, so `brew tap` will clone the tap repository. This is normal and required. The formula itself downloads pre-built binaries (no building).

## 🐧 Linux

### Debian/Ubuntu
```bash
curl -fsSL https://raw.githubusercontent.com/nile-agi/delta/main/packaging/linux/install-deb.sh | sudo bash
```

### RHEL/CentOS/Fedora
```bash
curl -fsSL https://raw.githubusercontent.com/nile-agi/delta/main/packaging/linux/install-rpm.sh | sudo bash
```

## 🪟 Windows

Open PowerShell as Administrator and run:
```powershell
Set-ExecutionPolicy Bypass -Scope Process -Force; Invoke-WebRequest -Uri "https://raw.githubusercontent.com/nile-agi/delta/main/packaging/windows/install.ps1" -OutFile install.ps1; .\install.ps1
```

## ✅ What Gets Installed

All installers automatically:
- ✅ Install minimal dependencies (only for downloading)
- ✅ Download pre-built binaries (no git, no building)
- ✅ Configure PATH automatically
- ✅ Handle conflicts with other `delta` commands
- ✅ Make `delta` available system-wide

## 🚀 After Installation

Just use:
```bash
delta --version
delta pull qwen2.5:0.5b
delta server
```

No configuration needed - it just works!

## 📚 More Options

See [INSTALL_SIMPLE.md](packaging/INSTALL_SIMPLE.md) for detailed installation options and troubleshooting.
