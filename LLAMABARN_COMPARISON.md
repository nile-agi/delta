# LlamaBarn vs Delta: Tokenization & Prompt Processing Comparison

## Key Finding: Both Use llama-server

Both **LlamaBarn** and **Delta** use the same underlying server: **llama-server** from llama.cpp. This means they should have identical tokenization performance.

## Architecture Comparison

### LlamaBarn
- **Wrapper**: macOS menu bar app (Swift)
- **Server**: llama-server from llama.cpp
- **API**: OpenAI-compatible at `http://localhost:2276/v1`
- **Tokenization**: Handled by llama-server (C++)

### Delta
- **Wrapper**: Cross-platform CLI/server (C++)
- **Server**: llama-server from llama.cpp (same as LlamaBarn)
- **API**: OpenAI-compatible at `http://localhost:8080/v1`
- **Tokenization**: Handled by llama-server (C++)

## Tokenization Flow

### Both Applications Follow This Flow:

```
Client Request → llama-server → Tokenization → Prompt Evaluation → Generation
```

1. **Client sends request** with full PDF text content
2. **llama-server receives** the request
3. **Tokenization happens** (synchronously, before streaming starts)
4. **Prompt evaluation** begins (with `prompt_progress` updates)
5. **Generation** streams back to client

## Delta's Implementation

### ✅ Already Supports `prompt_progress`

**Location**: `assets/src/lib/services/chat.ts:333-342`

```typescript
const promptProgress = parsed.prompt_progress;

if (timings || promptProgress) {
    this.updateProcessingState(timings, promptProgress, conversationId);
    if (timings) {
        lastTimings = timings;
    }
}
```

Delta correctly:
- ✅ Receives `prompt_progress` from llama-server
- ✅ Updates UI state with progress
- ✅ Shows "preparing" status during prompt processing

## The Real Issue

### Tokenization Happens BEFORE `prompt_progress`

The 683-second delay is happening during **tokenization**, which occurs **before** llama-server can start streaming `prompt_progress` updates.

**Timeline**:
```
0s: Request sent with 20k tokens of PDF text
0-683s: Tokenization (BLOCKING, no progress updates)
683s: Tokenization complete, prompt_progress starts
683s+: Prompt evaluation (with progress updates)
```

### Why Tokenization is Slow

1. **Synchronous Blocking**: `llama_tokenize()` is synchronous
2. **Large Input**: 20,532 tokens is a very large prompt
3. **No Chunking**: Entire PDF text sent as single string
4. **No Optimization**: No pre-processing or size limits

## What LlamaBarn Does Differently

**Answer: Nothing!** LlamaBarn doesn't implement any special tokenization optimizations. It relies entirely on llama-server, just like Delta.

However, LlamaBarn users might:
- Use smaller PDFs
- Process PDFs as images (vision models)
- Use chunking strategies in their client applications

## Recommendations Based on LlamaBarn's Approach

Since LlamaBarn doesn't solve this at the server level, the solution must be at the **client/application level**:

### 1. **Client-Side Size Limits** (Like LlamaBarn's Smart Catalog)

LlamaBarn shows "what fits your Mac" - Delta should:
- Warn users about large PDFs before sending
- Suggest alternatives (images, chunking)
- Set reasonable limits (e.g., 10k tokens max)

### 2. **Batch Size Configuration**

From llama.cpp docs, batch size affects large prompt handling:
- `-b, --batch-size N`: Logical maximum batch size (default: 2048)
- `-ub, --ubatch-size N`: Physical maximum batch size (default: 512)

Delta should ensure llama-server is started with appropriate batch sizes for large prompts.

### 3. **PDF Processing Strategy**

LlamaBarn doesn't process PDFs - clients do. Delta should:
- Process PDFs as images for vision models (already supported)
- Implement chunking for text PDFs
- Add size warnings before processing

## Code Changes Needed

### 1. Add Batch Size to llama-server Command

**File**: `src/commands.cpp:659-698`

Add batch size parameters:
```cpp
if (ctx_size > 16384) {
    cmd << " --gpu-layers 0";
    cmd << " -b 4096";  // Increase batch size for large contexts
    cmd << " -ub 1024"; // Increase physical batch size
}
```

### 2. Add PDF Size Validation

**File**: `assets/src/lib/utils/convert-files-to-extra.ts`

Add size check before processing:
```typescript
// Estimate token count (rough: 1 token ≈ 4 characters)
const estimatedTokens = Math.ceil(content.length / 4);
const MAX_PDF_TOKENS = 10000; // Reasonable limit

if (estimatedTokens > MAX_PDF_TOKENS) {
    toast.warning(
        `PDF "${file.name}" is very large (${estimatedTokens} tokens). ` +
        `Consider processing as images or splitting into chunks.`,
        { duration: 5000 }
    );
    // Optionally truncate or chunk
}
```

### 3. Implement Chunking Strategy

**File**: `assets/src/lib/services/chat.ts:535-554`

Split large PDFs into multiple messages:
```typescript
const CHUNK_SIZE = 8000; // tokens per chunk
const chunks = chunkTextByTokens(pdfFile.content, CHUNK_SIZE);

for (let i = 0; i < chunks.length; i++) {
    contentParts.push({
        type: 'text',
        text: `\n\n--- PDF File: ${pdfFile.name} (Part ${i+1}/${chunks.length}) ---\n${chunks[i]}`
    });
}
```

## Expected Performance Improvement

With these changes:
- **Small PDFs** (<10k tokens): No change, already fast
- **Large PDFs** (>10k tokens): 
  - Warning shown before processing
  - Chunked into smaller requests
  - Each chunk processes in <10 seconds
  - Total time: ~30-60 seconds (vs 683 seconds)

## Conclusion

LlamaBarn doesn't solve this problem at the server level - it's a client-side issue. Delta needs to:
1. Add size limits and warnings
2. Implement chunking for large PDFs
3. Optimize llama-server batch sizes
4. Better user feedback during processing

The tokenization bottleneck is inherent to llama.cpp's synchronous tokenization. The solution is to avoid sending 20k+ token prompts in a single request.
