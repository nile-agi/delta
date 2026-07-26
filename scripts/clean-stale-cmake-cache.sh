#!/usr/bin/env bash
# Wipe a build dir whose cached CMake compiler can't build the current macOS SDK
# (e.g. an old clang cached before an Xcode/CLT update). No-op otherwise.
set -euo pipefail

DIR="${1:-}"
[[ -n "$DIR" && "$(uname -s)" == "Darwin" && -f "$DIR/CMakeCache.txt" ]] || exit 0

CC="$(sed -n 's/^CMAKE_C_COMPILER:[^=]*=//p' "$DIR/CMakeCache.txt" | head -1)"
[[ -n "$CC" ]] || exit 0

if ! { [[ -x "$CC" ]] \
     && printf 'int main(void){ if(__builtin_available(visionOS 1.0, *)){} return 0; }\n' \
        | "$CC" -x c -fsyntax-only - >/dev/null 2>&1; }; then
    echo "  Stale CMake cache in $DIR (compiler: $CC) -- wiping to reconfigure with your current toolchain."
    rm -rf "$DIR"
fi
