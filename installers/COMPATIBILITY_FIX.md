# M1/M2 Compatibility Fix

## Issue
When installing a DMG built on M1 Mac on an M2 Mac, you may see:
```
You can't use this version of the application "Delta CLI" with this version of macOS.
```

## Solution
The packaging script has been updated to:

1. **Set minimum macOS version to 11.0** (macOS Big Sur) - compatible with all Apple Silicon Macs
2. **Specify architecture priority** - arm64 first (for M1/M2/M3), then x86_64 (for Intel)
3. **Remove quarantine attributes** - allows the app to run without Gatekeeper blocking
4. **Verify binary architecture** - ensures the binary is arm64 compatible

## What Changed

### Info.plist Updates:
- `LSMinimumSystemVersion`: Set to `11.0` (was `10.15`)
- `LSArchitecturePriority`: Added to specify arm64 and x86_64 support
- `LSRequiresIPhoneOS`: Set to `false` (ensures it's treated as macOS app)

### Binary Handling:
- Architecture verification before packaging
- Quarantine attributes removed
- Proper executable permissions set

## Building for Universal Compatibility

The app built on M1 will work on:
- ✅ M1 Macs (arm64)
- ✅ M2 Macs (arm64) 
- ✅ M3 Macs (arm64)
- ✅ Intel Macs (if you build a universal binary)

## If Issues Persist

1. **Check the binary architecture:**
   ```bash
   file installers/packages/DeltaCLI-*.dmg/Delta\ CLI.app/Contents/MacOS/delta
   ```
   Should show: `arm64` or `universal binary`

2. **Check macOS version on target Mac:**
   ```bash
   sw_vers
   ```
   Must be macOS 11.0 (Big Sur) or later

3. **Remove and reinstall:**
   ```bash
   # On the M2 Mac
   rm -rf /Applications/Delta\ CLI.app
   # Then reinstall from the DMG
   ```

4. **Check Gatekeeper:**
   ```bash
   # On the M2 Mac, if blocked (for newer macOS)
   xattr -cr /Applications/Delta\ CLI.app
   
   # Or for older macOS versions:
   find /Applications/Delta\ CLI.app -exec xattr -c {} \;
   ```

## Building Universal Binaries (Optional)

To support both Intel and Apple Silicon in one DMG:

1. Build for both architectures separately
2. Create a universal binary using `lipo`
3. Package the universal binary

This is more complex but provides maximum compatibility.

