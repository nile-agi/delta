# Fix: Homebrew Tap Name

## Issue

When running `brew tap nile-agi/delta`, Homebrew looks for a repository named `homebrew-delta` at `github.com/nile-agi/homebrew-delta`, but the actual repository is named `homebrew-delta-cli`.

## Solution

Use the correct tap name that matches the repository:

```bash
brew tap nile-agi/delta-cli && brew install delta
```

## Homebrew Tap Naming Convention

Homebrew automatically adds the `homebrew-` prefix:
- `brew tap user/repo` → looks for `github.com/user/homebrew-repo`
- `brew tap nile-agi/delta` → looks for `github.com/nile-agi/homebrew-delta` ❌
- `brew tap nile-agi/delta-cli` → looks for `github.com/nile-agi/homebrew-delta-cli` ✅

## Alternative: Rename Repository

If you want `brew tap nile-agi/delta` to work, you need to:

1. **Rename the GitHub repository** from `homebrew-delta-cli` to `homebrew-delta`
2. **Update all documentation** to use `nile-agi/delta`

Or keep the current name and use `nile-agi/delta-cli` (recommended - less confusing).

## Important Note

**Homebrew taps are git repositories** - this is how Homebrew works. When you run `brew tap`, it clones the tap repository. This is normal and required.

However, **the formula itself** (inside the tap) downloads pre-built binaries - no git, no building. The tap cloning is just to get the formula file.

