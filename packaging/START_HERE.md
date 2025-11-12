# 🚀 START HERE - Using the Packaging Scripts

## The Simplest Way to Use These Scripts

### Step 1: Build Delta CLI

```bash
cd /Users/suzanodero/Downloads/delta-cli
./packaging/build-scripts/build-macos.sh Release arm64
```

**Wait for:** `✅ Build completed successfully!`

### Step 2: Test It Works

```bash
./build_macos_arm64/delta --version
```

**You should see:** Version information

### Step 3: Create Release Package

```bash
./packaging/release/package-macos.sh 1.0.0 arm64
```

**Wait for:** `✅ Package created: release/delta-cli-macos-arm64.tar.gz`

**Copy the SHA256 hash** shown in the output!

### Step 4: Check Your Package

```bash
ls -lh release/
```

**You should see:** `delta-cli-macos-arm64.tar.gz`

---

## That's It! 🎉

You now have:
- ✅ Built Delta CLI binary
- ✅ Created release package
- ✅ Got SHA256 hash for package managers

---

## What Each Script Does

### Build Scripts (Create the Binary)

| Script | What It Does |
|--------|-------------|
| `build-macos.sh Release arm64` | Builds Delta CLI for Apple Silicon Macs |
| `build-macos.sh Release x86_64` | Builds Delta CLI for Intel Macs |
| `build-linux.sh Release x86_64` | Builds Delta CLI for Linux |
| `build-windows.ps1 Release x64` | Builds Delta CLI for Windows |

### Package Scripts (Create Distribution Files)

| Script | What It Does |
|--------|-------------|
| `package-macos.sh 1.0.0 arm64` | Creates `.tar.gz` file for macOS ARM64 |
| `package-linux.sh 1.0.0 x86_64` | Creates `.tar.gz` file for Linux |
| `package-windows.ps1 1.0.0 x64` | Creates `.zip` file for Windows |
| `package-all.sh 1.0.0` | Packages all platforms you've built |

---

## Common Questions

### Q: What if the script says "Permission denied"?

```bash
chmod +x packaging/build-scripts/*.sh
chmod +x packaging/release/*.sh
```

### Q: Where are the built files?

- **Build output:** `build_macos_arm64/delta` and `build_macos_arm64/delta-server`
- **Packages:** `release/delta-cli-macos-arm64.tar.gz`

### Q: How do I build for a different platform?

Just change the architecture:
- `arm64` = Apple Silicon (M1/M2/M3)
- `x86_64` = Intel Macs or Linux x86_64
- `aarch64` = Linux ARM64

### Q: What do I do with the SHA256 hash?

Update these files:
- `packaging/homebrew/delta-cli.rb` (replace `PLACEHOLDER_SHA256`)
- `packaging/winget/delta-cli.yaml` (replace `PLACEHOLDER_SHA256`)
- `packaging/nix/default.nix` (replace `PLACEHOLDER_SHA256`)

---

## Next Steps

1. ✅ **Build** - You did this!
2. ✅ **Package** - You did this!
3. 📝 **Update package definitions** with SHA256
4. 📤 **Create GitHub release** and upload packages
5. 🚀 **Submit to package managers**

For detailed instructions, see:
- `EXAMPLES.md` - More examples
- `HOW_TO_USE.md` - Complete guide
- `README.md` - Full documentation

---

## Quick Reference Card

```bash
# BUILD
./packaging/build-scripts/build-macos.sh Release arm64

# TEST
./build_macos_arm64/delta --version

# PACKAGE
./packaging/release/package-macos.sh 1.0.0 arm64

# CHECK
ls -lh release/
```

That's all you need to know to get started! 🎯

