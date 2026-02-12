# Batch Size Optimization Fix

## Problem Identified

Delta was processing the same PDF (20,532 tokens) in **683.63 seconds**, while LlamaBarn processed it in **174.73 seconds** - a **4x performance difference**.

## Root Cause

Delta was **not setting batch size parameters** when starting llama-server, relying on defaults:
- Default `--ubatch-size`: 512 tokens
- Default `--batch-size`: 2048 tokens

For large prompts (20k+ tokens), these defaults are suboptimal and cause slow prompt processing.

## Solution

Added batch size optimization to match LlamaBarn's performance:

### Changes Made

1. **`src/commands.cpp`** - Added batch size configuration based on context size
2. **`src/delta_server_wrapper.cpp`** - Added same batch size optimization

### Batch Size Logic

```cpp
if (ctx_size >= 8192) {
    // Large contexts: optimize for 20k+ token prompts
    cmd << " --ubatch-size 2048";  // 4x default (512 → 2048)
    cmd << " --batch-size 4096";   // 2x default (2048 → 4096)
} else if (ctx_size >= 4096) {
    // Medium contexts: moderate optimization
    cmd << " --ubatch-size 1024";  // 2x default
    cmd << " --batch-size 2048";   // Keep default
}
```

## Expected Performance Improvement

With `--ubatch-size 2048`:
- **Before**: 683.63 seconds (30 tokens/second)
- **After**: ~170-175 seconds (117-120 tokens/second)
- **Improvement**: ~4x faster, matching LlamaBarn performance

## Why This Works

The `--ubatch-size` parameter controls how many tokens are processed in each batch during prompt evaluation:
- **Smaller batches (512)**: More overhead, slower processing
- **Larger batches (2048)**: Less overhead, better GPU/CPU utilization, faster processing

For large prompts, larger batch sizes allow llama-server to:
1. Process more tokens per operation
2. Better utilize available compute resources
3. Reduce overhead from batch management
4. Improve memory access patterns

## Testing

Test with the same PDF that took 683 seconds:
1. Restart Delta server to apply new batch size settings
2. Process the same PDF
3. Expected: ~170-175 seconds (matching LlamaBarn)

## Notes

- Batch sizes are automatically configured based on context size
- No user configuration needed
- Backward compatible - only affects large contexts
- Matches LlamaBarn's optimization approach
