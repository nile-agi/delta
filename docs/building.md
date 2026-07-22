# Building Delta

## Prerequisites

Install these before building anything.

| Tool | Version | Purpose |
|------|---------|---------|
| CMake | 3.14+ | C++ build system |
| C++17 compiler | clang or gcc | Engine compilation |
| Node.js | 18+ | Web UI build |
| pnpm | 11+ | Package manager |
| Rust | stable | Desktop app (Tauri) |
| Make | any | Build automation |

### macOS

```bash
xcode-select --install       # C++ toolchain
brew install cmake node
npm install -g pnpm@latest   # pnpm 11+
curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh
```

### Linux (Ubuntu/Debian)

```bash
sudo apt-get install -y \
  build-essential cmake \
  libcurl4-openssl-dev \
  libwebkit2gtk-4.1-dev \
  libappindicator3-dev \
  librsvg2-dev patchelf
npm install -g pnpm
```

### Windows

- Visual Studio 2022 with C++ workload
- CMake (bundled with VS or standalone)
- Node.js from [nodejs.org](https://nodejs.org/)
- `npm install -g pnpm`

## Quick Start

```bash
git clone --recursive https://github.com/nile-agi/delta.git
cd delta
make engine     # Build C++ engine (delta-server + llama-server)
make sidecars   # Copy sidecar binaries to src-tauri/binaries/
make web        # Build SvelteKit web UI
make dev        # Run the Tauri desktop app
```

Or build everything in one go:

```bash
make            # Builds engine + sidecars + web
make dev        # Run the app
```

### After making changes

| Changed | Rebuild with | Then |
|---------|-------------|------|
| C++ source (`engine/`) | `make engine sidecars` | Restart the app |
| Web source (`web/app/`) | `make web` | Restart the app |
| Rust source (`src-tauri/`) | Nothing | `make dev` rebuilds automatically |

### All make targets

```
make            Build everything (engine + sidecars + web)
make engine     Build C++ engine (delta-server + llama-server)
make sidecars   Copy sidecar binaries into src-tauri/binaries/
make web        Build SvelteKit web UI
make dev        Run Tauri desktop app (cargo tauri dev)
make run        Full rebuild then run
make clean      Remove build artifacts
make submodules Init git submodules
```

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
│   └── src/lib.rs       # Tauri setup (spawns delta-server, injects ports)
├── public/              # Built web UI output (from web/app)
├── scripts/             # Build helper scripts
└── Makefile             # Build automation
```

## How the Desktop App Works

1. Tauri serves the SPA from `frontendDist` (`public/`) on `tauri://localhost`
2. The layout shows a splash screen while waiting for the server
3. Tauri spawns `delta-server` as a sidecar process
4. `delta-server` starts llama-server on a chosen port and a model management API on port + 1
5. Once the server is reachable, Rust injects `window.__DELTA_PORT__` and `window.__DELTA_MODEL_API_PORT__` via `eval()` and fires a `delta-server-ready` event
6. The SPA uses these ports to make API calls to `http://127.0.0.1:{port}`

## Building Without Make

If you prefer running commands manually:

### Engine (C++)

```bash
mkdir -p build
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DGGML_METAL=ON   # macOS
# cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DGGML_METAL=OFF  # Linux/Windows
cmake --build build -j$(sysctl -n hw.ncpu) --target delta-server --target llama-server
```

### Copy sidecars

```bash
chmod +x scripts/build-sidecars.sh
./scripts/build-sidecars.sh --release
```

### Web UI

```bash
cd web/app
pnpm install
pnpm run build    # outputs to ../../public/
```

### Run desktop app

```bash
cd src-tauri
cargo tauri dev
```

### Build desktop app for distribution

```bash
cd src-tauri
cargo tauri build
```

Outputs:
- `src-tauri/target/release/bundle/macos/Delta.app`
- `src-tauri/target/release/bundle/dmg/Delta_*.dmg`

### Cross-compile macOS architectures

```bash
cmake -S . -B build -DCMAKE_OSX_ARCHITECTURES=arm64 -DGGML_METAL=ON   # Apple Silicon
cmake -S . -B build -DCMAKE_OSX_ARCHITECTURES=x86_64 -DGGML_METAL=ON  # Intel
```

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
1. `version.txt`
2. `src-tauri/tauri.conf.json` -> `"version"`
3. `src-tauri/Cargo.toml` -> `version`

### macOS code signing

Set these GitHub repository secrets for notarized macOS builds:
- `APPLE_CERTIFICATE` -- Base64-encoded .p12 certificate
- `APPLE_CERTIFICATE_PASSWORD`
- `APPLE_SIGNING_IDENTITY`
- `APPLE_ID`, `APPLE_PASSWORD`, `APPLE_TEAM_ID`

Without these, builds still succeed but won't be signed/notarized.
