# Delta

<div align="center">

```
 ██████╗ ███████╗██╗  ████████╗ █████╗
 ██╔══██╗██╔════╝██║  ╚══██╔══╝██╔══██╗
 ██║  ██║█████╗  ██║     ██║   ███████║
 ██║  ██║██╔══╝  ██║     ██║   ██╔══██║
 ██████╔╝███████╗███████╗██║   ██║  ██║
 ╚═════╝ ╚══════╝╚══════╝╚═╝   ╚═╝  ╚═╝
```

**Run AI models locally. Completely offline. On any device.**

[![License](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)
[![Built on llama.cpp](https://img.shields.io/badge/built%20on-llama.cpp-orange.svg)](https://github.com/ggerganov/llama.cpp)

</div>

---

Delta is an offline-first AI assistant that runs large language models directly on your device. It ships as a **native desktop app** (macOS, Linux, Windows) and a **CLI** — no cloud, no subscriptions, no data leaves your machine.

### Desktop App

The Delta desktop app is a single native application with a built-in web UI. Download the `.dmg` (macOS), `.deb`/`.AppImage` (Linux), or `.exe` (Windows) from [Releases](https://github.com/nile-agi/delta/releases).

### CLI

```bash
delta                    # Launch web UI in browser
delta server             # Start server on port 8080
delta server -m model.gguf  # Start with a specific model
```

---

## Features

- **100% Offline** — all inference runs locally after model download
- **Native Desktop App** — single icon, click to open, full UI
- **Cross-Platform** — macOS (Intel + Apple Silicon), Linux, Windows
- **GPU Accelerated** — Metal, CUDA, Vulkan, ROCm
- **Model Management** — download, load, and switch models from the UI
- **Web UI** — built-in chat interface served by the Delta server
- **OpenAI-compatible API** — use with any client that speaks `/v1/chat/completions`
- **Any GGUF Model** — works with all quantized models from Hugging Face

## Architecture

```
┌─────────────────────────────────────────────┐
│  Delta Desktop App (Tauri)                  │
│  ┌───────────────────────────────────────┐  │
│  │  WebView                              │  │
│  │  Shows splash → navigates to server   │  │
│  └───────────────┬───────────────────────┘  │
│                  │                           │
│  ┌───────────────▼───────────────────────┐  │
│  │  delta-server (sidecar)               │  │
│  │  ├─ llama-server (port N)   ← Web UI  │  │
│  │  └─ Model API   (port N+1) ← manage  │  │
│  └───────────────────────────────────────┘  │
└─────────────────────────────────────────────┘
```

- **delta-server** wraps llama.cpp's `llama-server` with model management
- **Model API** (port N+1) handles downloading, loading, unloading models
- **Web UI** is built from `web/app/` (SvelteKit) and bundled into the app
- Ports are dynamically assigned — if 8080 is taken, the next free port is used

## Quick Start

### Download

Grab the latest release for your platform from [Releases](https://github.com/nile-agi/delta/releases):

| Platform | Desktop App | CLI |
|----------|-------------|-----|
| macOS (Apple Silicon) | `Delta_*_aarch64.dmg` | `delta-cli-macos-arm64.tar.gz` |
| macOS (Intel) | `Delta_*_x64.dmg` | `delta-cli-macos-x86_64.tar.gz` |
| Linux (x86_64) | `.deb` / `.AppImage` | `delta-cli-linux-x86_64.tar.gz` |
| Windows (x64) | `.exe` installer | `delta-cli-windows-x64.zip` |

### Build from Source

See [docs/building.md](docs/building.md) for full instructions.

```bash
git clone --recursive https://github.com/nile-agi/delta.git
cd delta

# Build web UI
cd web/app && pnpm install && pnpm run build && cd ../..

# Build C++ binaries
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . -j$(nproc) --target delta --target delta-server
cmake --build . -j$(nproc) --target llama-server || true

# Build desktop app
cd ../src-tauri
chmod +x ../scripts/build-sidecars.sh
../scripts/build-sidecars.sh --release
cargo tauri build
```

## Project Structure

```
delta/
├── engine/              # C++ source — CLI, delta-server, model API
├── web/app/             # SvelteKit web UI
├── src-tauri/           # Tauri desktop app
├── public/              # Built web UI (output of web/app build)
├── scripts/             # Build scripts
├── docs/                # Documentation
├── .github/workflows/   # CI/CD — release pipeline
└── VERSION              # Version (read by CMake + CI)
```

## Models

Delta uses GGUF format models. On first launch, open the model manager in the UI to download a model. Models are stored in `~/.delta-cli/models/`.

## Support

- **Issues**: [GitHub Issues](https://github.com/nile-agi/delta/issues)
- **Contact**: hi@nileagi.com

## Acknowledgments

- [llama.cpp](https://github.com/ggerganov/llama.cpp) — inference engine
- [Tauri](https://tauri.app) — desktop app framework
- [Hugging Face](https://huggingface.co) — model hosting

## License

MIT License. See [LICENSE](LICENSE).

---

<div align="center">

**Made by [Nile AGI](https://nileagi.com)**

[Releases](https://github.com/nile-agi/delta/releases) | [Report Issues](https://github.com/nile-agi/delta/issues)

</div>
