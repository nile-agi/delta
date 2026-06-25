# Building Delta

## Prerequisites

| Tool | Version | Purpose |
|------|---------|---------|
| CMake | 3.14+ | C++ build system |
| C++17 compiler | clang or gcc | Engine compilation |
| Node.js | 18+ | Web UI build |
| pnpm | 9+ | Package manager |
| Rust | stable | Desktop app (Tauri) |

### Platform-specific

**macOS:**
```bash
xcode-select --install       # C++ toolchain
brew install cmake node pnpm
curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh  # Rust (for desktop)
```

**Linux (Ubuntu/Debian):**
```bash
sudo apt-get install -y \
  build-essential cmake \
  libcurl4-openssl-dev \
  libwebkit2gtk-4.1-dev \
  libappindicator3-dev \
  librsvg2-dev patchelf
npm install -g pnpm
```

**Windows:**
- Visual Studio 2022 with C++ workload
- CMake (bundled with VS or standalone)
- Node.js from [nodejs.org](https://nodejs.org/)
- `npm install -g pnpm`

## Project Structure

```
delta/
├── engine/              # C++ source (CLI + delta-server)
│   ├── vendor/llama.cpp # llama.cpp submodule
│   └── ...
├── web/app/             # SvelteKit web UI
├── src-tauri/           # Tauri desktop app wrapper
│   ├── binaries/        # Sidecar binaries (delta-server, llama-server)
│   ├── frontend/        # Splash screen shown during startup
│   └── src/lib.rs       # Tauri setup (spawns delta-server, navigates webview)
├── public/              # Built web UI output (from web/app)
├── scripts/             # Build helper scripts
└── VERSION              # Single source of truth for version
```

## Initialize Submodules

```bash
git submodule update --init --recursive
```

## Build the Web UI

```bash
cd web/app
pnpm install
pnpm run build       # outputs to ../../public/
```

## Build CLI Binaries

```bash
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -DGGML_METAL=ON  # macOS
# cmake .. -DCMAKE_BUILD_TYPE=Release -DGGML_METAL=OFF  # Linux/Windows
cmake --build . -j$(nproc) --target delta --target delta-server
```

Outputs:
- `build/delta` — CLI binary
- `build/delta-server` — Server wrapper (manages llama-server + model API)

Also build llama-server:
```bash
cmake --build . -j$(nproc) --target llama-server
```

### Cross-compile macOS architectures

```bash
# arm64
cmake .. -DCMAKE_OSX_ARCHITECTURES=arm64 -DGGML_METAL=ON
# x86_64
cmake .. -DCMAKE_OSX_ARCHITECTURES=x86_64 -DGGML_METAL=ON
```

## Build Desktop App (Tauri)

### 1. Build sidecar binaries

```bash
chmod +x scripts/build-sidecars.sh
scripts/build-sidecars.sh --release
```

This builds delta-server and llama-server and copies them to `src-tauri/binaries/` with the correct target-triple suffix.

### 2. Build the Tauri app

```bash
cd src-tauri
cargo tauri build
```

Outputs:
- `src-tauri/target/release/bundle/macos/Delta.app` — macOS app bundle
- `src-tauri/target/release/bundle/dmg/Delta_*.dmg` — macOS disk image

### How the desktop app works

1. Tauri opens a webview showing `src-tauri/frontend/index.html` (animated Delta splash screen)
2. The Tauri setup spawns `delta-server` as a sidecar process
3. `delta-server` starts llama-server on the chosen port and a model management API on port + 1
4. Once the server is reachable, the webview navigates to `http://localhost:{port}`
5. The web UI is served by llama-server from `Contents/Resources/webui/` (bundled from `public/`)

## Run Locally

### CLI
```bash
./build/delta                    # Interactive mode
./build/delta-server --port 8080 --models-dir ~/.delta-cli/models
```

### Desktop app
```bash
open src-tauri/target/release/bundle/macos/Delta.app
```

## Release Workflow

The CI release is triggered by pushing a version tag:

```bash
git tag v1.0.0
git push origin v1.0.0
```

The workflow (`.github/workflows/release.yml`) runs two parallel jobs:

| Job | Builds | Platforms |
|-----|--------|-----------|
| `build-cli` | CLI archives (delta + delta-server + llama-server) | macOS arm64/x86_64, Linux x86_64, Windows x64 |
| `build-tauri` | Desktop installers (dmg, deb, AppImage, exe) | macOS arm64/x86_64, Linux x86_64, Windows x64 |

After both complete, `attach-cli` uploads CLI archives + SHA-256 checksums to the GitHub Release.

### Version bumping

Update version in three places (must match):
1. `VERSION`
2. `src-tauri/tauri.conf.json` → `"version"`
3. `src-tauri/Cargo.toml` → `version`

### macOS code signing

Set these GitHub repository secrets for notarized macOS builds:
- `APPLE_CERTIFICATE` — Base64-encoded .p12 certificate
- `APPLE_CERTIFICATE_PASSWORD`
- `APPLE_SIGNING_IDENTITY`
- `APPLE_ID`, `APPLE_PASSWORD`, `APPLE_TEAM_ID`

Without these, builds still succeed but won't be signed/notarized.
