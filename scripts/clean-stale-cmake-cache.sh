#!/usr/bin/env bash
# Wipe a build dir whose cached CMake C compiler isn't Apple's /usr/bin/cc (e.g. a
# Homebrew LLVM clang, which can't build the macOS SDK) so cmake reconfigures with it.
set -euo pipefail

DIR="${1:-}"
[[ -n "$DIR" && "$(uname -s)" == "Darwin" && -f "$DIR/CMakeCache.txt" ]] || exit 0

CC="$(sed -n 's/^CMAKE_C_COMPILER:[^=]*=//p' "$DIR/CMakeCache.txt" | head -1)"
if [[ -n "$CC" && "$CC" != "/usr/bin/cc" ]]; then
    echo "  Build dir cached a non-Apple compiler ($CC) -- wiping $DIR to rebuild with /usr/bin/cc."
    rm -rf "$DIR"
fi
