# Router Mode & Multi-Model Support — Summary

This document summarizes the changes made to support llama-server **router mode** (multi-model discovery) in delta-cli while keeping the existing single-model flow intact.

## Current Behavior (Before)

- **Server startup**: delta (and delta-server) always started llama-server with `-m <path>` (single model).
- **Model management**: Delta's model API (port 8081) listed installed models from `~/.delta-cli/models` and supported download/remove/use; "use" restarted llama-server with the new model.
- **WebUI**: Fetched `/v1/models` (main server) and `/api/models/list` (delta API), merged for the model selector.

## New Behavior (After)

### 1. Router mode (no `-m`)

- **`delta server`** (no `-m`): Starts llama-server **without** `-m`, with **`--models-dir <dir>`** so the server scans for `.gguf` files.
  - Default directory: `~/.delta-cli/models` (same as delta’s model cache).
  - Custom directory: `delta server --models-dir ~/my-ggufs/`.
- **`delta server -m qwen2.5:0.5b`**: Unchanged single-model mode.

### 2. Single-model mode (with `-m`)

- **`delta server -m <model>`**: Same as before; one model loaded, no scan.

### 3. delta-server binary (wrapper)

- Accepts **`--models-dir <dir>`**.
- If **`--models-dir`** is set and **`-m`** is not, builds the llama-server command with `--models-dir` only (router mode).
- If **`-m`** is set, builds with `-m` (single-model).
- Requires either `-m` or `--models-dir` (otherwise error).

### 4. WebUI

- **ModelsService.list()**: Still uses `/v1/models`. If the response has empty `data`, it tries **`/models`** (llama.cpp router endpoint) and normalizes the result so the model selector works in router mode.
- Model dropdown already merges main server models with delta’s installed list; router mode just feeds more models from `/v1/models` or `/models`.

### 5. CLI

- **`--models-dir <DIR>`** added to help and README.
- **`delta server`** without `-m`: uses default `~/.delta-cli/models` for router mode.
- **`delta server --models-dir /path`**: uses that path for router mode.

## Files Touched

| File | Changes |
|------|--------|
| **src/delta_server_wrapper.cpp** | `models_dir_` member, `set_models_dir()`, `build_llama_server_command()` builds `--models-dir` when no `-m`, `start_server()` allows empty model when models_dir set, parse `--models-dir` in main(). |
| **src/commands.cpp** | `build_llama_server_cmd(..., models_dir)` for router mode; `launch_server_auto(..., models_dir)` and skip model path check when router mode. |
| **src/commands.h** | Signatures for `launch_server_auto` and `build_llama_server_cmd` with optional `models_dir`. |
| **src/main.cpp** | Parse `--models-dir`; server branch: when no `-m` use router mode (default or given dir), build cmd with `--models-dir` only; help text. |
| **assets/src/lib/services/models.ts** | `list()` tries `/models` when `/v1/models` returns empty data and normalizes for router mode. |
| **README.md** | Router mode and `--models-dir` in Quick Start and options. |

## Compatibility

- **llama.cpp**: Router mode and `--models-dir` are supported in recent llama.cpp (late 2025+). If the vendored submodule is older, router mode may not work until the submodule is updated (e.g. `git submodule update --remote vendor/llama.cpp`).
- **Single-model**: Unchanged; `-m` still forces one model.
- **Offline / privacy**: No new network or telemetry; models-dir is local.

## Testing

1. **Router mode**: `delta server` (no `-m`) → server starts, open http://localhost:8080 → model selector shows models from `~/.delta-cli/models` (or from `/models`/`/v1/models`).
2. **Single-model**: `delta server -m qwen2.5:0.5b` → same as before, one model.
3. **Custom dir**: `delta server --models-dir /path/to/ggufs` → server scans that dir, WebUI lists those models.

## Optional: Update llama.cpp Submodule

To use router mode with a recent llama.server:

```bash
git submodule update --init vendor/llama.cpp
cd vendor/llama.cpp && git fetch origin && git checkout main && cd ../..
# Then rebuild delta and delta-server.
```
