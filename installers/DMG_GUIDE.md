# macOS DMG Installer Guide

This guide explains how to create a .dmg installer for Delta CLI on macOS, making it easy for users to download and install your application.

## Quick Start

To create a DMG installer in one step:

```bash
cd delta
./installers/create_dmg.sh
```

The DMG will be created at: `installers/packages/DeltaCLI-<version>-macOS-<arch>.dmg`

## Step-by-Step Process

### 1. Build the Application

First, build the release version of Delta CLI:

```bash
./installers/build_macos.sh
```

This will:
- Check for dependencies (Xcode Command Line Tools, CMake, etc.)
- Build the web UI from the `assets/` directory
- Compile the Delta CLI binary with Metal acceleration
- Create binaries in `build_macos_release/`

### 2. Create the DMG

Once the build is complete, create the DMG installer:

```bash
./installers/package_macos.sh
```

This will:
- Create a macOS application bundle (`.app`)
- Package the binaries and web UI
- Create a DMG with:
  - Delta CLI.app (the application)
  - Applications folder symlink (for easy installation)
  - README.txt (installation instructions)
- Output the DMG to `installers/packages/`

## What's in the DMG?

The DMG contains:

1. **Delta CLI.app** - A macOS application bundle containing:
   - `delta` binary (main CLI executable)
   - `delta-server` binary (if built)
   - Web UI files (if available)
   - Info.plist (application metadata)

2. **Applications** - A symlink to `/Applications` for easy drag-and-drop installation

3. **README.txt** - Installation and usage instructions

## Distribution

### Hosting the DMG

1. **GitHub Releases** (Recommended):
   - Create a new release on GitHub
   - Upload the DMG file as an asset
   - Users can download directly from the release page

2. **Direct Download Link**:
   - Upload to your web server or CDN
   - Provide a direct download link
   - Example: `https://yourdomain.com/downloads/DeltaCLI-1.0.0-macOS-arm64.dmg`

3. **Package Managers**:
   - For Homebrew: Update the formula to download from your release URL
   - The DMG can be used as an alternative to Homebrew installation

### User Installation Process

1. User downloads the DMG file
2. User double-clicks the DMG to mount it
3. User drags "Delta CLI.app" to the Applications folder
4. User can now run `delta` from Terminal (if PATH is configured) or double-click the app

## Customization

### Setting a Custom Version

```bash
VERSION=1.2.3 ./installers/package_macos.sh
```

### Adding a Custom Background

1. Create a background image (recommended: 600x400px PNG)
2. Save it as `installers/background.png`
3. The packaging script will automatically use it

### Customizing the App Bundle

Edit `installers/package_macos.sh` to modify:
- App name
- Bundle identifier
- Minimum macOS version
- App icon (add to `APP_BUNDLE/Contents/Resources/`)

## Troubleshooting

### Build Fails

**Error: "Xcode Command Line Tools not found"**
```bash
xcode-select --install
```

**Error: "CMake not found"**
```bash
brew install cmake
```

**Error: "npm not found"** (for web UI)
```bash
brew install node
```

### DMG Creation Fails

**Error: "Build not found"**
- Make sure you've run `./installers/build_macos.sh` first
- Check that `build_macos_release/delta` exists

**Error: "hdiutil failed"**
- Check available disk space
- Try running with `sudo` (though it shouldn't be necessary)

### DMG Doesn't Open on User's Mac

**Gatekeeper Issues:**
- The app may need to be code-signed for distribution
- For unsigned apps, users need to:
  1. Right-click the DMG → Open
  2. Or: System Preferences → Security & Privacy → Allow

**Code Signing (Optional but Recommended):**

To code sign the app bundle:

```bash
# Sign the app
codesign --deep --force --verify --verbose \
  --sign "Developer ID Application: Your Name" \
  installers/packages/DeltaCLI.app

# Sign the DMG
codesign --force --verify --verbose \
  --sign "Developer ID Application: Your Name" \
  installers/packages/DeltaCLI-1.0.0-macOS-arm64.dmg
```

**Notarization (For Distribution Outside App Store):**

```bash
# Notarize the DMG
xcrun notarytool submit \
  --apple-id "your@email.com" \
  --team-id "YOUR_TEAM_ID" \
  --password "app-specific-password" \
  installers/packages/DeltaCLI-1.0.0-macOS-arm64.dmg
```

## File Structure

After running the scripts, you'll have:

```
delta/
├── build_macos_release/          # Build output
│   ├── delta                      # Main binary
│   └── delta-server               # Server binary (optional)
├── installers/
│   ├── packages/                  # DMG output directory
│   │   └── DeltaCLI-1.0.0-macOS-arm64.dmg
│   ├── build_macos.sh            # Build script
│   ├── package_macos.sh          # DMG creation script
│   └── create_dmg.sh             # All-in-one script
└── public/                        # Web UI (built from assets/)
```

## Best Practices

1. **Version Management**: Always update the version in `CMakeLists.txt` before creating a release
2. **Testing**: Test the DMG on a clean macOS system before distribution
3. **Documentation**: Keep the README.txt in the DMG up-to-date
4. **Code Signing**: For public distribution, consider code signing and notarization
5. **Architecture**: Create separate DMGs for Intel (x86_64) and Apple Silicon (arm64) if needed

## Advanced: Universal Binary

To create a universal binary (Intel + Apple Silicon):

1. Build for both architectures:
```bash
# Build for arm64
arch -arm64 ./installers/build_macos.sh

# Build for x86_64 (on Intel Mac or via Rosetta)
arch -x86_64 ./installers/build_macos.sh
```

2. Create a universal binary:
```bash
lipo -create \
  build_macos_release_arm64/delta \
  build_macos_release_x86_64/delta \
  -output build_macos_release/delta
```

3. Package as usual:
```bash
./installers/package_macos.sh
```

## Support

For issues or questions:
- Check the main README.md
- Visit: https://github.com/oderoi/delta-cli/issues

