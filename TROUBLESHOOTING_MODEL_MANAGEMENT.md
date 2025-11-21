# Troubleshooting Model Management Not Showing

If you don't see the Model Management section in Developer Settings, try these steps:

## Step 1: Verify the API Server is Running

```bash
# Check if model API server is running
curl http://localhost:8081/api/models/list

# Should return JSON with your installed models
```

If this fails, the model API server isn't running. Make sure you:
1. Rebuilt delta with the latest changes
2. Restarted delta after rebuilding

## Step 2: Hard Refresh Browser

**Mac:**
- Press `Cmd + Shift + R`
- Or open DevTools (F12) → Right-click refresh button → "Empty Cache and Hard Reload"

**Windows/Linux:**
- Press `Ctrl + Shift + R`
- Or open DevTools (F12) → Right-click refresh button → "Empty Cache and Hard Reload"

## Step 3: Check Browser Console

1. Open DevTools (F12)
2. Go to Console tab
3. Look for any errors related to:
   - `ModelManagementTab`
   - `ModelsService`
   - `localhost:8081`
   - CORS errors

## Step 4: Verify Files Are Built

```bash
# Check if ModelManagementTab exists
ls -la assets/src/lib/components/app/chat/ModelManagement/ModelManagementTab.svelte

# Check if it's in the built HTML
grep -i "Model Management" public/index.html | head -1
```

## Step 5: Restart Delta

1. Stop the current delta process (Ctrl+C)
2. Rebuild delta:
   ```bash
   cd build_macos
   make delta
   ```
3. Restart delta:
   ```bash
   ./delta
   ```
4. Look for this message:
   ```
   Model Management API: http://localhost:8081
   ```

## Step 6: Check Network Tab

1. Open DevTools (F12)
2. Go to Network tab
3. Navigate to Settings → Developer
4. Look for requests to `localhost:8081`
5. Check if they're failing (red) or succeeding (green)

## Common Issues

### Issue: "Failed to fetch" errors
**Solution:** The model API server isn't running. Restart delta.

### Issue: CORS errors
**Solution:** Make sure you're accessing from `localhost`, not `127.0.0.1` or another domain.

### Issue: Component doesn't render
**Solution:** 
1. Check browser console for JavaScript errors
2. Verify the component file exists
3. Rebuild the web UI: `cd assets && npm run build`

### Issue: Empty lists
**Solution:** 
1. Check if you have models installed: `delta --list-models`
2. Verify the API is working: `curl http://localhost:8081/api/models/list`

## Quick Test

Run these commands to verify everything:

```bash
# 1. Check API server
curl http://localhost:8081/api/models/list

# 2. Check if delta was rebuilt recently
ls -lh build_macos/delta

# 3. Check if web UI was rebuilt
ls -lh public/index.html

# 4. Verify component exists
test -f assets/src/lib/components/app/chat/ModelManagement/ModelManagementTab.svelte && echo "OK" || echo "MISSING"
```

If all checks pass but you still don't see it, try:
1. Clear browser cache completely
2. Try a different browser
3. Check if there are JavaScript errors in the console

