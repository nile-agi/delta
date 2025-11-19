# Testing Model Management Features

This guide will help you test the new model management features in the Delta web UI.

## Prerequisites

- CMake 3.14 or higher
- C++17 compatible compiler
- Node.js and npm (for building the web UI)
- A model already installed (for testing list/remove/use features)

## Step 1: Build the Project

### Option A: Quick Build (Recommended)

```bash
# From the project root directory
cd /Users/suzanodero/io/GITHUB/delta

# Build the C++ components (delta-server with model API)
mkdir -p build_macos
cd build_macos
cmake .. -DCMAKE_BUILD_TYPE=Release
make delta-server
cd ..
```

### Option B: Full Build

```bash
# Build everything including the main delta CLI
cd build_macos
make
cd ..
```

## Step 2: Build the Web UI

```bash
# Navigate to the assets directory
cd assets

# Install dependencies (if not already installed)
npm install

# Build the SvelteKit app
npm run build

# This should output files to ../public directory
cd ..
```

## Step 3: Start the Delta Server

You need to start delta-server with a model. The model API server will automatically start on port 8081.

```bash
# Make sure you have a model installed first
# If not, download one:
./build_macos/delta pull qwen3:0.6b

# Start the server (replace with your model path)
./build_macos/delta-server -m ~/.delta-cli/models/Qwen3-0.6B-f16.gguf --port 8080

# Or if delta is installed system-wide:
delta-server -m ~/.delta-cli/models/Qwen3-0.6B-f16.gguf --port 8080
```

**Expected Output:**
```
🚀 Starting Delta CLI Server...
📡 Server: http://localhost:8080
🤖 Model: /path/to/model.gguf
⚡ Parallel: 4
🧠 Context: 16384
🌐 Web UI: http://localhost:8080
📡 API: http://localhost:8080/v1/chat/completions
🔧 Model Management API: http://localhost:8081
Model Management API server running on http://127.0.0.1:8081
```

## Step 4: Access the Web UI

1. Open your browser and navigate to: **http://localhost:8080**
2. You should see the Delta chat interface

## Step 5: Access Model Management

1. Click on the **Settings** button (gear icon) in the chat interface
2. In the settings dialog, look for the **"Model Management"** section in the sidebar
3. Click on **"Model Management"** to open the model management tab

## Step 6: Test Each Feature

### Test 1: List Installed Models

1. Make sure you're on the **"Installed"** tab (should be default)
2. You should see a list of all installed models
3. Each model card should show:
   - Model name and display name
   - Description
   - Size and quantization info
   - "Use Model" and "Delete" buttons

**Expected:** You should see at least one model (the one you started the server with)

### Test 2: List Available Models

1. Click on the **"Available"** tab
2. You should see a list of all available models from the registry
3. Each model should show:
   - Installation status (Installed/Not Installed badge)
   - Download button for non-installed models
   - Use Model button for installed models

**Expected:** You should see many models listed, with some marked as "Installed" and others as "Not Installed"

### Test 3: Download a Model

1. Go to the **"Available"** tab
2. Find a model that is **not installed** (shows "Not Installed" badge)
3. Click the **"Download"** button
4. The button should change to show "Downloading..." with a spinner
5. Wait for the download to complete (this may take several minutes depending on model size)

**Expected:**
- Button shows loading state during download
- After completion, the model should appear in the "Installed" tab
- The model's status in "Available" tab should change to "Installed"

**Note:** Downloads can take a while. Start with a small model like `qwen3:0.6b` (~1.5GB) for testing.

### Test 4: Remove a Model

1. Go to the **"Installed"** tab
2. Find a model you want to remove (don't remove the one currently in use!)
3. Click the **"Delete"** button (trash icon)
4. A confirmation dialog should appear
5. Click **"Delete"** in the dialog to confirm

**Expected:**
- Confirmation dialog appears
- After confirmation, the model is removed
- The model disappears from the "Installed" list
- The model's status in "Available" tab changes to "Not Installed"

**Warning:** Don't delete the model that's currently being used by the server!

### Test 5: Switch/Use a Model

1. Go to either **"Installed"** or **"Available"** tab
2. Find an installed model
3. Click the **"Use Model"** button
4. A message should appear indicating the model path

**Expected:**
- Alert/message shows model path
- Note that server restart is required to actually use the new model

**Note:** This feature returns the model path. To actually switch models, you need to restart the server with the new model.

### Test 6: Refresh Models

1. Click the **"Refresh"** button (top right, with refresh icon)
2. The lists should reload

**Expected:**
- Loading spinner appears briefly
- Lists refresh with current data

## Troubleshooting

### Model API Server Not Starting

**Check if port 8081 is available:**
```bash
lsof -i :8081
```

**Check server logs:**
Look for the message "Model Management API server running on http://127.0.0.1:8081" in the terminal where you started delta-server.

### CORS Errors in Browser Console

If you see CORS errors, make sure:
1. The model API server is running on port 8081
2. You're accessing the web UI from localhost (not 127.0.0.1 or another domain)

### Models Not Loading

1. Check browser console (F12) for errors
2. Verify the model API server is running: `curl http://localhost:8081/api/models/list`
3. Check that you have at least one model installed

### Build Errors

If you get compilation errors:

1. **Missing httplib or json:**
   ```bash
   # Make sure llama.cpp is properly built
   cd vendor/llama.cpp
   mkdir -p build
   cd build
   cmake .. -DLLAMA_BUILD_SERVER=ON
   make
   cd ../../..
   ```

2. **Link errors:**
   - Make sure `common` and `cpp-httplib` targets exist
   - Check that llama-server built successfully first

## Quick Test Commands

```bash
# Test the API endpoints directly with curl:

# List available models
curl http://localhost:8081/api/models/available

# List installed models
curl http://localhost:8081/api/models/list

# Download a model (replace MODEL_NAME)
curl -X POST http://localhost:8081/api/models/download \
  -H "Content-Type: application/json" \
  -d '{"model":"qwen3:0.6b"}'

# Remove a model (replace MODEL_NAME)
curl -X DELETE http://localhost:8081/api/models/qwen3:0.6b

# Switch to a model (replace MODEL_NAME)
curl -X POST http://localhost:8081/api/models/use \
  -H "Content-Type: application/json" \
  -d '{"model":"qwen3:0.6b"}'
```

## Expected Behavior Summary

✅ **Working Features:**
- View installed models
- View available models
- Download models (with progress indication)
- Remove models (with confirmation)
- Switch models (returns path, requires server restart)
- Refresh model lists
- Error handling and loading states

⚠️ **Known Limitations:**
- Model switching requires manual server restart
- Download progress is not real-time (shows loading state)
- Large model downloads may take significant time

## Next Steps

After testing, you can:
1. Integrate real-time download progress (using WebSockets or polling)
2. Add automatic server restart on model switch
3. Add model search/filter functionality
4. Add model details/info view

