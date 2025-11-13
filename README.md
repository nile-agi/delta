# Delta CLI

<div align="center">

```
╔═══════════════════════════════════════════════════════════════╗
║                                                               ║
║ ██████╗ ███████╗██╗  ████████╗ █████╗      ██████╗██╗     ██╗ ║
║ ██╔══██╗██╔════╝██║  ╚══██╔══╝██╔══██╗    ██╔════╝██║     ██║ ║
║ ██║  ██║█████╗  ██║     ██║   ███████║    ██║     ██║     ██║ ║
║ ██║  ██║██╔══╝  ██║     ██║   ██╔══██║    ██║     ██║     ██║ ║
║ ██████╔╝███████╗███████╗██║   ██║  ██║    ╚██████╗███████╗██║ ║
║ ╚═════╝ ╚══════╝╚══════╝╚═╝   ╚═╝  ╚═╝     ╚═════╝╚══════╝╚═╝ ║
║                                                               ║
║                Offline AI Assistant — Delta CLI               ║ 
║                      powered by llama.cpp                     ║
║                         Version 1.0.0                         ║
╚═══════════════════════════════════════════════════════════════╝
```

**Run powerful AI models locally, completely offline, on any device.**

[![License](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)
[![Open Source](https://img.shields.io/badge/open--source-Yes-green.svg)](https://github.com/nile-agi/delta)
[![Built on llama.cpp](https://img.shields.io/badge/built%20on-llama.cpp-orange.svg)](https://github.com/ggerganov/llama.cpp)

</div>

---

## What is Delta CLI?

Delta CLI is an **open-source, offline-first AI assistant** that runs large language models (LLMs) directly on your device. Built on top of [llama.cpp](https://github.com/ggerganov/llama.cpp), Delta CLI provides a simple command-line interface to interact with AI models without requiring internet connectivity or cloud services.

### Key Features

- 🔒 **100% Offline**: Works completely offline after initial model download
- ⚡ **High Performance**: Full GPU acceleration (CUDA, Metal, Vulkan, ROCm)
- 🌐 **Cross-Platform**: Runs on Windows, macOS, and Linux
- 🎨 **Beautiful Terminal UI**: Retro-green interface with custom styling
- 📦 **Easy Model Management**: One-command downloads from Hugging Face
- 🚀 **Zero-Setup**: Auto-downloads default model on first run
- 🔧 **llama.cpp Integration**: Access to all llama.cpp features and optimizations
- 🌐 **Web UI**: Built-in web interface using original llama.cpp web UI

### What Problem Does Delta Solve?

**Privacy & Security:**
- No data leaves your device - all processing happens locally
- No API keys required - no need to sign up for cloud services
- Complete control - your conversations and data stay private

**Cost & Accessibility:**
- No subscription fees - run AI models without recurring costs
- No usage limits - use as much as you want, whenever you want
- Works offline - perfect for areas with poor internet connectivity

**Flexibility & Control:**
- Choose your model - use any compatible GGUF model
- Customize settings - full control over temperature, context size, and more
- No vendor lock-in - open source and self-hosted

### Use Cases

- **Development**: Code assistance, documentation, debugging
- **Writing**: Content creation, editing, brainstorming
- **Learning**: Educational Q&A, explanations, tutorials
- **Research**: Data analysis, summarization, research assistance
- **Privacy-sensitive tasks**: When you can't send data to cloud services

---

## Installation

### Quick Installation (One Command)

Each platform has a dedicated installation method that handles everything automatically:

#### 🍎 macOS

**Homebrew (Recommended):**
```bash
brew tap nile-agi/delta-cli && brew install --HEAD nile-agi/delta-cli/delta-cli
```

**What it does:**
- ✅ Automatically clones repository (git happens in background)
- ✅ Automatically installs dependencies
- ✅ Automatically builds from source (~40 seconds)
- ✅ Automatically configures PATH
- ✅ Users don't need to know about git

**Alternative - Installation Script:**
```bash
curl -fsSL https://raw.githubusercontent.com/nile-agi/delta/main/install.sh | bash
```

#### 🐧 Linux

**Debian/Ubuntu:**
```bash
curl -fsSL https://raw.githubusercontent.com/nile-agi/delta/main/packaging/linux/install-deb.sh | sudo bash
```

**RHEL/CentOS/Fedora:**
```bash
curl -fsSL https://raw.githubusercontent.com/nile-agi/delta/main/packaging/linux/install-rpm.sh | sudo bash
```

**Homebrew (Linux):**
```bash
# Install Homebrew for Linux first (if not installed)
/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"

# Install Delta CLI
brew tap nile-agi/delta-cli && brew install --HEAD nile-agi/delta-cli/delta-cli
```

#### 🪟 Windows

**Winget (Recommended):**
```powershell
winget install DeltaCLI.DeltaCLI
```

**PowerShell Installation Script:**
```powershell
Set-ExecutionPolicy Bypass -Scope Process -Force; Invoke-WebRequest -Uri "https://raw.githubusercontent.com/nile-agi/delta/main/packaging/windows/install.ps1" -OutFile install.ps1; .\install.ps1
```

### Verification

After installation, verify it works:

```bash
delta --version
```

You should see:
```
Delta CLI v1.0.0
Professional offline AI assistant
```

### Installation Methods Comparison

| Method | Windows | macOS | Linux | Speed | Git Required | Building Required |
|---|---|---|---|---|---|---|
| **Winget** | ✅ | ❌ | ❌ | ⚡ Fast | ❌ No | ❌ No |
| **Homebrew** | ❌ | ✅ | ✅ | 🐢 Slow (builds) | ✅ Yes (automatic) | ✅ Yes (automatic) |
| **Install Script** | ✅ | ✅ | ✅ | ⚡ Fast | ❌ No | ❌ No |

**All methods are automatic - users don't need git knowledge!**

---

## Quick Start

### 1. Run Delta CLI

```bash
# Just type 'delta' and press Enter
delta
```

**What happens:**
1. Shows banner and welcome message
2. Auto-downloads default model (`qwen2.5:0.5b`, ~400MB) if not installed
3. Loads model automatically
4. Starts interactive mode
5. You're ready to chat!

### 2. Start Chatting

Once in interactive mode, simply type your questions:

```
δ> What is artificial intelligence?
δ> Explain quantum computing in simple terms
δ> Write a Python function to sort a list
```

### 3. Download More Models (Optional)

```bash
# See all available models
delta --list-models --available

# Download a model
delta pull qwen2.5:0.5b      # 400 MB - default, fast
delta pull llama3.1:8b       # 4.7 GB - powerful, versatile
delta pull mistral:7b         # 4.3 GB - great for coding
```

### 4. Use Specific Model

```bash
# Use a specific model
delta --model llama3.1:8b "Your question here"

# With GPU acceleration
delta --model qwen2.5:0.5b --gpu-layers -1 "Your question"
```

### 5. Start Web Server

```bash
# Start web UI server
delta server

# Or with specific model
delta server -m qwen2.5:0.5b

# Or with custom port
delta server --port 8081
```

Then open **http://localhost:8080** in your browser to use the web interface.

---

## Usage

### Basic Commands

```bash
# Interactive mode
delta

# Single query
delta "What is the capital of France?"

# With specific model
delta --model llama3.1:8b "Explain neural networks"

# List local models
delta --list-models

# Download a model
delta pull qwen2.5:0.5b

# Remove a model
delta remove qwen2.5:0.5b

# Start web server
delta server
```

### Command-Line Options

```bash
delta [OPTIONS] [PROMPT]

OPTIONS:
    -h, --help              Show help message
    -v, --version           Show version information
    -m, --model <MODEL>     Specify model (short name or full filename)
    -l, --list-models       List locally cached models
        --available         With -l, show available models to download
    -t, --tokens <N>        Max tokens to generate (default: 512)
    -T, --temperature <F>  Sampling temperature (default: 0.8)
    -c, --ctx-size <N>      Context size (default: 2048)
    -g, --gpu-layers <N>    Number of GPU layers (default: 0, use -1 for all)
    --multimodal            Enable multimodal mode (images + text)
    --interactive           Start interactive chat mode
    --server                Start Delta Server (OpenAI-compatible API)
        --port <N>          Server port (default: 8080)
        --np <N>            Max parallel requests (default: 4)
        --c <N>             Max context size (default: 16384)
    --check-updates         Check for new versions
    --update                Update to latest version
    --no-color              Disable colored output
```

### Interactive Commands

In interactive mode, use slash commands:

```bash
/download <model>        # Download a model
/remove <model>          # Remove a model (alias: /delete)
/list                    # List local models
/available               # List available models
/use <model>             # Switch to another model
/tokens <N>              # Set max tokens (default: 512)
/temperature <F>         # Set temperature (default: 0.8)
/gpu-layers <N>          # Set GPU layers (default: 0, -1 for all)
/multimodal              # Toggle multimodal mode
/server                  # Start web dashboard
/updates                 # Check for updates
/version                 # Show version info
/no-color                # Toggle colored output
/help                    # Show all commands
```

### Session Management

```bash
/new-session <name>      # Create a new named session
/switch-session <name>  # Switch to another session
/list-sessions          # List all available sessions
/delete-session <name>  # Delete a session
/history                # Show conversation history
/delete-history <all|id|day|week|year> [date]  # Delete history entries
/clear-screen           # Clear the terminal screen
```

**Note:** Delta ALWAYS uses a 'default' session for all interactions. All conversations are automatically saved to this session.

### Examples

```bash
# Download a model
delta pull qwen2.5:0.5b
delta pull llama3.1:8b

# Remove a model
delta remove qwen2.5:0.5b

# List models
delta --list-models
delta --list-models --available

# Use a model with short names
delta --model qwen2.5-0.5b "Explain quantum computing"
delta --model llama3.1-8b --gpu-layers -1 "Write a poem"
delta --model mistral-7b --interactive

# Start server
delta server
delta --server -m llama3.2:1b --port 8080

# Check for updates
delta --check-updates
delta --update
```

---

## Troubleshooting

### Command Not Found

**macOS/Linux:**
```bash
# Reload shell configuration
source ~/.zshrc  # or ~/.bash_profile
# Or restart terminal

# Or use full path
/opt/homebrew/bin/delta --version  # macOS
/usr/local/bin/delta --version     # Linux
```

**Windows:**
- Restart PowerShell/Command Prompt
- Or use full path: `C:\Program Files\Delta CLI\delta.exe`

### PATH Conflicts

If you have another `delta` command (like llvm's delta or git-delta):

**macOS:**
- An alias is created: `alias delta='/opt/homebrew/bin/delta'`
- Run `source ~/.zshrc` to activate

**Linux/Windows:**
- Installation directory is added to PATH first

### Server Won't Start

1. Check if a model is installed:
   ```bash
   delta --list-models
   ```

2. Check if port 8080 is available:
   ```bash
   lsof -i :8080  # macOS/Linux
   netstat -ano | findstr :8080  # Windows
   ```

3. Kill any existing server:
   ```bash
   pkill -f llama-server  # macOS/Linux
   taskkill /F /IM llama-server.exe  # Windows
   ```

### Model Not Found

Download a model first:
```bash
delta pull qwen2.5:0.5b
```

### Build Errors (if building from source)

- Ensure all dependencies are installed
- Check CMake version (3.14+ required)
- Verify C++ compiler is installed
- Check disk space (build requires several GB)

---

## Uninstallation

### macOS (Homebrew)

```bash
brew uninstall delta-cli
brew untap nile-agi/delta-cli
```

### Linux

**If installed via script:**
```bash
# Remove binaries
sudo rm -f /usr/local/bin/delta /usr/local/bin/delta-server

# Remove web UI
sudo rm -rf /usr/local/share/delta-cli

# Remove PATH configuration
sudo rm -f /etc/profile.d/delta-cli.sh
```

### Windows

**If installed via PowerShell script:**
```powershell
# Remove installation directory
Remove-Item -Recurse -Force "C:\Program Files\Delta CLI"

# Remove from PATH (requires Administrator)
$currentPath = [Environment]::GetEnvironmentVariable("Path", "Machine")
$newPath = $currentPath -replace ";C:\\Program Files\\Delta CLI", ""
[Environment]::SetEnvironmentVariable("Path", $newPath, "Machine")

# Remove desktop shortcut
Remove-Item "$env:USERPROFILE\Desktop\Delta CLI.lnk" -ErrorAction SilentlyContinue
```

### Complete Cleanup

**macOS:**
```bash
rm -f /opt/homebrew/bin/delta /opt/homebrew/bin/delta-server
rm -f /usr/local/bin/delta /usr/local/bin/delta-server
rm -rf /opt/homebrew/share/delta-cli /usr/local/share/delta-cli
rm -rf ~/.delta ~/.config/delta-cli
# Edit ~/.zshrc to remove Delta CLI related lines
```

**Linux:**
```bash
sudo rm -f /usr/local/bin/delta /usr/local/bin/delta-server
sudo rm -rf /usr/local/share/delta-cli
sudo rm -f /etc/profile.d/delta-cli.sh
rm -rf ~/.delta ~/.config/delta-cli
```

**Windows:**
```powershell
Remove-Item -Recurse -Force "C:\Program Files\Delta CLI"
Remove-Item -Recurse -Force "$env:USERPROFILE\.delta" -ErrorAction SilentlyContinue
Remove-Item -Recurse -Force "$env:APPDATA\delta-cli" -ErrorAction SilentlyContinue
# Remove from PATH via System Properties > Environment Variables
```

---

## Supported Platforms

Delta CLI runs on **3 major platforms**:

- ✅ **macOS** (Intel & Apple Silicon)
  - Native Metal acceleration
  - Homebrew installation support
  - Full feature support

- ✅ **Linux** (x86_64, ARM, ARM64)
  - Multiple distribution support
  - CUDA/Vulkan GPU acceleration
  - Package manager integration

- ✅ **Windows** (x64)
  - Visual Studio support
  - CUDA acceleration
  - PowerShell installation

---

## Open Source & Built on llama.cpp

### Open Source License

Delta CLI is released under the **MIT License**, which means:
- ✅ Free to use for any purpose
- ✅ Free to modify and distribute
- ✅ Commercial use allowed
- ✅ No warranty provided

See [LICENSE](LICENSE) for full details.

### Built on llama.cpp

Delta CLI is built on top of [llama.cpp](https://github.com/ggerganov/llama.cpp), a popular open-source project for running LLMs efficiently. This provides:

- **Proven Performance**: llama.cpp is battle-tested and optimized
- **Active Development**: Regular updates and improvements
- **Community Support**: Large community and extensive documentation
- **Model Compatibility**: Supports all GGUF format models
- **GPU Acceleration**: CUDA, Metal, Vulkan, ROCm support
- **Quantization**: Efficient model compression (Q4, Q5, Q8, etc.)

### llama.cpp Features Available in Delta CLI

- ✅ Multiple quantization formats
- ✅ GPU acceleration (CUDA, Metal, Vulkan, ROCm)
- ✅ Multimodal inputs (text + images)
- ✅ Efficient inference algorithms
- ✅ Context management
- ✅ Streaming responses
- ✅ Chat templates
- ✅ Grammar constraints

---

## Contributing

Contributions are welcome! Delta CLI is open source and community-driven.

1. Fork the repository
2. Create a feature branch
3. Make your changes
4. Submit a pull request

See [CONTRIBUTING.md](CONTRIBUTING.md) for detailed guidelines.

---

## Support & Resources

- **Issues**: [GitHub Issues](https://github.com/nile-agi/delta/issues)
- **Repository**: [https://github.com/nile-agi/delta](https://github.com/nile-agi/delta)
- **Documentation**: This README and project files

---

## Acknowledgments

- [llama.cpp](https://github.com/ggerganov/llama.cpp) - Efficient LLM inference engine
- [Hugging Face](https://huggingface.co) - Model hosting and distribution
- All contributors and users of Delta CLI

---

## License

Delta CLI is released under the MIT License. See [LICENSE](LICENSE) for details.

---

<div align="center">

**Made with ❤️ by the Delta CLI Team**

*Run AI anywhere, anytime, offline.*

[⭐ Star us on GitHub](https://github.com/nile-agi/delta) | [🐛 Report Issues](https://github.com/nile-agi/delta/issues)

</div>
