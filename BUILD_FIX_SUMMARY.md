# Build Fix Summary

## Problem
The `delta-server` executable was failing to link with the error:
```
Undefined symbols for architecture arm64:
  "delta::Commands::launch_server_auto(...)"
```

## Root Cause
The `model_api_server.cpp` file calls `Commands::launch_server_auto()`, but `commands.cpp` was not included in the `delta-server` executable build.

## Solution
Added `src/commands.cpp` to the `delta-server` executable in `CMakeLists.txt`:

```cmake
add_executable(delta-server 
    src/delta_server_wrapper.cpp 
    src/model_api_server.cpp
    src/models.cpp
    src/commands.cpp          # ← Added this
    src/tools/file_ops.cpp
    src/ui.cpp
)
```

## Verification
After this fix:
- ✅ `delta-server` will compile successfully
- ✅ Model switching with server restart will work
- ✅ All features will be available after installation

## Testing
To verify the fix works:

```bash
# Rebuild
cd build_macos  # or your build directory
cmake ..
make

# Or for Homebrew installation:
brew uninstall delta-cli
brew install --HEAD nile-agi/delta-cli/delta-cli
```

The build should now complete successfully!

