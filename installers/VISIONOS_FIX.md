# Fixing visionOS Compilation Errors

## Problem

When building on macOS with newer Xcode Command Line Tools, you may encounter errors like:

```
error: unrecognized platform name visionOS
error: use of undeclared identifier '__builtin_verbose_trap'
```

This happens because:
1. The SDK headers (Accelerate framework) reference `visionOS` which older compilers don't recognize
2. Some Metal code also references `visionOS` in `@available` checks

## Solution

The build script (`build_macos.sh`) automatically handles this by:

1. **Disabling BLAS/Accelerate**: Since Metal is the primary acceleration method on macOS, we disable BLAS/Accelerate to avoid the Accelerate framework header issues.

2. **Adding compiler flags**: The script adds flags to convert errors to warnings:
   ```bash
   -Wno-error  # Convert all errors to warnings (compatible with all clang versions)
   ```

3. **Metal still works**: Metal acceleration is enabled and will work perfectly - it's actually the preferred method on macOS.

## Manual Fix (if needed)

If you need to build manually and encounter these errors:

```bash
export CFLAGS="-Wno-error"
export CXXFLAGS="-Wno-error"
export OBJCFLAGS="-Wno-error"

cmake .. \
  -DGGML_METAL=ON \
  -DGGML_BLAS=OFF \
  -DGGML_ACCELERATE=OFF \
  -DCMAKE_C_FLAGS="${CFLAGS}" \
  -DCMAKE_CXX_FLAGS="${CXXFLAGS}" \
  -DCMAKE_OBJC_FLAGS="${OBJCFLAGS}"
```

## Why This Works

- **Metal is sufficient**: On macOS, Metal provides excellent GPU acceleration. BLAS/Accelerate are optional CPU-based optimizations that aren't necessary when Metal is available.

- **Compiler compatibility**: The flags tell the compiler to treat `visionOS` references as warnings instead of errors, allowing the build to complete.

- **No functionality loss**: The application will work exactly the same - Metal handles all the GPU acceleration.

## Alternative: Update Compiler

If you want to use BLAS/Accelerate, you would need to:
1. Install the full Xcode (not just Command Line Tools)
2. Use a newer compiler that recognizes visionOS
3. Or wait for Apple to update the Command Line Tools

However, this is not necessary since Metal provides better performance on macOS anyway.

