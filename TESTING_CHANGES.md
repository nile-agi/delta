# Testing Guide for Model Switching Fixes

This guide will help you test the recent changes to model switching functionality.

## Prerequisites

- Make sure you have at least 2 models installed (e.g., "Qwen 2.5 0.5B" and "Qwen 3 1.7B")
- Check installed models: `delta --list-models` or `delta /list`

## Step 1: Rebuild the Project

### 1.1 Rebuild the C++ Backend

The changes to `src/model_api_server.cpp` require rebuilding the C++ executable:

```bash
cd /Users/suzanodero/io/GITHUB/delta

# Navigate to your build directory (adjust for your platform)
cd build_macos  # or build_macos_arm64, build_macos_release, etc.

# Rebuild
make

# Or if you need to reconfigure:
cmake .. -DCMAKE_BUILD_TYPE=Release
make
```

### 1.2 Rebuild the Frontend (Web UI)

The changes to `assets/src/lib/stores/models.svelte.ts` and `assets/src/lib/services/models.ts` require rebuilding the web UI:

```bash
cd /Users/suzanodero/io/GITHUB/delta/assets

# Install dependencies if needed
npm install

# Build the web UI
npm run build
```

This will:
- Compile TypeScript/Svelte files
- Copy the built files to the `public/` directory
- Make them available to the server

## Step 2: Start the Application

### Option A: Using the build script

```bash
cd /Users/suzanodero/io/GITHUB/delta
./build-and-run.sh
```

### Option B: Manual start

```bash
# Terminal 1: Start the delta server
cd /Users/suzanodero/io/GITHUB/delta/build_macos
./delta server

# Or if installed system-wide:
delta server
```

The server should show:
```
Model Management API server running on http://127.0.0.1:8081
Delta Server started on http://localhost:8080
```

## Step 3: Test the Fixes

### Test 1: Model Dropdown Shows Display Names (Not Filenames)

1. Open http://localhost:8080 in your browser
2. Look at the model selector dropdown (top right or in the input area)
3. **Expected**: You should see display names like:
   - "Qwen 3 1.7B" ✅
   - "Qwen 2.5 0.5B" ✅
   - "Gemma 3 270M" ✅
4. **NOT expected**: Filenames like:
   - "Qwen3-1.7B-f16.gguf" ❌
   - "Qwen2.5-0.5B-Q4_K_M.gguf" ❌

**How to verify:**
- Click on the model selector dropdown
- Check that all model names are human-readable short names
- No `.gguf` extensions should be visible

### Test 2: Model Switching Works

1. Open http://localhost:8080
2. Click on the model selector dropdown
3. Select a different model (e.g., switch from "Qwen 3 1.7B" to "Qwen 2.5 0.5B")
4. **Expected behavior:**
   - The dropdown should show a loading indicator
   - The model should switch successfully
   - The selected model name should update in the UI
   - You should see the new model's display name in the dropdown

**How to verify:**
- Watch the browser console (F12 → Console tab) for any errors
- The model selector should update to show the new model
- The server logs should show the model switch and server restart

### Test 3: Page Refresh After Model Switch (Connection Refused Fix)

This is the critical test for the connection refused error:

1. Open http://localhost:8080
2. Switch to a different model (e.g., "Qwen 2.5 0.5B")
3. **Wait 3-5 seconds** for the server to restart
4. **Refresh the page** (F5 or Cmd+R)
5. **Expected**: 
   - Page should load successfully ✅
   - No "ERR_CONNECTION_REFUSED" error ✅
   - The correct model should be displayed ✅

**How to verify:**
- The page should reload without errors
- Check browser console (F12) - should see no connection errors
- The model selector should show the correct model
- You should be able to send a message and get a response

### Test 4: Server Restart Handling

1. Open http://localhost:8080
2. Open browser DevTools (F12) → Network tab
3. Switch to a different model
4. **Expected behavior:**
   - You should see the API call to `/api/models/use`
   - The server should restart (check terminal logs)
   - The frontend should retry fetching models with exponential backoff
   - After 2-3 seconds, the UI should update with the new model

**How to verify:**
- Check terminal logs for server restart messages
- Check browser console for retry attempts (if any)
- The UI should eventually show the correct model

## Step 4: Debugging Tips

### If the dropdown still shows filenames:

1. **Clear browser cache:**
   ```bash
   # Hard refresh: Cmd+Shift+R (Mac) or Ctrl+Shift+R (Windows/Linux)
   ```

2. **Check if the API is returning display names:**
   ```bash
   curl http://localhost:8081/api/models/list
   ```
   Should return JSON with `display_name` fields.

3. **Check browser console:**
   - Open DevTools (F12)
   - Look for errors in Console tab
   - Check Network tab for failed API calls

### If you get connection refused errors:

1. **Check if the server is running:**
   ```bash
   ps aux | grep llama-server
   ```

2. **Check server logs:**
   - Look at the terminal where you started `delta server`
   - Should see "Model Management API server running on http://127.0.0.1:8081"

3. **Wait longer:**
   - Server restart can take 3-5 seconds
   - Try refreshing after waiting 5 seconds

4. **Check ports:**
   ```bash
   lsof -i :8080  # Web UI server
   lsof -i :8081  # Model API server
   ```

### If model switching doesn't work:

1. **Check API endpoint:**
   ```bash
   curl -X POST http://localhost:8081/api/models/use \
     -H "Content-Type: application/json" \
     -d '{"model": "qwen2.5:0.5b"}'
   ```

2. **Check if model is installed:**
   ```bash
   delta --list-models
   ```

3. **Check browser console:**
   - Look for JavaScript errors
   - Check Network tab for failed requests

## Step 5: Verification Checklist

- [ ] Model dropdown shows display names (not filenames)
- [ ] Can switch between models successfully
- [ ] Page refresh works after model switch (no connection refused)
- [ ] Server restarts automatically when switching models
- [ ] UI shows loading state during model switch
- [ ] No errors in browser console
- [ ] No errors in server logs

## Quick Test Script

You can also use this quick test:

```bash
#!/bin/bash
# Quick test script

echo "Testing model API..."
curl -s http://localhost:8081/api/models/list | jq '.[0].display_name'

echo "Testing model switch..."
curl -X POST http://localhost:8081/api/models/use \
  -H "Content-Type: application/json" \
  -d '{"model": "qwen2.5:0.5b"}' | jq '.message'

echo "Waiting for server restart..."
sleep 3

echo "Testing web UI..."
curl -s http://localhost:8080 | head -20
```

## Common Issues and Solutions

### Issue: "Cannot find module '$lib/services/models'"

**Solution:** Rebuild the frontend:
```bash
cd assets
npm run build
```

### Issue: "Model not found" error

**Solution:** Make sure the model name uses the correct format:
- Use registry name: `qwen2.5:0.5b` (with colon)
- Not filename: `Qwen2.5-0.5B-Q4_K_M.gguf`

### Issue: Server doesn't restart

**Solution:** Check if `Commands::launch_server_auto` is working:
- Check server logs
- Verify the model path is correct
- Make sure port 8080 is available

## Next Steps

After testing, if everything works:
1. The dropdown should show friendly names like "Qwen 3 1.7B"
2. Model switching should work smoothly
3. Page refresh should work without connection errors

If you encounter any issues, check the debugging tips above or review the server/browser console logs.

