# Performance Fix Summary: Batch Size Optimization

## Problem
Delta was processing large PDFs (20,532 tokens) in **683.63 seconds**, while LlamaBarn processed the same PDF in **174.73 seconds** - a **4x performance difference**.

## Root Cause
Delta was not setting batch size parameters (`--ubatch-size` and `--batch-size`) when starting llama-server, relying on suboptimal defaults (512/2048) for large prompts.

## Solution Implemented
Added automatic batch size optimization based on context size:

- **Large contexts (≥8192)**: `--ubatch-size 2048 --batch-size 4096`
- **Medium contexts (≥4096)**: `--ubatch-size 1024 --batch-size 2048`
- **Small contexts (<4096)**: Use defaults (no change)

## Files Changed
1. `src/commands.cpp` - Added batch size logic to `build_llama_server_cmd()`
2. `src/delta_server_wrapper.cpp` - Added same batch size logic to `build_llama_server_command()`

## Expected Results
- **Before**: 683.63 seconds (~30 tokens/second)
- **After**: ~170-175 seconds (~117-120 tokens/second)
- **Improvement**: ~4x faster, matching LlamaBarn performance

## Next Steps

### 1. Build the Changes
```bash
cd /Users/suzanodero/io/GITHUB/delta
mkdir -p build_macos && cd build_macos
cmake ..
make -j$(sysctl -n hw.ncpu)
```

### 2. Test the Fix
1. Stop current Delta server: `pkill -f delta-server`
2. Start new build: `./delta --server --port 8080`
3. Upload the same PDF and measure processing time
4. Expected: ~170-175 seconds (vs previous 683 seconds)

### 3. Verify Batch Parameters
```bash
ps aux | grep llama-server | grep ubatch-size
# Should show: --ubatch-size 2048 --batch-size 4096 (for ctx_size >= 8192)
```

## Documentation Created
- `BATCH_SIZE_FIX.md` - Technical details of the fix
- `BUILD_AND_TEST.md` - Complete build and testing instructions
- `LLAMABARN_COMPARISON.md` - Comparison with LlamaBarn's approach
- `PDF_PERFORMANCE_ANALYSIS.md` - Original performance analysis

## Notes
- Changes are backward compatible (only affects large contexts)
- No user configuration needed
- Automatic optimization based on model context size
- Matches LlamaBarn's optimization strategy
