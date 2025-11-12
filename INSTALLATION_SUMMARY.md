# Delta CLI Installation - Complete Guide

## 🎯 For Regular Users (No Git/GitHub Knowledge Required)

### ✅ Easiest Method: Installation Script

**macOS / Linux:**
```bash
curl -fsSL https://raw.githubusercontent.com/nile-agi/delta/main/install.sh | bash
```

**Windows:**
```powershell
Invoke-WebRequest -Uri https://raw.githubusercontent.com/nile-agi/delta/main/install.ps1 -OutFile install.ps1
.\install.ps1
```

**What this does:**
- ✅ Downloads pre-built binaries (no compilation needed)
- ✅ No git, GitHub, or development tools required
- ✅ Automatically detects your platform
- ✅ Installs everything for you
- ✅ Works for everyone, including non-developers

### Alternative: Package Managers

**macOS - Homebrew:**
```bash
brew tap nile-agi/delta-cli
brew install --HEAD delta-cli
```

**macOS - MacPorts:**
```bash
sudo port install delta-cli
```

**Windows - Winget:**
```powershell
winget install DeltaCLI.DeltaCLI
```

## 🔧 For Developers

If you want to build from source:

```bash
git clone --recurse-submodules https://github.com/nile-agi/delta.git
cd delta
./packaging/build-scripts/build-macos.sh Release arm64
sudo cp build_macos_arm64/delta /usr/local/bin/delta
```

## ✅ Verification

After installation:
```bash
delta --version
```

Expected output: `Delta CLI v1.0.0`

## 📚 Next Steps

1. **Download a model:**
   ```bash
   delta pull qwen2.5:0.5b
   ```

2. **Start using:**
   ```bash
   delta server    # Web interface
   # OR
   delta           # Interactive CLI
   ```

## 🎉 That's It!

No git, no GitHub, no development knowledge needed. Just install and use!

