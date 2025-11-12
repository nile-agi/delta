# Delta CLI - Quick Start Guide

## ✅ Installation Complete!

Your Delta CLI has been successfully installed. Here's what to do next:

## Step 1: Test Installation

```bash
delta --version
```

Expected output: `Delta CLI v1.0.0`

## Step 2: Download a Model

Before you can use Delta CLI, you need to download a model:

```bash
# Download a small, fast model (recommended for first time)
delta download qwen2.5:0.6b

# Or list available models first
delta list --available
```

**Popular models:**
- `qwen2.5:0.6b` - Small, fast (recommended for testing)
- `qwen2.5:1.5b` - Slightly larger, better quality
- `qwen2.5:3b` - Good balance of speed and quality
- `qwen2.5:7b` - Higher quality (requires more RAM)

## Step 3: Start the Server

```bash
delta server
```

This will:
- Start the server on `http://localhost:8080`
- Automatically open your browser
- Use the original llama.cpp web UI from `vendor/llama-cpp/tools/server/webui`

## Step 4: Start Chatting!

Once the server starts:
1. Your browser will automatically open to `http://localhost:8080`
2. You'll see the llama.cpp web UI
3. Start chatting with your AI model!

## Alternative: Interactive Mode

If you prefer command-line chat instead of web UI:

```bash
delta interactive
```

## Common Commands

```bash
# Check version
delta --version

# List installed models
delta list

# List available models online
delta list --available

# Download a model
delta download <model-name>

# Start web server
delta server

# Start interactive chat
delta interactive

# Get help
delta help
```

## Troubleshooting

### "delta: command not found"

If the command is not found, restart your terminal or run:
```bash
export PATH="/usr/local/bin:$PATH"
```

### Server won't start

1. Check if a model is installed:
   ```bash
   delta list
   ```

2. Check if port 8080 is available:
   ```bash
   lsof -i :8080
   ```

3. Kill any existing server:
   ```bash
   pkill -f llama-server
   ```

### Model not found

Download a model first:
```bash
delta download qwen2.5:0.6b
```

## Next Steps

1. ✅ Test: `delta --version`
2. ✅ Download: `delta download qwen2.5:0.6b`
3. ✅ Start: `delta server`
4. ✅ Chat: Open http://localhost:8080

Enjoy using Delta CLI! 🚀
