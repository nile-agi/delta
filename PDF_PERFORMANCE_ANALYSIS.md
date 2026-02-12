# PDF Processing Performance Analysis

## Issue Summary
Processing a PDF file takes **683.63 seconds** with **20,532 prompt tokens**, which is extremely slow for prompt processing.

## Root Cause Analysis

### 1. **Synchronous Tokenization Bottleneck**
The entire PDF text content is tokenized synchronously before the request can proceed:

**Location**: `src/inference.cpp:135-182`
- The `tokenize()` function calls `llama_tokenize()` synchronously
- For 20,532 tokens, this blocks the entire request thread
- No progress reporting during tokenization phase

### 2. **Full PDF Content in Prompt**
The entire extracted PDF text is included in the message content:

**Location**: `assets/src/lib/services/chat.ts:549-552`
```typescript
contentParts.push({
    type: 'text',
    text: `\n\n--- PDF File: ${pdfFile.name} ---\n${pdfFile.content}`
});
```

### 3. **No Size Limits or Chunking**
There's no limit on PDF text size before sending to the server:
- Large PDFs can generate tens of thousands of tokens
- All content is sent in a single request
- No truncation or chunking mechanism

## Code Flow

1. **Frontend PDF Processing** (`assets/src/lib/utils/pdf-processing.ts:53-82`)
   - PDF.js extracts ALL text from all pages
   - Text is joined with `\n` separator
   - Entire content stored in database

2. **Message Construction** (`assets/src/lib/services/chat.ts:535-554`)
   - PDF content retrieved from database
   - Entire content added to prompt as text
   - Sent to `/v1/chat/completions` endpoint

3. **Server Tokenization** (`src/inference.cpp:135-182`)
   - Entire prompt tokenized synchronously
   - Blocks until complete (683 seconds for 20k tokens)
   - No progress updates during tokenization

## Performance Impact

- **Tokenization Time**: ~683 seconds for 20,532 tokens
- **Rate**: ~30 tokens/second (extremely slow)
- **User Experience**: No feedback during processing
- **Server**: Request thread blocked during tokenization

## Recommendations

### Immediate Fixes (High Priority)

1. **Add PDF Size Limits**
   - Limit PDF text extraction to reasonable size (e.g., 10,000 tokens)
   - Warn users when PDFs exceed limit
   - Option to truncate or process in chunks

2. **Implement Chunking Strategy**
   - Split large PDFs into multiple messages
   - Process chunks separately
   - Or use sliding window approach

3. **Add Progress Reporting**
   - Report tokenization progress to client
   - Show "Processing PDF..." indicator
   - Display estimated time remaining

### Long-term Optimizations

1. **Async Tokenization**
   - Move tokenization to background thread
   - Stream progress updates
   - Allow request to proceed incrementally

2. **Smart PDF Processing**
   - Extract only relevant pages/sections
   - Use OCR only when needed
   - Cache tokenized content

3. **Alternative Approaches**
   - Process PDFs as images for vision models (already supported)
   - Use embeddings for large documents
   - Implement document indexing/search

## Files to Modify

1. **`assets/src/lib/utils/pdf-processing.ts`**
   - Add size limit checks
   - Implement chunking logic

2. **`assets/src/lib/services/chat.ts`**
   - Add PDF size validation
   - Implement chunking for large PDFs

3. **`src/inference.cpp`** (if modifying server-side)
   - Add async tokenization support
   - Implement progress callbacks

4. **`assets/src/lib/utils/convert-files-to-extra.ts`**
   - Add warnings for large PDFs
   - Suggest alternatives (images, chunking)

## Expected Improvements

- **With size limits**: Reduce processing time to <10 seconds for most PDFs
- **With chunking**: Process large PDFs incrementally
- **With progress reporting**: Better user experience during processing
- **With async tokenization**: Non-blocking request handling

## Testing Recommendations

1. Test with PDFs of various sizes (small, medium, large)
2. Measure tokenization time vs. token count
3. Verify progress reporting works correctly
4. Test chunking with very large PDFs (>50k tokens)
5. Verify user warnings appear for oversized PDFs
