# Installation Verification Guide

This document ensures that all new model switching features are properly included when Delta is permanently installed on macOS, Linux, and Windows.

## ✅ What's Included in Installation

### 1. C++ Backend Changes
- ✅ `src/model_api_server.cpp` - Model Management API Server (port 8081)
  - Included in both `delta` and `delta-server` executables
  - Compiled automatically during build
  - Provides `/api/models/list`, `/api/models/use`, etc.

- ✅ `src/commands.cpp` - Updated to call `start_model_api_server(8081)`
- ✅ `src/delta_server_wrapper.cpp` - Updated to start/stop model API server

### 2. Web UI Frontend Changes
All new TypeScript/Svelte files are automatically included when building from `assets/`:

- ✅ `assets/src/lib/services/models.ts` - ModelsService with all API methods
- ✅ `assets/src/lib/stores/models.svelte.ts` - Updated models store with display names
- ✅ `assets/src/lib/services/parameter-sync.ts` - Parameter sync service
- ✅ `assets/src/lib/services/chat.ts` - Chat service
- ✅ `assets/src/lib/services/slots.ts` - Slots service
- ✅ `assets/src/lib/services/index.ts` - Service exports

### 3. Installation Process

#### macOS (`install-macos.sh`)
1. ✅ Builds C++ backend (includes `model_api_server.cpp`)
2. ✅ Builds web UI from `assets/` using `npm run build`
3. ✅ Copies web UI from `public/` to `/usr/local/share/delta-cli/webui`
4. ✅ Installs `delta` binary to `/usr/local/bin/delta`

#### Linux (`install-linux.sh`)
1. ✅ Builds C++ backend (includes `model_api_server.cpp`)
2. ✅ Builds web UI from `assets/` using `npm run build`
3. ✅ Copies web UI from `public/` to `/usr/local/share/delta-cli/webui`
4. ✅ Installs `delta` binary to `/usr/local/bin/delta`

#### Windows (`install-windows.ps1`)
1. ✅ Builds C++ backend (includes `model_api_server.cpp`)
2. ✅ Builds web UI from `assets/` using `npm run build`
3. ✅ Copies web UI from `public/` to `C:\Program Files\Delta CLI\webui`
4. ✅ Installs `delta.exe` to `C:\Program Files\Delta CLI\delta.exe`

### 4. CMakeLists.txt Configuration
- ✅ `model_api_server.cpp` included in `DELTA_SOURCES` (line 104)
- ✅ `model_api_server.cpp` included in `delta-server` executable (line 156)
- ✅ Web UI installed from `public/` to `share/delta-cli/webui` (lines 322-357)
- ✅ Web UI build step runs during CMake configuration (lines 242-309)

## 🔍 Verification Steps

### After Installation on macOS/Linux

1. **Verify Binary Installation:**
   ```bash
   which delta
   delta --version
   ```

2. **Verify Web UI Installation:**
   ```bash
   ls -la /usr/local/share/delta-cli/webui/
   # Should show index.html, index.html.gz, and other web UI files
   ```

3. **Verify Model API Server:**
   ```bash
   # Start delta server
   delta server
   
   # In another terminal, test the API
   curl http://localhost:8081/api/models/list
   # Should return JSON with installed models
   ```

4. **Test Web UI:**
   - Open http://localhost:8080
   - Check model dropdown shows display names (not filenames)
   - Try switching models
   - Refresh page after switching - should work without errors

### After Installation on Windows

1. **Verify Binary Installation:**
   ```powershell
   Get-Command delta
   delta --version
   ```

2. **Verify Web UI Installation:**
   ```powershell
   Test-Path "C:\Program Files\Delta CLI\webui\index.html"
   # Should return True
   ```

3. **Verify Model API Server:**
   ```powershell
   # Start delta server
   delta server
   
   # In another PowerShell window, test the API
   Invoke-WebRequest -Uri http://localhost:8081/api/models/list
   # Should return JSON with installed models
   ```

4. **Test Web UI:**
   - Open http://localhost:8080
   - Check model dropdown shows display names (not filenames)
   - Try switching models
   - Refresh page after switching - should work without errors

## 🧪 Automated Verification Script

Run the test script after installation:

```bash
./test-model-switching.sh
```

This will verify:
- ✅ Both servers are running (8080 and 8081)
- ✅ Installed models API returns data with display names
- ✅ Model switch API works
- ✅ Server restart functionality works

## 📋 Checklist for Release

Before releasing a new version, verify:

- [ ] `model_api_server.cpp` is in CMakeLists.txt (lines 104, 156)
- [ ] All new TypeScript files are in `assets/src/lib/`
- [ ] Web UI builds successfully (`npm run build` in `assets/`)
- [ ] `public/` directory contains built web UI files
- [ ] Installation scripts build web UI from `assets/` if `public/` doesn't exist
- [ ] CMakeLists.txt installs web UI to `share/delta-cli/webui`
- [ ] Server wrapper finds web UI in installed location
- [ ] Model API server starts on port 8081
- [ ] Model dropdown shows display names (not filenames)
- [ ] Model switching works
- [ ] Page refresh after model switch works (no connection errors)

## 🐛 Troubleshooting

### Web UI Not Found After Installation

**Problem:** Web UI files not in installed location

**Solution:**
1. Check if `public/` directory exists in source
2. Rebuild web UI: `cd assets && npm run build`
3. Reinstall: `sudo make install` (or run install script again)

### Model API Server Not Starting

**Problem:** Port 8081 not accessible

**Solution:**
1. Check if `model_api_server.cpp` was compiled:
   ```bash
   nm delta | grep start_model_api_server
   ```
2. Check server logs for errors
3. Verify port 8081 is not in use: `lsof -i :8081`

### Model Dropdown Empty

**Problem:** Models not showing in dropdown

**Solution:**
1. Check browser console for errors (F12)
2. Verify Model API server is running: `curl http://localhost:8081/api/models/list`
3. Check if models are installed: `delta --list-models`
4. Verify web UI files include `assets/src/lib/services/models.ts`

### Display Names Not Showing

**Problem:** Still seeing filenames instead of display names

**Solution:**
1. Clear browser cache (Cmd+Shift+R or Ctrl+Shift+R)
2. Verify `assets/src/lib/stores/models.svelte.ts` has the updated `fetch()` method
3. Check that `/api/models/list` returns `display_name` field
4. Rebuild web UI: `cd assets && npm run build`

## 📝 Notes

- The web UI is built during installation if `public/` doesn't exist
- All TypeScript files in `assets/src/` are automatically included in the build
- The Model API server runs on port 8081 (separate from web UI on 8080)
- Display names come from the model registry in `src/models.cpp`
- Model switching automatically restarts the server on port 8080

