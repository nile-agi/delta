# Delta CLI - Build and Run Guide

## Quick Start (Automated)

Run the automated build and run script:

```bash
cd /Users/suzanodero/Downloads/delta-cli
./build-and-run.sh
```

This script will:
1. Stop any running server
2. Rebuild the web UI
3. Build Delta CLI
4. Install Delta CLI system-wide
5. Start the Delta server

Then open **http://localhost:8080** in your browser.

---

## Manual Build Steps

If you prefer to build manually:

### Step 1: Rebuild Web UI

```bash
cd /Users/suzanodero/Downloads/delta-cli/vendor/llama-cpp/tools/server/webui
npm install          # Only needed first time
npm run build
./scripts/post-build.sh
cd /Users/suzanodero/Downloads/delta-cli
```

### Step 2: Build Delta CLI

```bash
cd /Users/suzanodero/Downloads/delta-cli/build_macos
make clean           # Optional: clean previous build
cmake .. -DCMAKE_BUILD_TYPE=Release
make
cd ..
```

### Step 3: Install Delta CLI

```bash
cd /Users/suzanodero/Downloads/delta-cli/build_macos
sudo make install
cd ..
```

### Step 4: Run Delta

```bash
# Stop any existing server
pkill -f llama-server

# Start Delta
delta server
```

Or just run:
   ```bash
delta
```

Then open **http://localhost:8080** in your browser.

---

## Troubleshooting

### Blank Page

1. **Hard refresh your browser:**
   - Mac: `Cmd+Shift+R`
   - Windows/Linux: `Ctrl+Shift+R`

2. **Check browser console:**
   - Open Developer Tools (F12 or Cmd+Option+I)
   - Check Console tab for JavaScript errors
   - Check Network tab for 404 errors

3. **Verify server path:**
   ```bash
   ps aux | grep llama-server | grep -- '--path'
   ```
   Should show an absolute path like:
   ```
   --path /Users/suzanodero/Downloads/delta-cli/vendor/llama-cpp/tools/server/public
   ```

4. **Rebuild web UI:**
   ```bash
   cd /Users/suzanodero/Downloads/delta-cli/vendor/llama-cpp/tools/server/webui
   npm run build
   ./scripts/post-build.sh
   ```

### Port 8080 Already in Use

   ```bash
# Find and kill the process using port 8080
lsof -ti:8080 | xargs kill -9

# Or kill all llama-server processes
   pkill -f llama-server
   ```

### Web UI Shows "llama.cpp" Instead of "Delta"

1. Rebuild the web UI (Step 1 above)
2. Rebuild and reinstall Delta CLI (Steps 2-3)
3. Restart the server

---

## Verification

After building and running, verify everything is working:

   ```bash
# Check server is running with correct path
ps aux | grep llama-server | grep -- '--path'

# Test web UI is accessible
curl -sS http://localhost:8080 | head -10

# Check JavaScript files are accessible
curl -sI http://localhost:8080/_app/immutable/entry/start.*.js | head -1
   ```
   
---

## Development Workflow

For development, you can rebuild just the parts you changed:

**After changing web UI files:**
```bash
cd vendor/llama-cpp/tools/server/webui
npm run build
./scripts/post-build.sh
```

**After changing C++ code:**
```bash
cd build_macos
make
sudo make install
```

**After any changes, restart server:**
```bash
pkill -f llama-server
delta server
```
