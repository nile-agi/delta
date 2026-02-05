# Homebrew formula for Delta CLI

This directory contains the **canonical** Homebrew formula for Delta CLI.

## Install from this repo (recommended for tap users)

To get a working install that includes the **llama.cpp server** binary (required for `delta` and `delta-server` to run), use the formula in this repo:

```bash
brew tap nile-agi/delta-cli
brew install --HEAD nile-agi/delta-cli/delta-cli
```

**Important:** The tap repo [nile-agi/homebrew-delta-cli](https://github.com/nile-agi/homebrew-delta-cli) must use a formula that **installs the llama.cpp server binary** (as `server` next to `delta` and `delta-server`). This repo’s `delta-cli.rb` does that; if the tap’s copy does not, you will see:

- `Error: HTTP server binary ('server') not found` when running `delta`, or  
- Delta will not start the backend.

**Tap maintainers:** Keep the tap’s `Formula/delta-cli.rb` in sync with this file, especially the block that finds and installs the server binary (the `server_path` / `bin.install server_path => "server"` section). Without it, only `delta` and `delta-server` are installed and the wrapper cannot find llama-server.

## After install

1. Ensure at least one model is available:
   ```bash
   delta pull qwen2.5:0.5b
   ```
2. Run:
   ```bash
   delta
   ```
   or `delta server`. The server always uses `-m <absolute path>` (no `--models-dir`), so it works on all macOS/Homebrew builds.

## Local install from this repo

From the **delta repo root** (not the tap):

```bash
brew install --HEAD ./packaging/homebrew/delta-cli.rb
```

This uses the formula in this repo and installs the server binary correctly.
