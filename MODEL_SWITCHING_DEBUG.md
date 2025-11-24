# Model Switching Debug Guide

## Changes Made

1. **Fixed deadlock in `restart_llama_server()`**: The function was acquiring a lock and then calling `stop_llama_server()` which also tried to acquire the same lock. Fixed by consolidating the stop logic.

2. **Added comprehensive debug logging**: Added debug output to trace:
   - When `restart_llama_server()` is called
   - When the model switch callback is invoked
   - When llama-server is started/stopped
   - When the model API server starts
   - When the callback is set

3. **Ensured callback is always set**: The callback is now set every time `launch_server_auto()` is called, even if the model API server was already running.

## How to Test

1. Run `./delta` from the build directory
2. Check the console output for debug messages:
   - `[DEBUG] Starting model API server on port 8081`
   - `[DEBUG] Setting up model switch callback`
   - `[DEBUG] Model switch callback set successfully`

3. Open the web UI at http://localhost:8080
4. Select a different model from the dropdown
5. Watch the console for:
   - `[DEBUG] /api/models/use: model_name=...`
   - `[DEBUG] g_model_switch_callback is set`
   - `[DEBUG] Calling model switch callback...`
   - `[DEBUG] restart_llama_server called: model=...`
   - `[DEBUG] Stopping llama-server with PID: ...`
   - `[DEBUG] llama-server started with PID: ...`

## Expected Flow

1. User runs `./delta` → `launch_server_auto()` is called
2. `launch_server_auto()` starts llama-server and model API server
3. Model API server callback is set to `Commands::restart_llama_server()`
4. User selects model in web UI → Frontend calls `/api/models/use`
5. Model API server receives request and calls the callback
6. `restart_llama_server()` stops current llama-server and starts new one
7. API returns `{"loaded": true}` to frontend
8. Frontend updates UI to show selected model

## Troubleshooting

If model switching doesn't work, check:

1. **Is the model API server running?**
   - Look for `[DEBUG] Starting model API server on port 8081`
   - Check if port 8081 is listening: `lsof -i :8081`

2. **Is the callback set?**
   - Look for `[DEBUG] Model switch callback set successfully`
   - Check if `g_model_switch_callback is set` appears in `/api/models/use` logs

3. **Is the callback being called?**
   - Look for `[DEBUG] Calling model switch callback...`
   - Look for `[DEBUG] Model switch callback invoked: model=...`

4. **Is llama-server restarting?**
   - Look for `[DEBUG] Stopping llama-server with PID: ...`
   - Look for `[DEBUG] llama-server started with PID: ...`
   - Check if the process is actually running: `ps aux | grep llama-server`

5. **Is the model path correct?**
   - Check `[DEBUG] restart_llama_server called: model=..., path=...`
   - Verify the path exists: `ls -la <model_path>`

## Common Issues

1. **Callback not set**: If `g_model_switch_callback is null`, the callback wasn't set. Check if `launch_server_auto()` was called.

2. **Callback not called**: If the callback is set but not called, check if `/api/models/use` is receiving requests. Check browser network tab.

3. **llama-server not restarting**: If the callback is called but llama-server doesn't restart, check:
   - Is llama-server binary found? (Check `[DEBUG] llama-server not found` error)
   - Is the model path valid?
   - Are there permission issues?

4. **Port already in use**: If port 8080 is already in use, llama-server won't start. Kill the existing process first.

