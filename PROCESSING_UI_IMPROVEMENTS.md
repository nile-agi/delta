# Processing UI Improvements

## Summary

Implemented real-time processing status display with percentage, ETA, and prompt statistics matching the reference images.

## Changes Made

### 1. Enhanced Processing Message (`use-processing-state.svelte.ts`)

**Before:**
- "Processing..." (static text)
- "Processing (X%)" (no ETA)

**After:**
- "Processing X% (ETA: Ys)" - Shows real-time percentage and estimated time remaining
- Calculates ETA based on progress and elapsed time

### 2. Added Prompt Processing Statistics (`use-processing-state.svelte.ts`)

**New Features:**
- **Prompt Tokens**: Shows number of tokens being processed
- **Elapsed Time**: Shows time spent processing prompt (in seconds)
- **Processing Speed**: Shows tokens per second during prompt processing

**Display Logic:**
- During prompt processing (`preparing` status): Shows prompt-specific stats first
- During generation (`generating` status): Shows generation stats
- Stats are displayed in `ChatProcessingInfo` component below the processing message

### 3. Extended API Types (`api.d.ts`)

Added new fields to `ApiProcessingState`:
- `promptProgressTimeMs`: Elapsed time in milliseconds
- `promptProgressProcessed`: Number of tokens processed
- `promptProgressTotal`: Total tokens to process
- `promptTokensPerSecond`: Prompt processing speed

### 4. Enhanced Slots Service (`slots.ts`)

Updated `parseCompletionTimingData()` to:
- Extract prompt progress data from timing information
- Calculate prompt processing speed (tokens/second)
- Store prompt progress details for ETA calculation

## UI Display

### Processing Message
```
Processing 20% (ETA: 69s)
```

### Real-time Stats (during prompt processing)
```
2048 tokens | 7.83s | 261.52 tokens/s
```

### Real-time Stats (during generation)
```
Context: 2048/4096 (50%) | Output: 100/∞ | 238.31 tokens/sec
```

## ETA Calculation

The ETA is calculated using:
```
ETA = (elapsed_time / progress_percent) * (100 - progress_percent)
```

This provides a linear estimate based on current processing speed.

## Files Modified

1. `assets/src/lib/hooks/use-processing-state.svelte.ts`
   - Updated `getProcessingMessage()` to show percentage and ETA
   - Added `calculateETA()` function
   - Updated `getProcessingDetails()` to show prompt-specific stats

2. `assets/src/lib/services/slots.ts`
   - Enhanced `parseCompletionTimingData()` to extract prompt progress
   - Calculate prompt processing speed

3. `assets/src/lib/types/api.d.ts`
   - Extended `ApiProcessingState` interface with prompt progress fields

## Testing

1. **Build Status**: ✅ Build completed successfully
2. **Linting**: ✅ No linting errors
3. **Type Safety**: ✅ All types properly defined

## Next Steps

1. **Test the UI**:
   - Start Delta server
   - Upload a large PDF
   - Verify "Processing X% (ETA: Ys)" appears
   - Verify prompt stats (tokens, time, tokens/s) are displayed

2. **Verify Behavior**:
   - ETA updates in real-time as progress changes
   - Stats update smoothly during prompt processing
   - Stats switch to generation stats when generation starts

## Expected User Experience

When processing a large PDF:
1. User sees: "Processing 10% (ETA: 71s)"
2. Below that: "2048 tokens | 7.83s | 261.52 tokens/s"
3. As processing continues, percentage and ETA update in real-time
4. Stats update smoothly showing current processing speed

This matches the reference images provided and provides clear feedback during document processing.
