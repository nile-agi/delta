# Fix PATH Conflict with Delta CLI

## Problem

The `delta` command points to llvm's delta tool instead of Delta CLI because `/opt/homebrew/opt/llvm@18/bin` comes before `/opt/homebrew/bin` in your PATH.

## Quick Fix (Temporary)

Use the full path:
```bash
/opt/homebrew/bin/delta --version
```

## Permanent Fix

Add this to your `~/.zshrc` file:

```bash
# Ensure Homebrew bin comes before llvm in PATH
export PATH="/opt/homebrew/bin:/opt/homebrew/sbin:$PATH"
```

Then reload your shell:
```bash
source ~/.zshrc
```

Or restart your terminal.

## Verify

After fixing, check:
```bash
which delta
# Should show: /opt/homebrew/bin/delta

delta --version
# Should show: Delta CLI v1.0.0
```

## Alternative: Create an Alias

If you prefer not to modify PATH, add this to `~/.zshrc`:

```bash
alias delta='/opt/homebrew/bin/delta'
```

Then reload: `source ~/.zshrc`

