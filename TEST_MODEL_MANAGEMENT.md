# Testing Model Management

## After Rebuilding

1. **Stop delta** (Ctrl+C)

2. **Restart delta:**
   ```bash
   cd /Users/suzanodero/delta/build_macos
   ./delta
   ```

3. **Open browser:**
   - Go to http://localhost:8080
   - Open DevTools (F12)
   - Go to Console tab

4. **Navigate to Settings:**
   - Click Settings icon (gear)
   - Click "Developer" in sidebar
   - **Scroll down**

5. **What to look for:**
   - In the page: You should see "✓ Model Management Component Loaded" in green text
   - In console: You should see "ModelManagementTab mounted"
   - In console: You should see "Loaded installed models: X"

6. **If you see the green text but no tabs:**
   - Check console for API errors
   - Check Network tab for failed requests to localhost:8081

7. **If you don't see anything:**
   - Check console for JavaScript errors
   - Verify you're on the "Developer" section (not another section)
   - Try scrolling down more

## Expected Visual Result

When you're in Settings → Developer, you should see (in order):

1. **Checkboxes:**
   - ☑ Enable model selector
   - ☑ Show raw LLM output
   - Custom JSON (textarea)

2. **A horizontal divider line**

3. **"Model Management" heading** (h4)

4. **Description text:** "Manage your installed models..."

5. **Green text:** "✓ Model Management Component Loaded"

6. **Two tabs:** "Installed (X)" and "Available (Y)"

7. **Model cards** with buttons

## Quick Test in Browser Console

Open DevTools Console and run:
```javascript
// Check if component element exists
document.querySelector('[data-model-management="true"]')

// Should return an element, not null
```

If it returns `null`, the component isn't rendering.

