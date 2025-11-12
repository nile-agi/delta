# Delta CLI Installation - Final Guide

## 🎯 For Regular Users (No Git/GitHub Required)

### ✅ Recommended: Installation Script

**One command installs everything:**
```bash
curl -fsSL https://raw.githubusercontent.com/nile-agi/delta/main/install.sh | bash
```

**What this does:**
- ✅ Downloads pre-built binaries (no compilation)
- ✅ No git, GitHub, or development tools needed
- ✅ Works for everyone, including non-developers
- ✅ Automatically sets up everything

**After installation:**
```bash
# Verify installation
delta --version

# If you see an error about "python" or wrong delta:
# The PATH might have another 'delta' command first
# Check with: which -a delta
# Our delta should be at: /usr/local/bin/delta
```

## 📦 Package Manager Installation

### macOS - Homebrew

**After the fix is pushed:**
```bash
brew tap nile-agi/delta-cli
brew install --HEAD delta-cli
```

**Note:** This builds from source and requires git. For faster installation, use the script above.

## ⚠️ PATH Conflicts

If you have another `delta` command installed (like git-delta or llvm-delta):

```bash
# Check which delta is being used
which -a delta

# Use full path to our delta
/usr/local/bin/delta --version

# Or add /usr/local/bin to the front of PATH
export PATH="/usr/local/bin:$PATH"
echo 'export PATH="/usr/local/bin:$PATH"' >> ~/.zshrc
source ~/.zshrc
```

## 🔧 Homebrew Formula Fix

The formula has been updated to:
- ✅ Remove automatic submodule processing
- ✅ Manually handle only llama.cpp submodule
- ✅ Avoid nested repository issues

**To push the fix:**
```bash
cd /Users/suzanodero/io/GITHUB/delta/homebrew-delta-cli
git push origin main
```

## ✅ Verification

After installation:
```bash
# Should show: Delta CLI v1.0.0
delta --version

# If wrong delta shows up, use full path:
/usr/local/bin/delta --version
```

## 📚 Quick Start

1. **Install:**
   ```bash
   curl -fsSL https://raw.githubusercontent.com/nile-agi/delta/main/install.sh | bash
   ```

2. **Download a model:**
   ```bash
   delta pull qwen2.5:0.5b
   ```

3. **Start using:**
   ```bash
   delta server    # Web interface
   # OR
   delta           # Interactive CLI
   ```

## 🎉 That's It!

No git, no GitHub, no technical knowledge needed. Just install and use!

