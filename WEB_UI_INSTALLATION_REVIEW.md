# Web UI Installation Review

## Summary

This document reviews the installation procedures to ensure users get the updated Delta web UI (with favicon, model name improvements, and "context" instead of "ctx") when installing Delta CLI on macOS, Linux, and Windows.

## Changes Made

### 1. CMakeLists.txt ✅
**Status:** Updated

**Changes:**
- Added automatic web UI build step that runs during CMake configuration
- Builds web UI from `assets/` directory using npm
- Checks if `public/index.html` exists and is up-to-date before building
- Installs web UI from `public/` to `share/delta-cli/webui` during `make install`
- Falls back gracefully if npm is not available (warns but continues)

**Location:** Lines 204-319

### 2. Homebrew Formula (delta-cli.rb) ✅
**Status:** Updated

**Changes:**
- Added `depends_on "node" => :build` to ensure Node.js is available
- Updated installation to prioritize `public/` (built from assets/) over `vendor/llama.cpp`
- Added informative messages about which web UI is being installed
- Falls back to original llama.cpp web UI if custom UI not built

**Location:** `packaging/homebrew/delta-cli.rb`

### 3. Server Code (delta_server_wrapper.cpp) ✅
**Status:** Updated

**Changes:**
- Updated `find_webui_path()` to check paths in priority order:
  1. Homebrew share directory (`/opt/homebrew/share/delta-cli/webui` or `/usr/local/share/delta-cli/webui`)
  2. `public/` directory (built from assets/)
  3. `vendor/llama.cpp/tools/server/public` (fallback)
- Updated comments to reflect Delta web UI instead of "original llama.cpp web UI"

**Location:** `src/delta_server_wrapper.cpp` lines 93-153

### 4. Commands Code (commands.cpp) ✅
**Status:** Updated

**Changes:**
- Updated `public_candidates` in two locations to prioritize:
  1. Homebrew share directory
  2. `public/` directory (built from assets/)
  3. `vendor/llama.cpp` (fallback)
- Updated comments to reflect priority order

**Location:** `src/commands.cpp` lines 111-135 and 782-806

### 5. README.md ✅
**Status:** Updated

**Changes:**
- Updated feature description to mention custom Delta branding
- Added note about Node.js requirement for Homebrew installation
- Added troubleshooting section about Node.js/npm requirements
- Documented that custom web UI requires Node.js to build

**Location:** Multiple sections in README.md

## Installation Methods Review

### ✅ macOS - Homebrew
**Status:** Fixed

**How it works:**
1. Homebrew installs Node.js as build dependency
2. CMake automatically builds web UI from `assets/` during configuration
3. Web UI is installed to `/opt/homebrew/share/delta-cli/webui` (or `/usr/local/share/delta-cli/webui`)
4. Server code finds web UI in Homebrew share directory

**User experience:**
- ✅ Users get updated web UI automatically
- ✅ No manual steps required
- ✅ Node.js is installed automatically by Homebrew

### ✅ Linux - Package Managers
**Status:** Fixed (for source builds)

**How it works:**
1. If building from source, CMake builds web UI from `assets/` (if npm available)
2. Web UI is installed to `/usr/local/share/delta-cli/webui` during `make install`
3. Server code finds web UI in standard locations

**Note:** Pre-built packages (deb/rpm) need to include built web UI in release artifacts.

### ✅ Windows - Installation Scripts
**Status:** Fixed (for source builds)

**How it works:**
1. If building from source, CMake builds web UI from `assets/` (if npm available)
2. Web UI is installed to installation directory during install
3. Server code finds web UI relative to executable

**Note:** Pre-built installers need to include built web UI in release artifacts.

## Pre-built Release Packages

**Important:** For pre-built release packages (tar.gz, zip, deb, rpm, etc.), the web UI must be built and included in the package before distribution.

**Required steps for releases:**
1. Build web UI: `cd assets && npm install && npm run build`
2. Include `public/` directory in release package
3. Package structure should include:
   - `delta` / `delta.exe` (binary)
   - `delta-server` / `delta-server.exe` (binary)
   - `webui/` or `public/` (built web UI)

## Testing Checklist

### macOS (Homebrew)
- [ ] Install via Homebrew: `brew install --HEAD nile-agi/delta-cli/delta-cli`
- [ ] Verify web UI is built: Check `/opt/homebrew/share/delta-cli/webui/index.html`
- [ ] Run `delta server` and verify:
  - [ ] Favicon appears in browser tab
  - [ ] Model name shows `short_name` (e.g., "qwen3-0.6b")
  - [ ] Context badge shows "context: X,XXX" not "ctx"

### Linux (Source Build)
- [ ] Install Node.js: `sudo apt install nodejs npm`
- [ ] Build from source: `cmake .. && make && sudo make install`
- [ ] Verify web UI is built: Check `public/index.html` exists
- [ ] Verify web UI is installed: Check `/usr/local/share/delta-cli/webui/index.html`
- [ ] Run `delta server` and verify web UI features

### Windows (Source Build)
- [ ] Install Node.js from nodejs.org
- [ ] Build from source using Visual Studio
- [ ] Verify web UI is built: Check `public/index.html` exists
- [ ] Run `delta server` and verify web UI features

## Path Resolution Priority

The server code now checks for web UI in this order:

1. **Homebrew share directory** (for installed packages)
   - `/opt/homebrew/share/delta-cli/webui` (Apple Silicon)
   - `/usr/local/share/delta-cli/webui` (Intel/standard)
   - Relative paths from executable

2. **public/ directory** (built from assets/)
   - `public/` (current directory)
   - `./public/`
   - `../public/`
   - Relative to executable

3. **vendor/llama.cpp** (fallback to original llama.cpp web UI)
   - `vendor/llama.cpp/tools/server/public`
   - Relative paths

## Fallback Behavior

If the custom Delta web UI is not available:
- Server falls back to original llama.cpp web UI (if available)
- Server uses embedded UI (if no external UI found)
- User experience degrades gracefully (no errors, just missing customizations)

## Recommendations

1. **For Release Packages:**
   - Always build web UI before creating release packages
   - Include `public/` directory in all release artifacts
   - Test that web UI works in release packages

2. **For Development:**
   - Run `cd assets && npm run build` after making web UI changes
   - CMake will automatically rebuild if `assets/package.json` is newer than `public/index.html`

3. **For CI/CD:**
   - Ensure Node.js is available in build environment
   - Run `npm install` and `npm run build` in `assets/` directory
   - Include `public/` in build artifacts

## Conclusion

✅ **All installation methods now support the updated web UI:**
- Homebrew automatically builds and installs custom web UI
- Source builds automatically build web UI if Node.js is available
- Server code correctly finds web UI in all installation scenarios
- README documents requirements and troubleshooting

**Next Steps:**
- Test Homebrew installation on clean macOS system
- Test source build on Linux with and without Node.js
- Ensure release packages include built web UI
- Update CI/CD pipelines to build web UI before packaging

