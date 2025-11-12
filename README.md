# Delta CLI

<div align="center">

```
╔══════════════════════════════════════════════════════════════╗
║                                                              ║
║    ██████╗ ███████╗██╗  ████████╗ █████╗      ██████╗██╗     ║
║    ██╔══██╗██╔════╝██║  ╚══██╔══╝██╔══██╗    ██╔════╝██║     ║
║    ██║  ██║█████╗  ██║     ██║   ███████║    ██║     ██║     ║
║    ██║  ██║██╔══╝  ██║     ██║   ██╔══██║    ██║     ██║     ║
║    ██████╔╝███████╗███████╗██║   ██║  ██║    ╚██████╗███████║║
║    ╚═════╝ ╚══════╝╚══════╝╚═╝   ╚═╝  ╚═╝     ╚═════╝╚══════╝║
║                                                              ║
║           Offline AI Assistant powered by llama.cpp          ║
╚══════════════════════════════════════════════════════════════╝
```

**Run powerful AI models locally, completely offline, on any device.**

[![License](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)
[![Open Source](https://img.shields.io/badge/open--source-Yes-green.svg)](https://github.com/oderoi/delta-cli)
[![Built on llama.cpp](https://img.shields.io/badge/built%20on-llama.cpp-orange.svg)](https://github.com/ggerganov/llama.cpp)

</div>

---

## What is Delta CLI?

Delta CLI is an **open-source, offline-first AI assistant** that runs large language models (LLMs) directly on your device. Built on top of [llama.cpp](https://github.com/ggerganov/llama.cpp), Delta CLI provides a simple command-line interface to interact with AI models without requiring internet connectivity or cloud services.

### Key Features

- 🔒 **100% Offline**: Works completely offline after initial model download
- ⚡ **High Performance**: Full GPU acceleration (CUDA, Metal, Vulkan, ROCm)
- 🌐 **Cross-Platform**: Runs on desktop, mobile, and edge devices
- 🎨 **Beautiful Terminal UI**: Retro-green interface with custom styling
- 📦 **Easy Model Management**: One-command downloads from Hugging Face
- 🚀 **Zero-Setup**: Auto-downloads default model on first run
- 🔧 **llama.cpp Integration**: Access to all llama.cpp features and optimizations

## What Problem Does Delta Solve?

### Privacy & Security
- **No data leaves your device**: All processing happens locally
- **No API keys required**: No need to sign up for cloud services
- **Complete control**: Your conversations and data stay private

### Cost & Accessibility
- **No subscription fees**: Run AI models without recurring costs
- **No usage limits**: Use as much as you want, whenever you want
- **Works offline**: Perfect for areas with poor internet connectivity

### Flexibility & Control
- **Choose your model**: Use any compatible GGUF model
- **Customize settings**: Full control over temperature, context size, and more
- **No vendor lock-in**: Open source and self-hosted

### Use Cases
- **Development**: Code assistance, documentation, debugging
- **Writing**: Content creation, editing, brainstorming
- **Learning**: Educational Q&A, explanations, tutorials
- **Research**: Data analysis, summarization, research assistance
- **Privacy-sensitive tasks**: When you can't send data to cloud services

## Who is Delta CLI For?

### Developers
- Want to integrate AI into their workflow without API dependencies
- Need offline AI capabilities for development environments
- Prefer command-line tools and automation

### Privacy-Conscious Users
- Concerned about data privacy and security
- Want to keep conversations and data local
- Work with sensitive information

### Cost-Conscious Users
- Want to avoid subscription fees
- Need unlimited usage without per-request costs
- Prefer one-time setup over ongoing expenses

### Offline Users
- Work in areas with limited or no internet
- Need reliable AI assistance without connectivity
- Want to reduce dependency on cloud services

### Hobbyists & Enthusiasts
- Interested in running AI models locally
- Want to experiment with different models
- Enjoy tinkering with open-source tools

## Advantages of Delta CLI

### 1. **Privacy First**
- All processing happens on your device
- No data sent to external servers
- Complete control over your information

### 2. **Cost Effective**
- No subscription fees
- No per-request charges
- One-time setup, unlimited usage

### 3. **High Performance**
- GPU acceleration support (CUDA, Metal, Vulkan, ROCm)
- Optimized inference via llama.cpp
- Efficient memory usage with quantization

### 4. **Easy to Use**
- Simple command-line interface
- Auto-downloads default model
- Intuitive interactive mode

### 5. **Flexible & Customizable**
- Support for multiple models
- Adjustable parameters (temperature, context, etc.)
- Extensible architecture

### 6. **Cross-Platform**
- Works on major operating systems
- Mobile device support
- Edge device compatibility

### 7. **Open Source**
- MIT License - free to use and modify
- Built on llama.cpp (also open source)
- Community-driven development

### 8. **No Vendor Lock-In**
- Use any compatible model
- No dependency on specific services
- Portable and self-contained

## Supported Platforms

Delta CLI runs on **5 major platforms**:

### Desktop Platforms
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

### Mobile Platforms
- ✅ **Android** (ARM64, ARM)
  - Termux support
  - Android NDK builds
  - Native compilation

- ✅ **iOS** (ARM64)
  - Xcode integration
  - Device and simulator support
  - Framework packaging

### Edge Devices
- ✅ Raspberry Pi (3, 4, 5)
- ✅ NVIDIA Jetson
- ✅ Other ARM-based devices

## Installation

### Quick Installation (One Command)

Each platform has a dedicated installation script that handles everything automatically:

#### macOS
```bash
./install-macos.sh
```

**What it does:**
- Checks/installs Xcode Command Line Tools
- Checks/installs Homebrew
- Installs dependencies (cmake, git, curl)
- Builds Delta CLI with Metal acceleration
- Installs to `/usr/local/bin/delta`
- Configures PATH automatically

#### Linux
```bash
./install-linux.sh
```

**What it does:**
- Detects your package manager (apt, yum, dnf, pacman, apk, zypper)
- Updates package lists
- Installs build dependencies
- Builds Delta CLI (with optional CUDA/Vulkan support)
- Installs to `/usr/local/bin/delta`
- Configures PATH automatically

**Supported distributions:**
- Ubuntu/Debian (apt)
- Fedora/RHEL/CentOS (yum/dnf)
- Arch Linux (pacman)
- Alpine Linux (apk)
- openSUSE (zypper)

#### Windows
```powershell
# Run PowerShell as Administrator
.\install-windows.ps1
```

**What it does:**
- Checks for Visual Studio or Build Tools
- Checks/installs Chocolatey (optional)
- Installs dependencies (CMake, Git)
- Sets up build environment
- Builds Delta CLI
- Installs to `C:\Program Files\Delta CLI`
- Adds to system PATH

**Requirements:**
- Windows 10 or later
- PowerShell 5.1+
- Visual Studio 2019/2022 with C++ workload OR Build Tools

#### Android (Termux)
```bash
# In Termux
./install-android.sh
```

**What it does:**
- Detects Termux or Android NDK environment
- Installs build dependencies via Termux package manager
- Builds Delta CLI natively (Termux) or cross-compiles (NDK)
- Installs to `$PREFIX/local/bin/delta`
- Configures PATH in `~/.bashrc`

**Note:** Building and running AI models on mobile devices may be slow. Consider using smaller models.

#### iOS
```bash
# Requires macOS with Xcode
./install-ios.sh
```

**What it does:**
- Checks for Xcode
- Downloads/updates iOS CMake toolchain
- Checks for CMake and Git
- Builds Delta CLI for iOS
- Provides integration instructions

**Important:** iOS requires code signing and app packaging. The binary must be integrated into an Xcode project.

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

### Troubleshooting Installation

**Command not found:**
- **macOS/Linux**: Restart terminal or run `export PATH="/usr/local/bin:$PATH"`
- **Windows**: Restart PowerShell or run `$env:Path += ";C:\Program Files\Delta CLI"`
- **Android**: Run `source ~/.bashrc` in Termux

**Build errors:**
- Ensure all dependencies are installed (scripts handle this automatically)
- Check CMake version (3.14+ required)
- Verify C++ compiler is installed
- Check disk space (build requires several GB)

## Quick Start

### 1. Run Delta CLI

```bash
# Just type 'delta' and press Enter
delta
```

**What happens:**
1. Shows banner and welcome message
2. Auto-downloads default model (`qwen3:0.6b`, ~400MB) if not installed
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
delta pull qwen3:0.6b      # 400 MB - default, fast
delta pull llama3.1:8b     # 4.7 GB - powerful, versatile
delta pull mistral:7b      # 4.3 GB - great for coding
```

### 4. Use Specific Model

```bash
# Use a specific model
delta --model llama3.1:8b "Your question here"

# With GPU acceleration
delta --model qwen3:0.6b --gpu-layers -1 "Your question"
```

## Updating Delta CLI

### Method 1: Re-run Installation Script

The easiest way to update is to re-run the installation script for your platform:

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

The scripts will rebuild and reinstall Delta CLI with the latest changes. **Your models and settings are preserved during updates.**

### Method 2: From Source (Git)

If you installed from source:

```bash
# Navigate to delta-cli directory
cd ~/delta-cli

# Pull latest changes
git pull

# Rebuild
./installers/build_macos.sh  # or build_linux.sh, etc.

# Reinstall
cd build_macos  # or build_linux, etc.
sudo cmake --install .
```

### Check for Updates

```bash
# Check your current version
delta --version

# Check latest release (GitHub)
curl -s https://api.github.com/repos/oderoi/delta-cli/releases/latest | grep '"tag_name"'
```

## Usage Examples

### Basic Usage

```bash
# Interactive mode
delta

# Single query
delta "What is the capital of France?"

# With specific model
delta --model llama3.1:8b "Explain neural networks"
```

### Advanced Usage

```bash
# With GPU acceleration
delta --gpu-layers -1 "Your prompt"

# Custom settings
delta --model mistral:7b \
      --temperature 0.7 \
      --tokens 1024 \
      --ctx-size 4096 \
      "Write a detailed article"

# List local models
delta --list-models

# Remove a model
delta remove qwen3:0.6b
```

### Interactive Commands

In interactive mode, use slash commands:

```bash
/download <model>     # Download a model
/use <model>         # Switch to another model
/list                # List local models
/tokens <N>          # Set max tokens
/temperature <F>     # Set temperature
/gpu-layers <N>     # Set GPU layers
/server              # Start web dashboard
/help                # Show all commands
```

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

## Project Structure

```
delta-cli/
├── src/              # Source code
│   ├── main.cpp      # Main entry point
│   ├── inference.cpp # Model inference
│   ├── models.cpp    # Model management
│   └── ...
├── vendor/
│   └── llama.cpp/    # llama.cpp integration
├── installers/       # Platform-specific build scripts
├── install-*.sh      # Platform-specific installation scripts
└── README.md         # This file
```

## Contributing

Contributions are welcome! Delta CLI is open source and community-driven.

1. Fork the repository
2. Create a feature branch
3. Make your changes
4. Submit a pull request

See [CONTRIBUTING.md](CONTRIBUTING.md) for detailed guidelines.

## Support & Resources

- **Issues**: [GitHub Issues](https://github.com/oderoi/delta-cli/issues)
- **Discussions**: [GitHub Discussions](https://github.com/oderoi/delta-cli/discussions)
- **Documentation**: See project files for detailed guides

## Acknowledgments

- [llama.cpp](https://github.com/ggerganov/llama.cpp) - Efficient LLM inference engine
- [Hugging Face](https://huggingface.co) - Model hosting and distribution
- All contributors and users of Delta CLI

## License

Delta CLI is released under the MIT License. See [LICENSE](LICENSE) for details.

---

<div align="center">

**Made with ❤️ by the Delta CLI Team**

*Run AI anywhere, anytime, offline.*

[⭐ Star us on GitHub](https://github.com/oderoi/delta-cli) | [📖 Documentation](#) | [🐛 Report Issues](https://github.com/oderoi/delta-cli/issues)

</div>
