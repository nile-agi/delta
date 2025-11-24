# Delta vs Delta-Server Model Switching

## When you run `./delta --server`:

1. **Delta binary** (`./delta`) is the main CLI
2. When you use `--server` flag, it:
   - Finds the `delta-server` binary
   - Spawns it using `system()` call
   - Passes model path and other parameters

3. **Delta-server binary** (`./delta-server`) is what actually:
   - Runs llama-server
   - Starts the model API server (port 8081)
   - Has the model switching callback mechanism

## Model Switching Flow:

✅ **YES, it will work!** When you run `./delta --server`:

1. `delta` spawns `delta-server` 
2. `delta-server` starts with model switching enabled
3. When user selects model in UI → `/api/models/use` is called
4. Model API server → Calls callback → `restart_llama_server()`
5. llama-server is restarted with new model
6. API returns `"loaded": true`

## Both ways work:

- `./delta --server -m <model>` → Spawns delta-server → Model switching works ✅
- `./delta-server -m <model>` → Direct execution → Model switching works ✅

The model switching functionality is in `delta-server`, so it works regardless of how it's started!
