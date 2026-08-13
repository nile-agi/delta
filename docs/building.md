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

> On macOS 26+ the system SDK requires **clang 17+ (Xcode 16+)**. If `clang --version`
> is older, the build fails in llama.cpp with `unrecognized platform name visionOS`.
> Fix: `sudo xcode-select -s /Applications/Xcode.app/Contents/Developer` (full Xcode),
> or refresh CLT: `sudo rm -rf /Library/Developer/CommandLineTools && sudo xcode-select --install`.
> The build runs this check up front (`scripts/check-toolchain.sh`) and stops early if the toolchain is too old.

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
make            # Builds sidecars (delta-server + llama-server) + web UI
make dev        # Run the Tauri desktop app
```

`make` (a.k.a. `make all`) builds `sidecars` + `web` — the two things the app
ships. `sidecars` compiles the C++ engine **and** copies the binaries into
`src-tauri/binaries/` with the target-triple suffix Tauri requires; `make
engine` alone does not do that copy, so it is not part of `all`. `make dev` also
auto-builds the sidecars if they're missing, so a plain `make dev` on a fresh
clone works too.

Prefer step-by-step:

```bash
make sidecars   # Build C++ engine + copy binaries into src-tauri/binaries/
make web        # Build SvelteKit web UI
make dev        # Run the app
```

### After making changes

| Changed | Rebuild with | Then |
|---------|-------------|------|
| C++ source (`engine/`) | `make sidecars` | Restart the app |
| Web source (`web/app/`) | `make web` | Restart the app |
| Rust source (`src-tauri/`) | Nothing | `make dev` rebuilds automatically |

### All make targets

```
make            Build what the app ships: sidecars + web
make sidecars   Build C++ engine + copy binaries into src-tauri/binaries/
make engine     Build C++ engine only, into build/ (standalone CLI; not used by the app)
make web        Build SvelteKit web UI
make preview    Build web + serve it WITH the engine at http://localhost:8080 (browser)
make dev        Run Tauri desktop app (auto-builds sidecars if missing)
make run        Full rebuild then run (all + dev)
make clean      Remove build artifacts
make submodules Init git submodules
```

### Preview in a browser (no desktop build)

`make preview` builds the web UI and serves it **together with the engine** at
`http://localhost:8080` via the CLI's UI-only server, so the app actually works
in a browser — pick a model in the UI and go. It builds the `delta` CLI once on
first run.

> Note: `pnpm run preview` (Vite's own hint after `make web`) serves the static
> files **only** — with no engine behind it the app just shows
> "Server Connection Error". Use `make preview` (or `make dev`) instead.

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

## Linux Troubleshooting

**Blank screen on Ubuntu / Linux:**
The app sets `WEBKIT_DISABLE_COMPOSITING_MODE=1` automatically to work around WebKitGTK GPU compositing issues. If you still see a blank screen, try these environment variables:

```bash
# Sandbox issues inside AppImage
WEBKIT_FORCE_SANDBOX=0 ./Delta_*.AppImage

# Wayland display server issues
GDK_BACKEND=x11 ./Delta_*.AppImage

# Combine all workarounds
GDK_BACKEND=x11 WEBKIT_FORCE_SANDBOX=0 ./Delta_*.AppImage
```

To diagnose, run the AppImage from a terminal and check stderr for WebKit errors.

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
