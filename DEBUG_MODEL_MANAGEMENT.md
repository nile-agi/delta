# Debug Steps for Model Management Not Showing

## Step 1: Verify Component is in Build
```bash
cd /Users/suzanodero/delta
grep -c "Model Management Component Loaded" public/index.html
# Should output: 1
```

## Step 2: Check Browser Console
1. Open http://localhost:8080
2. Press F12 to open DevTools
3. Go to Console tab
4. Look for:
   - "Model Management Component Loaded" (confirms component rendered)
   - "Loaded installed models: X" (confirms API call succeeded)
   - Any red errors

## Step 3: Check Network Tab
1. In DevTools, go to Network tab
2. Navigate to Settings → Developer
3. Look for requests to `localhost:8081/api/models/list`
4. Check if they return 200 (green) or fail (red)

## Step 4: Verify API Server is Running
```bash
curl http://localhost:8081/api/models/list
# Should return JSON array of models
```

## Step 5: Check if Component is Hidden
1. In DevTools, go to Elements/Inspector tab
2. Search for "Model Management"
3. Check if the element exists but is hidden (display: none, visibility: hidden, etc.)

## Step 6: Verify Section Title Match
The component only shows when `currentSection.title === 'Developer'`. Check:
1. In DevTools Console, type:
   ```javascript
   // This will show the current section
   document.querySelector('[data-section-title]')?.textContent
   ```
2. Or check the Settings sidebar - the section should be titled exactly "Developer"

## Step 7: Force Component to Show (Test)
Temporarily modify ChatSettingsDialog.svelte to always show the component:
```svelte
<!-- Add this at the top of the content area for testing -->
<div class="border-2 border-red-500 p-4">
  <h2>TEST: Model Management</h2>
  <ModelManagementTab />
</div>
```

If this shows, the component works but the conditional rendering is the issue.

## Step 8: Check for JavaScript Errors
Look in console for:
- `Uncaught TypeError`
- `Cannot read property`
- `Failed to fetch`
- CORS errors

## Step 9: Verify File Paths
```bash
# Component should exist
test -f assets/src/lib/components/app/chat/ModelManagement/ModelManagementTab.svelte && echo "OK"

# Import should be correct
grep "ModelManagementTab" assets/src/lib/components/app/chat/ChatSettings/ChatSettingsDialog.svelte
```

## Step 10: Hard Refresh
- Mac: Cmd + Shift + R
- Windows/Linux: Ctrl + Shift + R
- Or: DevTools → Application → Clear Storage → Clear site data

## Common Issues

### Issue: Component renders but shows "Failed to load models"
**Solution:** API server not running. Restart delta.

### Issue: Component doesn't render at all
**Solution:** 
1. Check browser console for import errors
2. Verify the component file exists
3. Rebuild web UI: `cd assets && npm run build`

### Issue: Section title doesn't match
**Solution:** Check the exact title in the settingsSections array - it must match exactly "Developer"

