# Model Switch Test Results

## Test Date
November 23, 2024

## Test Objective
Determine if switching models in the Delta Web UI actually changes the model that is running and processing requests.

## Test Methodology

1. Checked the current server model via `/v1/models` endpoint
2. Attempted to switch models using `/api/models/use` endpoint
3. Verified if server model changed after switch
4. Sent test chat requests with different model names in the request body
5. Compared system fingerprints and model parameters to identify actual model

## Test Results

### Initial State
- **Server Model**: Qwen 3 0.6B
- **Model Parameters**:
  - Vocabulary size: 151,936
  - Embedding dimension: 1,024
  - Parameters: 751,632,384
  - Context size: 40,960

### After Model Switch Attempt
- **Selected Model** (via API): SmolLM 135M
- **Server Model** (actual): Qwen 3 0.6B (UNCHANGED)
- **Model Parameters**: Still Qwen 3 0.6B parameters

### Chat Request Tests

#### Request 1: With `"model": "smollm:135m"` in request body
```json
{
  "model": "smollm:135m",
  "system_fingerprint": "b6730-e60f01d9",
  ...
}
```

#### Request 2: With `"model": "qwen3:0.6b"` in request body
```json
{
  "model": "qwen3:0.6b",
  "system_fingerprint": "b6730-e60f01d9",
  ...
}
```

**Key Finding**: Both requests returned the **same system fingerprint** (`b6730-e60f01d9`), which identifies the actual model being used.

## Conclusion

### ❌ Model switching does NOT change the actual running model

**Evidence:**
1. Server model remains unchanged after API switch call
2. Model parameters (vocab size, embedding dim, etc.) remain the same
3. System fingerprint is identical regardless of model name in request
4. The `model` field in responses only echoes back what was sent in the request

### How llama-server Works

1. **Model Loading**: llama-server loads ONE model at startup (specified with `-m` flag)
2. **Model Field in Requests**: The `model` field in API requests is treated as **metadata only**
3. **No Dynamic Switching**: llama-server does not support loading/unloading models at runtime
4. **Response Model Field**: The `model` field in responses just reflects what was in the request, not what was actually used

### Implications for Delta Web UI

1. ✅ **UI Selection Works**: Users can select models from the dropdown
2. ✅ **Model Name Sent**: The selected model name is correctly sent in API requests
3. ✅ **Model Displayed**: The selected model is shown in the UI
4. ❌ **Model Not Actually Used**: The server continues using the model loaded at startup

### To Actually Switch Models

Users must **restart the delta-server** with the desired model:

```bash
./delta-server -m "/path/to/new/model.gguf" --port 8080
```

## Recommendations

1. **Add UI Warning**: Show a clear message that model switching requires server restart
2. **Model Mismatch Detection**: Detect when selected model differs from server model and warn user
3. **Future Enhancement**: Consider implementing a model proxy layer that can actually load/unload models dynamically

