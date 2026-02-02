# Installing Delta CLI from Local Project Folder

This guide shows you how to build and install Delta CLI directly from your local project folder.

> **Reinstalling and not seeing web UI changes?** (e.g. context length selection in Model Management)  
> See **[REINSTALL.md](REINSTALL.md)** — you must reinstall using the script so the built web UI from this repo is installed:  
> `./scripts/build-webui-and-install.sh`

## Prerequisites

- **macOS**: Xcode Command Line Tools, Homebrew (for dependencies)
- **Linux**: CMake, Git, build-essential, curl
- **Windows**: Visual Studio 2019+ or Build Tools, CMake

## Quick Installation (macOS)

If you're on macOS, use the automated script:

```bash
cd /path/to/delta
./install-macos.sh
```

This script will:
1. ✅ Check and install dependencies (CMake, Git, etc.)
2. ✅ Build Delta CLI in `build_macos/`
3. ✅ Install system-wide to `/usr/local/bin/`
4. ✅ Configure PATH automatically

## Manual Installation (All Platforms)

### Step 1: Navigate to Project Directory

```bash
cd /path/to/delta
```

### Step 2: Create Build Directory

**macOS:**
```bash
mkdir -p build_macos
cd build_macos
```

**Linux:**
```bash
mkdir -p build_linux
cd build_linux
```

**Windows:**
```powershell
mkdir build_windows
cd build_windows
```

### Step 3: Configure with CMake

**macOS (Apple Silicon with Metal):**
```bash
cmake .. \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=/usr/local \
    -DGGML_METAL=ON \
    -DBUILD_SERVER=ON
```

**macOS (Intel with Metal):**
```bash
cmake .. \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=/usr/local \
    -DGGML_METAL=ON \
    -DBUILD_SERVER=ON
```

**Linux:**
```bash
cmake .. \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=/usr/local \
    -DBUILD_SERVER=ON
```

**Windows (PowerShell):**
```powershell
cmake .. `
    -DCMAKE_BUILD_TYPE=Release `
    -DCMAKE_INSTALL_PREFIX=C:\Program Files\Delta `
    -DBUILD_SERVER=ON
```

### Step 4: Build

**macOS/Linux:**
```bash
cmake --build . --config Release -j$(sysctl -n hw.ncpu)  # macOS
# or
cmake --build . --config Release -j$(nproc)              # Linux
```

**Windows:**
```powershell
cmake --build . --config Release -j
```

### Step 5: Install

**macOS/Linux (requires sudo):**
```bash
sudo cmake --install . --prefix /usr/local
```

**Windows (run as Administrator):**
```powershell
cmake --install . --prefix "C:\Program Files\Delta"
```

### Step 6: Verify Installation

```bash
delta --version
delta-server --version
```

## Alternative: Install to User Directory (No sudo)

If you don't want to use `sudo`, install to your home directory:

### macOS/Linux:

```bash
# Build (same as above)
cd build_macos  # or build_linux
cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=$HOME/.local
cmake --build . --config Release -j

# Install (no sudo needed)
cmake --install . --prefix $HOME/.local

# Add to PATH (add to ~/.zshrc or ~/.bashrc)
export PATH="$HOME/.local/bin:$PATH"
```

Then restart your terminal or run:
```bash
source ~/.zshrc  # or source ~/.bashrc
```

## Quick Build & Run (Development)

For development, you can run directly from the build directory without installing:

```bash
# Build
cd build_macos  # or build_linux
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release -j

# Run directly
./delta --version
./delta-server --version

# Or run from project root
../build_macos/delta --version
```

## Keeping your web UI changes after reinstall

If you edit the web UI in `assets/` (e.g. Model Management, settings), those changes are only used when the **built** `public/` directory is installed with Delta. Reinstalling Delta in these ways does **not** use your local changes:

- **`brew reinstall delta-cli`** – Uses the formula’s source or bottle (upstream), not your clone.
- **Installing from a pre-built tarball** – The tarball contains whatever web UI was bundled at release time.

To install Delta so it uses **your** built web UI from this repo:

### Option A: Script (recommended)

From the repo root (with Node.js and npm installed):

```bash
./scripts/build-webui-and-install.sh
```

This will:

1. Build the web UI from `assets/` into `public/`
2. Configure and build the C++ app in `build/`
3. Install binaries and the web UI to `/usr/local` (or pass a prefix, e.g. `./scripts/build-webui-and-install.sh /opt/homebrew`)

After this, `delta` and `delta-server` will serve the web UI from your build.

### Option B: Manual steps

1. **Build the web UI** (required so your changes are in `public/`):

   ```bash
   cd assets
   npm install
   npm run build
   cd ..
   ```

2. **Build and install** as in Manual Installation above (create build dir, `cmake ..`, `cmake --build .`, then `sudo cmake --install .`).  
   CMake will install the existing `public/` to `share/delta-cli/webui`.

### Option C: Force CMake to rebuild the web UI

If you already have a `public/` from an older build and run `cmake` again, it may skip building the web UI. To force a fresh build:

```bash
cd build_macos   # or your build dir
cmake .. -DFORCE_BUILD_WEBUI=ON
cmake --build . --config Release -j
sudo cmake --install .
```

---

## Troubleshooting

### "CMake not found"
**macOS:**
```bash
brew install cmake
```

**Linux (Debian/Ubuntu):**
```bash
sudo apt-get install cmake
```

**Linux (Fedora/RHEL):**
```bash
sudo dnf install cmake
```

### "llama.cpp not found"
Make sure you're in the project root directory and that `vendor/llama.cpp/` exists. If not:
```bash
git submodule update --init --recursive
```

### "Permission denied" during install
Use `sudo` for system-wide installation, or install to `$HOME/.local` instead.

### "delta: command not found"
Add the installation directory to your PATH:
```bash
# For /usr/local/bin
export PATH="/usr/local/bin:$PATH"

# For $HOME/.local/bin
export PATH="$HOME/.local/bin:$PATH"
```

Then restart your terminal or run `source ~/.zshrc` (or `~/.bashrc`).

## Next Steps

After installation:

1. **Test the installation:**
   ```bash
   delta --version
   ```

2. **Download a model:**
   ```bash
   delta pull qwen3:0.6b
   ```

3. **Start using Delta:**
   ```bash
   delta
   ```

4. **Start the web server:**
   ```bash
   delta server
   ```

## Uninstalling

**System-wide installation:**
```bash
sudo rm /usr/local/bin/delta
sudo rm /usr/local/bin/delta-server
```

**User installation:**
```bash
rm $HOME/.local/bin/delta
rm $HOME/.local/bin/delta-server
```

