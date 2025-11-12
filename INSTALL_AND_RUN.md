# Delta CLI - Installation and Usage Guide

## Quick Start

### Step 1: Install Delta CLI

**macOS:**
```bash
./install-macos.sh
```

**Linux:**
```bash
./install-linux.sh
```

**Windows:**
```powershell
.\install-windows.ps1
```

**Android:**
```bash
./install-android.sh
```

**iOS:**
```bash
./install-ios.sh
```

### Step 2: Run Delta CLI

```bash
# Start the server
delta server

# Or use interactive mode
delta interactive

# Or download a model
delta download qwen2.5:0.6b

# Or list available models
delta list
```

## Detailed Installation (macOS)

### Prerequisites

- macOS (Intel or Apple Silicon)
- Xcode Command Line Tools
- CMake 3.14+
- Homebrew (recommended)

### Installation Steps

1. **Install dependencies:**
   ```bash
   # Install Homebrew (if not installed)
   /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
   
   # Install CMake
   brew install cmake
   ```

2. **Run installation script:**
   ```bash
   cd /Users/suzanodero/Downloads/delta-cli
   ./install-macos.sh
   ```

   This will:
   - Install all dependencies
   - Build Delta CLI
   - Install system-wide to `/usr/local/bin/delta`

3. **Verify installation:**
   ```bash
   delta --version
   ```

## Detailed Installation (Linux)

1. **Install dependencies:**
   ```bash
   # Ubuntu/Debian
   sudo apt-get update
   sudo apt-get install -y build-essential cmake git
   
   # Fedora/RHEL
   sudo dnf install -y gcc gcc-c++ cmake git
   ```

2. **Run installation script:**
   ```bash
   cd /path/to/delta-cli
   ./install-linux.sh
   ```

3. **Verify installation:**
   ```bash
   delta --version
   ```

## Usage

### Starting the Server

```bash
# Start server with auto-selected model
delta server

# Start server with specific model
delta server --model qwen2.5:0.6b

# Start server on custom port
delta server --port 8081

# Start server with custom context size
delta server --ctx-size 8192
```

The server will:
- Start on `http://localhost:8080` (or your specified port)
- Automatically open your browser
- Use the web UI from `vendor/llama.cpp/tools/server/webui`

### Downloading Models

```bash
# Download a model
delta download qwen2.5:0.6b

# List available models
delta list

# List available models online
delta list --available
```

### Interactive Mode

```bash
# Start interactive chat
delta interactive

# Use a specific model
delta interactive --model qwen2.5:0.6b
```

### Other Commands

```bash
# Show help
delta help

# Check version
delta --version

# Update Delta CLI
delta update
```

## Troubleshooting

### "delta: command not found"

**Solution:**
```bash
# Check if delta is in PATH
which delta

# If not, add to PATH (macOS/Linux)
export PATH="/usr/local/bin:$PATH"

# Or reinstall
cd /path/to/delta-cli
./install-macos.sh  # or install-linux.sh
```

### Server won't start

**Check:**
1. Is a model installed?
   ```bash
   delta list
   ```

2. Is port 8080 available?
   ```bash
   lsof -i :8080
   ```

3. Kill any existing server:
   ```bash
   pkill -f llama-server
   ```

### Web UI not loading

**Solution:**
1. Check if webui directory exists:
   ```bash
   ls -la vendor/llama.cpp/tools/server/webui
   ```

2. Hard refresh browser:
   - Mac: `Cmd+Shift+R`
   - Windows/Linux: `Ctrl+Shift+R`

3. Try incognito/private window

### Model not found

**Solution:**
```bash
# Download the model first
delta download qwen2.5:0.6b

# Then use it
delta server --model qwen2.5:0.6b
```

## Quick Reference

```bash
# Installation
./install-macos.sh          # macOS
./install-linux.sh          # Linux
.\install-windows.ps1       # Windows

# Basic usage
delta server                # Start server
delta download <model>     # Download model
delta list                 # List models
delta interactive          # Interactive chat
delta help                 # Show help
```

## Next Steps

1. **Download a model:**
   ```bash
   delta download qwen2.5:0.6b
   ```

2. **Start the server:**
   ```bash
   delta server
   ```

3. **Open http://localhost:8080 in your browser**

4. **Start chatting!**

Enjoy using Delta CLI! 🚀
