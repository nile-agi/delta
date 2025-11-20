# Installing Delta CLI on Ubuntu

## About the "Potentially Unsafe" Warning

When installing Delta CLI on Ubuntu, you may see a warning that the package is "potentially unsafe" or from a "third party." This is **normal and expected** for packages that are:

- Not signed with a GPG key
- Not from Ubuntu's official repositories
- Downloaded directly from GitHub or other sources

**The package is safe to install.** This warning appears for all third-party .deb packages, including many popular open-source applications.

## Installation Methods

### Method 1: Command Line (Recommended)

This method gives you more control and information:

```bash
# Download the .deb file
wget https://github.com/oderoi/delta-cli/releases/latest/download/delta-cli_1.0.0_amd64.deb

# Verify the package (optional but recommended)
dpkg-deb -I delta-cli_1.0.0_amd64.deb

# Install the package
sudo dpkg -i delta-cli_1.0.0_amd64.deb

# Fix any dependency issues
sudo apt-get install -f
```

### Method 2: Ubuntu Software Center / App Center

1. Download the `.deb` file
2. Double-click the file to open it in App Center
3. Click **"Install"** when prompted
   - You'll see a warning about third-party packages - this is normal
   - Click **"Install"** again to confirm
4. Enter your password when prompted

### Method 3: GDebi Package Installer

GDebi provides a better installation experience:

```bash
# Install GDebi
sudo apt-get install gdebi

# Install the package with GDebi
sudo gdebi delta-cli_1.0.0_amd64.deb
```

## Verifying the Package

Before installing, you can verify the package contents:

```bash
# List all files in the package
dpkg-deb -c delta-cli_1.0.0_amd64.deb

# Show package information
dpkg-deb -I delta-cli_1.0.0_amd64.deb

# Extract package contents (without installing)
dpkg-deb -x delta-cli_1.0.0_amd64.deb /tmp/delta-cli-extracted
```

## Why the Warning Appears

Ubuntu shows this warning because:

1. **No GPG Signature**: The package isn't signed with a GPG key
2. **Not in Official Repos**: It's not in Ubuntu's official repositories
3. **Security Precaution**: Ubuntu errs on the side of caution for user safety

This is the same warning you'd see for:
- Google Chrome
- VS Code
- Many other popular third-party applications

## Trust and Verification

Delta CLI is:
- ✅ **Open Source**: Source code available on GitHub
- ✅ **Transparent**: You can review the code before installing
- ✅ **Verifiable**: Package contents can be inspected
- ✅ **Community Maintained**: Active development and support

**Source Code**: https://github.com/oderoi/delta-cli

## After Installation

Once installed, verify it works:

```bash
# Check version
delta --version

# Get help
delta --help

# Start using Delta CLI
delta pull qwen3:0.6b    # Download a model
delta                     # Start chatting
```

## Uninstalling

To remove Delta CLI:

```bash
sudo dpkg -r delta-cli
```

Or use App Center:
1. Open App Center
2. Search for "Delta CLI"
3. Click "Remove"

## Troubleshooting

### "Dependency errors" after installation

```bash
sudo apt-get install -f
```

This will install any missing dependencies.

### "Permission denied" errors

Make sure you're using `sudo` for installation:
```bash
sudo dpkg -i delta-cli_*.deb
```

### Package won't install

Check the architecture matches your system:
```bash
# Check your system architecture
dpkg --print-architecture

# Check package architecture
dpkg-deb -I delta-cli_*.deb | grep Architecture
```

They should match (usually `amd64` for 64-bit systems).

## Security Best Practices

1. **Download from Official Source**: Always download from GitHub releases
2. **Verify Checksums**: Check SHA256 checksums if provided
3. **Review Source Code**: Review the code on GitHub before installing
4. **Keep Updated**: Update to the latest version regularly
5. **Report Issues**: Report any security concerns on GitHub

## Getting Help

- **GitHub Issues**: https://github.com/oderoi/delta-cli/issues
- **Documentation**: https://github.com/oderoi/delta-cli#readme
- **Community**: Check the GitHub discussions

---

**Note**: The "potentially unsafe" warning is a standard Ubuntu security feature. Delta CLI is safe to install and use. The warning appears because the package isn't from Ubuntu's official repositories, not because there's anything wrong with the software.

