# Delta CLI - Complete Build & Run Guide

## Overview

Delta CLI is built on top of **llama.cpp**, which is included as a subdirectory in `vendor/llama.cpp/`. When you build Delta CLI, it automatically builds llama.cpp as part of the process.

## Prerequisites

### macOS
- **Xcode Command Line Tools**: `xcode-select --install`
- **CMake 3.14+**: `brew install cmake` or download from [cmake.org](https://cmake.org)
- **Git**: Usually pre-installed, or `brew install git`
- **C++ Compiler**: Included with Xcode Command Line Tools

### Linux
- **Build tools**: `sudo apt-get install build-essential cmake git` (Ubuntu/Debian)
- **libcurl**: `sudo apt-get install libcurl4-openssl-dev`
- **Optional GPU support**: CUDA, Vulkan, or ROCm drivers

### Windows
- **Visual Studio 2019/2022** with C++ workload, OR
- **Build Tools for Visual Studio**
- **CMake 3.14+**
- **Git for Windows**

## Build Process

Delta CLI uses CMake to build both itself and llama.cpp. The build process is:

1. **Configure** with CMake (sets up both Delta and llama.cpp)
2. **Compile** with make/cmake --build (builds everything)
3. **Install** (optional, copies binaries to system paths)

---

## Quick Build (macOS)

### Option 1: Automated Script (Recommended)

```bash
cd /Users/suzanodero/Downloads/delta-cli
./install-macos.sh
```

This script:
- Checks/installs dependencies
- Builds Delta CLI + llama.cpp
- Installs system-wide
- Sets up web UI

### Option 2: Manual Build

```bash
cd /Users/suzanodero/Downloads/delta-cli

# Step 1: Create build directory
mkdir -p build_macos
cd build_macos

# Step 2: Configure with CMake
# This configures both Delta CLI and llama.cpp
cmake .. -DCMAKE_BUILD_TYPE=Release

# Step 3: Build (this builds both Delta and llama.cpp)
# Use all CPU cores for faster compilation
make -j$(sysctl -n hw.ncpu)

# Step 4: Install (optional, requires sudo)
sudo make install

# Or just copy the binary
cp delta ~/.local/bin/delta
```

---

## Detailed Build Steps

### Step 1: Build Web UI (Delta Branding)

The web UI needs to be built separately before building the CLI:

```bash
cd /Users/suzanodero/Downloads/delta-cli/vendor/llama.cpp/tools/server/webui

# Install dependencies (first time only)
npm install

# Build the web UI
npm run build

# Deploy to public directory
./scripts/post-build.sh

cd /Users/suzanodero/Downloads/delta-cli
```

### Step 2: Configure CMake

```bash
cd /Users/suzanodero/Downloads/delta-cli
mkdir -p build_macos
cd build_macos

# Basic configuration
cmake .. -DCMAKE_BUILD_TYPE=Release

# Or with specific options:
cmake .. \
    -DCMAKE_BUILD_TYPE=Release \
    -DGGML_METAL=ON \          # Enable Metal (macOS GPU)
    -DUSE_CURL=ON \            # Enable HTTP requests
    -DBUILD_SERVER=ON \        # Build server components
    -DBUILD_TESTS=OFF          # Skip tests (faster build)
```

**What happens during CMake configuration:**
- Detects your compiler and platform
- Configures llama.cpp build (in `vendor/llama.cpp/`)
- Sets up Delta CLI build
- Links everything together

### Step 3: Build Everything

```bash
# Build using all CPU cores (faster)
make -j$(sysctl -n hw.ncpu)

# Or build with specific number of jobs
make -j4

# Or use cmake build command
cmake --build . --config Release -j$(sysctl -n hw.ncpu)
```

**What gets built:**
1. **llama.cpp libraries** (`libllama.a`, `libggml.a`, etc.)
2. **Delta CLI executable** (`delta`)
3. **Delta server wrapper** (`delta-server`)

**Build time:** 
- First build: 10-30 minutes (depends on CPU)
- Incremental builds: 1-5 minutes

### Step 4: Install (Optional)

```bash
# System-wide installation (requires sudo)
sudo make install

# This installs:
# - /usr/local/bin/delta
# - /usr/local/bin/delta-server

# Or install to custom location
cmake --install . --prefix ~/.local

# Or just copy manually
cp delta ~/.local/bin/delta
cp delta-server ~/.local/bin/delta-server
```

---

## Build Options

### CMake Configuration Options

```bash
cmake .. \
    -DCMAKE_BUILD_TYPE=Release \     # Release (fast) or Debug (debuggable)
    -DGGML_METAL=ON \                 # macOS Metal GPU acceleration
    -DLLAMA_CUDA=ON \                 # NVIDIA CUDA support (Linux/Windows)
    -DLLAMA_VULKAN=ON \               # Vulkan GPU support
    -DLLAMA_BLAS=ON \                 # BLAS acceleration
    -DUSE_CURL=ON \                   # Enable HTTP requests (required)
    -DBUILD_SERVER=ON \               # Build server components
    -DBUILD_TESTS=ON \                # Build test suite
    -DGGML_CCACHE=ON                  # Use ccache for faster rebuilds
```

### Build Types

- **Release**: Optimized, fast execution (default for production)
- **Debug**: Includes debug symbols, slower but debuggable
- **RelWithDebInfo**: Optimized with debug info

```bash
# Debug build
cmake .. -DCMAKE_BUILD_TYPE=Debug

# Release build
cmake .. -DCMAKE_BUILD_TYPE=Release
```

---

## Running Delta CLI

### After Building

```bash
# If installed system-wide
delta --version

# If using local binary
./build_macos/delta --version

# Or add to PATH
export PATH="$PWD/build_macos:$PATH"
delta --version
```

### Start the Server

```bash
# Start web UI server
delta server

# Or with specific model
delta server -m qwen3:0.6b

# Or with custom port
delta server --port 8081
```

Then open **http://localhost:8080** in your browser.

### Interactive Mode

```bash
# Start interactive chat
delta

# Or with specific model
delta -m llama3.1:8b
```

---

## Building llama.cpp Separately (Optional)

If you want to build llama.cpp standalone:

```bash
cd /Users/suzanodero/Downloads/delta-cli/vendor/llama.cpp

# Create build directory
mkdir -p build
cd build

# Configure
cmake .. -DCMAKE_BUILD_TYPE=Release

# Build
make -j$(sysctl -n hw.ncpu)

# This builds:
# - llama-server (server executable)
# - llama-cli (command-line interface)
# - Various examples
```

**Note:** When building Delta CLI, llama.cpp is automatically built as a dependency, so you don't need to build it separately.

---

## Troubleshooting

### Build Fails with "llama.cpp not found"

```bash
# Ensure llama.cpp is in vendor directory
ls vendor/llama.cpp/CMakeLists.txt

# If missing, you may need to initialize submodules
git submodule update --init --recursive
```

### Build is Slow

```bash
# Use more CPU cores
make -j$(sysctl -n hw.ncpu)

# Or specific number
make -j8

# Enable ccache for faster rebuilds
cmake .. -DGGML_CCACHE=ON
```

### "Command not found" after build

```bash
# Add build directory to PATH
export PATH="$PWD/build_macos:$PATH"

# Or install system-wide
sudo make install
```

### Web UI Not Showing

```bash
# Rebuild web UI
cd vendor/llama.cpp/tools/server/webui
npm run build
./scripts/post-build.sh
cd ../../../../..
```

### Clean Build

```bash
# Remove build directory and start fresh
rm -rf build_macos
mkdir build_macos
cd build_macos
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(sysctl -n hw.ncpu)
```

---

## Build Output Locations

After building, you'll find:

```
build_macos/
├── delta              # Main Delta CLI executable
├── delta-server       # Server wrapper executable
├── CMakeCache.txt     # CMake configuration cache
└── vendor/
    └── llama.cpp/     # Built llama.cpp libraries
        ├── libllama.a
        ├── libggml.a
        └── ...
```

---

## Quick Reference

### Full Build Command (macOS)

```bash
cd /Users/suzanodero/Downloads/delta-cli

# Build web UI
cd vendor/llama.cpp/tools/server/webui
npm install && npm run build && ./scripts/post-build.sh
cd ../../../../..

# Build Delta CLI + llama.cpp
mkdir -p build_macos && cd build_macos
cmake .. -DCMAKE_BUILD_TYPE=Release -DGGML_METAL=ON
make -j$(sysctl -n hw.ncpu)

# Install
sudo make install

# Run
delta server
```

### Quick Rebuild (after code changes)

```bash
cd /Users/suzanodero/Downloads/delta-cli/build_macos
make -j$(sysctl -n hw.ncpu)
sudo make install
```

---

## Summary

1. **Delta CLI** and **llama.cpp** are built together in one CMake process
2. **Web UI** must be built separately using npm
3. **Build time**: 10-30 minutes first time, 1-5 minutes for incremental builds
4. **Output**: `delta` and `delta-server` executables in `build_macos/`
5. **Installation**: Optional, can use binaries directly or install system-wide

The build process automatically handles all llama.cpp dependencies - you don't need to build it separately!

