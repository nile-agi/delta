#!/bin/bash
# Script to check app bundle compatibility

APP_PATH="/Applications/Delta CLI.app"

if [ ! -d "$APP_PATH" ]; then
    echo "❌ App not found at: $APP_PATH"
    exit 1
fi

echo "Checking Delta CLI app bundle..."
echo "================================"
echo ""

# Check macOS version
echo "1. macOS Version:"
sw_vers
echo ""

# Check binary architecture
echo "2. Binary Architecture:"
file "$APP_PATH/Contents/MacOS/delta"
echo ""

# Check Info.plist
echo "3. Info.plist Contents:"
if [ -f "$APP_PATH/Contents/Info.plist" ]; then
    echo "LSMinimumSystemVersion:"
    plutil -extract LSMinimumSystemVersion raw "$APP_PATH/Contents/Info.plist" 2>/dev/null || echo "  Not found"
    echo ""
    echo "CFBundleVersion:"
    plutil -extract CFBundleVersion raw "$APP_PATH/Contents/Info.plist" 2>/dev/null || echo "  Not found"
    echo ""
    echo "CFBundleExecutable:"
    plutil -extract CFBundleExecutable raw "$APP_PATH/Contents/Info.plist" 2>/dev/null || echo "  Not found"
    echo ""
    
    # Validate plist
    echo "4. Info.plist Validation:"
    if plutil -lint "$APP_PATH/Contents/Info.plist" 2>&1; then
        echo "  ✓ Valid"
    else
        echo "  ❌ Invalid!"
    fi
else
    echo "  ❌ Info.plist not found!"
fi
echo ""

# Check xattr
echo "5. Extended Attributes:"
xattr -l "$APP_PATH" 2>/dev/null | head -5 || echo "  None"
echo ""

# Try to run the binary directly
echo "6. Testing Binary Execution:"
if "$APP_PATH/Contents/MacOS/delta" --version 2>&1 | head -3; then
    echo "  ✓ Binary runs successfully"
else
    echo "  ❌ Binary execution failed"
fi

