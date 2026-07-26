#!/usr/bin/env bash
#
# Preflight compiler check. On macOS the system SDK headers + llama.cpp use the
# 'visionOS' availability platform, which needs Xcode 16+ / clang 17+. Fail early
# with a clear message instead of a wall of SDK header errors on a stale toolchain.
set -euo pipefail

[[ "$(uname -s)" == "Darwin" ]] || exit 0

# Test the exact compiler the build pins (/usr/bin/cc), falling back if absent.
CC_BIN="/usr/bin/cc"
[[ -x "$CC_BIN" ]] || CC_BIN="$(xcrun -f clang 2>/dev/null || command -v clang || true)"
[[ -n "$CC_BIN" ]] || exit 0

if printf 'int main(void){ if(__builtin_available(visionOS 1.0, *)){} return 0; }\n' \
        | "$CC_BIN" -x c -fsyntax-only - >/dev/null 2>&1; then
    exit 0
fi

echo ""
echo "ERROR: your C/C++ toolchain is too old for this macOS SDK."
echo "  $("$CC_BIN" --version | head -1)"
echo "  It doesn't understand the 'visionOS' platform used by the system headers,"
echo "  so llama.cpp cannot compile. Update to Xcode 16+ / clang 17+:"
echo "    sudo xcode-select -s /Applications/Xcode.app/Contents/Developer && sudo xcodebuild -license accept"
echo "  or refresh the Command Line Tools:"
echo "    sudo rm -rf /Library/Developer/CommandLineTools && sudo xcode-select --install"
echo "  Verify 'clang --version' is 17+, then: rm -rf build build_tauri_release && make sidecars"
echo ""
exit 1
