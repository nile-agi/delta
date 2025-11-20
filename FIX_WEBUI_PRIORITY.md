# Fix: Model Management Not Showing with ./delta

## The Problem

When you run `./delta`, it uses the **old web UI** from Homebrew (`/opt/homebrew/share/delta-cli/webui`) which doesn't have the Model Management component.

When you run `./delta-server`, it uses the **new web UI** from `public/` which has the Model Management component.

## The Fix

I've updated the code to check `public/` directory **first** before checking Homebrew locations. This means:

- **Development builds** (running `./delta` from build directory) will use `public/` with latest changes
- **Installed versions** (via Homebrew) will still use the installed web UI

## After Rebuilding

1. **Rebuild delta:**
   ```bash
   cd /Users/suzanodero/delta/build_macos
   make delta
   ```

2. **Restart delta:**
   ```bash
   ./delta
   ```

3. **Verify it's using the right web UI:**
   - Look at the process: `ps aux | grep llama-server`
   - Check the `--path` argument - it should point to `public/` not `/opt/homebrew/share/delta-cli/webui`

4. **Hard refresh browser:**
   - `Cmd + Shift + R` (Mac) or `Ctrl + Shift + R` (Windows)

5. **Navigate to Settings → Developer**

You should now see the Model Management section!

## Alternative: Update Homebrew Web UI

If you want to keep using Homebrew's web UI location, you can copy the new build:

```bash
# Backup old web UI
sudo mv /opt/homebrew/share/delta-cli/webui /opt/homebrew/share/delta-cli/webui.backup

# Copy new web UI
sudo cp -r /Users/suzanodero/delta/public /opt/homebrew/share/delta-cli/webui
```

But the fix above is better - it automatically uses the local `public/` directory for development.

