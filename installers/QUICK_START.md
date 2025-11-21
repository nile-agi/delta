# Quick Start: Creating a macOS DMG Installer

## One-Command Solution

```bash
cd delta
./installers/create_dmg.sh
```

That's it! The DMG will be created at:
```
installers/packages/DeltaCLI-<version>-macOS-<arch>.dmg
```

## What You Get

A professional .dmg installer containing:
- ✅ Delta CLI.app (macOS application bundle)
- ✅ Applications folder link (for drag-and-drop installation)
- ✅ README with installation instructions
- ✅ Web UI (if available)

## Distribution

1. **Upload to GitHub Releases:**
   - Go to your repository → Releases → Draft a new release
   - Upload the DMG file
   - Users can download directly

2. **Direct Download:**
   - Host the DMG on your website/CDN
   - Provide a download link
   - Example: `https://yourdomain.com/downloads/DeltaCLI-1.0.0-macOS-arm64.dmg`

3. **User Installation:**
   - User downloads the DMG
   - Double-clicks to mount
   - Drags "Delta CLI.app" to Applications
   - Done!

## Customization

**Set custom version:**
```bash
VERSION=1.2.3 ./installers/package_macos.sh
```

**Add custom background:**
- Place `background.png` in `installers/` directory
- Script will automatically use it

## Troubleshooting

**Build fails?**
- Install Xcode Command Line Tools: `xcode-select --install`
- Install CMake: `brew install cmake`
- Install Node.js (for web UI): `brew install node`

**DMG creation fails?**
- Make sure you've run `./installers/build_macos.sh` first
- Check available disk space

For more details, see [DMG_GUIDE.md](./DMG_GUIDE.md)

