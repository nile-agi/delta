# Fixing Homebrew Installation Issue

## Problem

When installing via Homebrew, you get this error:
```
fatal: No url found for submodule path 'homebrew-delta-cli' in .gitmodules
```

## Root Cause

The `homebrew-delta-cli` directory is a nested git repository inside the delta repository. When Homebrew clones with `submodules: true`, it tries to recursively process all submodules, including `homebrew-delta-cli`, which isn't in `.gitmodules`.

## Solution Applied

1. **Removed `submodules: true` from head line** - This prevents automatic recursive submodule processing
2. **Handle submodules manually** - Only initialize `vendor/llama.cpp` explicitly
3. **Added to .gitignore** - `homebrew-delta-cli/` is now ignored in the delta repo

## Updated Formula

The formula now:
- Clones the repository without automatic submodule processing
- Manually initializes only `vendor/llama.cpp` submodule
- Recursively updates llama.cpp's submodules if needed
- Ignores nested repos like `homebrew-delta-cli`

## Testing

After pushing the fix:

```bash
brew untap nile-agi/delta-cli
brew tap nile-agi/delta-cli
brew install --HEAD delta-cli
```

## For Non-Developers

**Best option:** Use the installation script (no git required):
```bash
curl -fsSL https://raw.githubusercontent.com/nile-agi/delta/main/install.sh | bash
```

This downloads pre-built binaries and doesn't require git or GitHub.

