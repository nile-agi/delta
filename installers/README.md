# Delta CLI Installers

This directory contains build scripts and packaging tools for various platforms.

## Building from Source

### Linux
```bash
./installers/build_linux.sh
```

Optional: Create `.deb` package
```bash
./installers/package_linux_deb.sh
```

### macOS
```bash
./installers/build_macos.sh
```

Optional: Create `.dmg` installer
```bash
./installers/package_macos.sh
```

### Windows
```cmd
installers\build_windows.bat
```

Optional: Create installer
```cmd
installers\package_windows.bat
```

### Android
```bash
export ANDROID_NDK=/path/to/ndk
./installers/build_android.sh
```

### iOS
```bash
./installers/build_ios.sh
```

## Build Options

### Linux/macOS Environment Variables
- `BUILD_TYPE`: `Release` or `Debug` (default: `Release`)
- `ENABLE_CUDA`: `ON` or `OFF` (default: `OFF`)
- `ENABLE_VULKAN`: `ON` or `OFF` (default: `OFF`)
- `INSTALL_PREFIX`: Installation path (default: `/usr/local`)

Example:
```bash
BUILD_TYPE=Release ENABLE_CUDA=ON ./installers/build_linux.sh
```

### Android Environment Variables
- `ANDROID_NDK`: Path to Android NDK
- `ANDROID_ABI`: `arm64-v8a` or `armeabi-v7a` (default: `arm64-v8a`)
- `ANDROID_PLATFORM`: Minimum API level (default: `21`)

### iOS Environment Variables
- `IOS_PLATFORM`: `OS64` (device) or `SIMULATOR64` (default: `OS64`)

## Platform-Specific Notes

### Linux
- Requires: CMake 3.14+, GCC 7+ or Clang 5+
- Optional: libcurl-dev, uuid-dev
- For CUDA: CUDA Toolkit 11+
- For Vulkan: Vulkan SDK

### macOS
- Requires: CMake 3.14+, Xcode Command Line Tools
- Metal acceleration enabled by default on Apple Silicon
- Recommended: Install via Homebrew: `brew install cmake`

### Windows
- Requires: CMake 3.14+, Visual Studio 2019+
- Run build script from Visual Studio Developer Command Prompt
- Optional: CUDA Toolkit for GPU acceleration
- Optional: NSIS for creating installers

### Android
- Requires: Android NDK (r21+), CMake 3.14+
- Set `ANDROID_NDK` environment variable
- Supports ARM64 and ARM architectures
- Minimum API level: 21 (Android 5.0)

### iOS
- Requires: Xcode 12+, CMake 3.14+
- Metal acceleration enabled by default
- Minimum deployment target: iOS 14.0
- Requires proper signing for device deployment

## Raspberry Pi / Edge Devices

### Raspberry Pi 4/5
```bash
./installers/build_linux.sh
```

### NVIDIA Jetson
```bash
BUILD_TYPE=Release ENABLE_CUDA=ON ./installers/build_linux.sh
```

### Other ARM Devices
```bash
./installers/build_linux.sh
```

## Cross-Compilation

For cross-compilation, specify the appropriate toolchain file:

```bash
cmake .. -DCMAKE_TOOLCHAIN_FILE=/path/to/toolchain.cmake
```

## Creating Custom Installers

### Debian/Ubuntu (.deb)
1. Build the project: `./installers/build_linux.sh`
2. Create package: `./installers/package_linux_deb.sh`
3. Install: `sudo dpkg -i installers/packages/*.deb`

### macOS (.dmg)

**Quick Start (All-in-One):**
```bash
./installers/create_dmg.sh
```
This will build the application and create a .dmg installer in one step.

**Step-by-Step:**
1. Build the project: `./installers/build_macos.sh`
2. Create DMG: `./installers/package_macos.sh`
3. The DMG will be created in `installers/packages/DeltaCLI-<version>-macOS-<arch>.dmg`

**What's Included:**
- Delta CLI application bundle (.app)
- Applications folder symlink for easy installation
- README with installation instructions
- Web UI (if built)

**Distribution:**
- Upload the .dmg file to your release page or hosting service
- Users can download and double-click to mount
- They drag "Delta CLI.app" to the Applications folder
- The app can be run from Terminal or by double-clicking

**Customization:**
- Set version: `VERSION=1.2.3 ./installers/package_macos.sh`
- Add custom background: Place `background.png` in `installers/` directory

### Windows (.exe)
1. Build the project: `installers\build_windows.bat`
2. Install NSIS from https://nsis.sourceforge.io/
3. Create installer: `installers\package_windows.bat`
4. Run installer: `installers\packages\DeltaCLI-Setup-*.exe`

## Troubleshooting

### Common Issues

**Q: CMake can't find llama.cpp**
```bash
git submodule update --init --recursive
```

**Q: Build fails with "compiler not found"**
- Linux: Install build-essential: `sudo apt install build-essential`
- macOS: Install Xcode tools: `xcode-select --install`
- Windows: Use Visual Studio Developer Command Prompt

**Q: Tests fail**
- Normal if no models are installed
- Download a model to `~/.delta-cli/models/` and re-run tests

**Q: GPU acceleration not working**
- Verify CUDA/Metal/Vulkan installation
- Rebuild with appropriate flags enabled
- Check `delta --help` output for GPU support status

## Contact

For issues or questions, please visit:
https://github.com/yourusername/delta-cli/issues

