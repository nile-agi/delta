# Final Fix Instructions

## The Problem
The Model Management component is built and integrated, but not showing in the browser.

## Solution Steps

### 1. Stop Delta
```bash
# Press Ctrl+C in the terminal where delta is running
# Or kill the process:
pkill -f delta
pkill -f llama-server
```

### 2. Clean and Rebuild Everything
```bash
cd /Users/suzanodero/delta

# Clean web UI build
rm -rf public/*
rm -rf assets/.svelte-kit

# Rebuild web UI
cd assets
npm run build

# Rebuild delta executable
cd ../build_macos
make delta
```

### 3. Restart Delta
```bash
cd /Users/suzanodero/delta/build_macos
./delta
```

**Look for this message:**
```
Model Management API: http://localhost:8081
```

### 4. Clear Browser Cache Completely
**Option A: Hard Refresh**
- Mac: `Cmd + Shift + R`
- Windows/Linux: `Ctrl + Shift + R`

**Option B: Clear Cache via DevTools**
1. Open DevTools (F12)
2. Right-click the refresh button
3. Select "Empty Cache and Hard Reload"

**Option C: Clear All Site Data**
1. Open DevTools (F12)
2. Go to Application tab
3. Click "Clear storage" in left sidebar
4. Click "Clear site data"

### 5. Verify in Browser
1. Open http://localhost:8080
2. Click Settings (gear icon)
3. Click "Developer" in the sidebar
4. **Scroll down** - you should see:
   - A box with "Model Management" heading
   - "Manage your AI models: download, remove, and switch between them."
   - Two tabs: "Installed" and "Available"
   - Model cards with buttons

### 6. Check Browser Console
1. Open DevTools (F12)
2. Go to Console tab
3. Look for:
   - "Loaded installed models: X" (confirms it's working)
   - Any red errors (these need to be fixed)

### 7. If Still Not Showing

**Test 1: Check if component renders at all**
In browser console, type:
```javascript
document.querySelector('.model-management-container') || 
document.querySelector('[class*="Model Management"]')
```
If this returns `null`, the component isn't rendering.

**Test 2: Check API**
```bash
curl http://localhost:8081/api/models/list
```
Should return JSON array.

**Test 3: Check Network Requests**
1. Open DevTools → Network tab
2. Navigate to Settings → Developer
3. Look for requests to `localhost:8081`
4. Check if they succeed (200) or fail

**Test 4: Verify Section Title**
In browser console:
```javascript
// Find the current section
Array.from(document.querySelectorAll('*')).find(el => 
  el.textContent?.includes('Enable model selector')
)?.closest('[class*="section"]')?.textContent
```

## Expected Result

When you navigate to Settings → Developer, you should see:

1. **At the top:**
   - ☑ Enable model selector
   - ☑ Show raw LLM output
   - Custom JSON (textarea)

2. **Below a divider line:**
   - **Model Management** (heading)
   - "Manage your installed models..." (description)
   - A box with "Model Management" title
   - Two tabs: "Installed (X)" and "Available (Y)"
   - Model cards with Download/Remove/Use buttons

## If Nothing Works

1. Try a different browser (Chrome, Firefox, Safari)
2. Try incognito/private mode
3. Check if there are JavaScript errors in console
4. Verify the web UI path: `curl http://localhost:8080` should return HTML

The component is definitely in the build - the issue is likely browser cache or a runtime JavaScript error.

