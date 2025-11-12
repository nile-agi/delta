# Installation Instructions

Delta CLI can be installed via multiple package managers on different platforms.

## Windows

### Winget (Windows Package Manager)

```powershell
winget install DeltaCLI.DeltaCLI
```

## macOS

### Homebrew

```bash
# Using tap (before official inclusion)
brew install oderoi/delta-cli/delta-cli

# Or after official inclusion
brew install delta-cli
```

### MacPorts

```bash
sudo port install delta-cli
```

### Nix

```bash
# Using nix-env
nix-env -iA nixpkgs.delta-cli

# Or using Nix Flakes
nix profile install nixpkgs#delta-cli
```

## Linux

### Homebrew (Linux)

```bash
# Using tap (before official inclusion)
brew install oderoi/delta-cli/delta-cli

# Or after official inclusion
brew install delta-cli
```

### Nix

```bash
# Using nix-env
nix-env -iA nixpkgs.delta-cli

# Or using Nix Flakes
nix profile install nixpkgs#delta-cli
```

## Manual Installation

If package managers are not available, you can install manually:

### Download Pre-built Binaries

1. Download the appropriate package from [GitHub Releases](https://github.com/oderoi/delta-cli/releases)
2. Extract the archive
3. Copy `delta` and `delta-server` to a directory in your PATH (e.g., `/usr/local/bin` on Unix, or `C:\Program Files\Delta CLI\` on Windows)
4. Ensure the binaries are executable (Unix): `chmod +x delta delta-server`

### Build from Source

See [BUILD_GUIDE.md](../BUILD_GUIDE.md) for detailed build instructions.

## Verification

After installation, verify it works:

```bash
delta --version
```

You should see the Delta CLI version information.

## Next Steps

1. Run `delta` to start the interactive CLI
2. Use `/help` to see available commands
3. Use `/download <model>` to download your first model
4. Start chatting with AI models offline!

For more information, visit the [main README](../README.md).

