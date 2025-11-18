# Troubleshooting DMG Creation

## Common Issues and Solutions

### Xcode Command Line Tools Not Found

**Error:**
```
xcode-select: note: No developer tools were found, requesting install.
CMake Error: The C compiler is not able to compile a simple test program.
```

**Solution:**
1. Install Xcode Command Line Tools:
   ```bash
   xcode-select --install
   ```
2. Wait for the installation to complete (this may take 10-15 minutes)
3. If a dialog appears, click "Install" and wait
4. After installation, restart your terminal
5. Verify installation:
   ```bash
   xcode-select -p
   # Should output: /Library/Developer/CommandLineTools
   ```
6. Test the compiler:
   ```bash
   clang --version
   ```
7. Run the build script again:
   ```bash
   ./installers/create_dmg.sh
   ```

**Alternative:** If you have Xcode installed but it's not selected:
```bash
sudo xcode-select --switch /Applications/Xcode.app/Contents/Developer
```

### CMake Not Found

**Error:**
```
cmake: command not found
```

**Solution:**
```bash
brew install cmake
```

### npm/Node.js Not Found (for Web UI)

**Error:**
```
npm: command not found
```

**Solution:**
```bash
brew install node
```

### Build Fails with Compiler Errors

**Possible causes:**
1. **Incomplete Xcode Command Line Tools installation**
   - Reinstall: `sudo rm -rf /Library/Developer/CommandLineTools && xcode-select --install`

2. **Wrong architecture**
   - On Apple Silicon, make sure you're not forcing x86_64
   - On Intel Mac, make sure Rosetta 2 is installed if needed

3. **Missing dependencies**
   - Install via Homebrew: `brew install cmake git curl`

### DMG Creation Fails

**Error:**
```
hdiutil: create failed - No space left on device
```

**Solution:**
- Free up disk space (need at least 2-3 GB free)
- Check available space: `df -h`

**Error:**
```
Build not found. Please run ./installers/build_macos.sh first.
```

**Solution:**
- Make sure the build completed successfully
- Check that `build_macos_release/delta` exists
- Re-run the build: `./installers/build_macos.sh`

### Permission Errors

**Error:**
```
Permission denied
```

**Solution:**
- Make scripts executable: `chmod +x installers/*.sh`
- For system-wide installation issues, the DMG doesn't require sudo

### Web UI Build Fails

**Error:**
```
npm install failed
```

**Solution:**
1. Clear npm cache: `npm cache clean --force`
2. Delete node_modules: `rm -rf assets/node_modules`
3. Try again: `cd assets && npm install`

**Error:**
```
vite build failed
```

**Solution:**
- Check Node.js version: `node --version` (should be 16+)
- Update Node.js: `brew upgrade node`
- Check for syntax errors in assets/src/

## Getting Help

If you're still stuck:

1. **Check the logs:**
   - Build logs are shown in the terminal
   - Look for the first error message

2. **Verify your environment:**
   ```bash
   # Check versions
   clang --version
   cmake --version
   node --version
   npm --version
   ```

3. **Clean and retry:**
   ```bash
   # Clean build directory
   rm -rf build_macos_release
   
   # Clean web UI
   cd assets && rm -rf node_modules .svelte-kit && cd ..
   
   # Try again
   ./installers/create_dmg.sh
   ```

4. **Report issues:**
   - Include the full error message
   - Include your macOS version: `sw_vers`
   - Include architecture: `uname -m`

