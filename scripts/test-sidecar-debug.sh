#!/usr/bin/env bash
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
work="$(mktemp -d "${TMPDIR:-/tmp}/delta-debug-policy.XXXXXX")"
trap 'rm -rf "$work"' EXIT
mkdir -p "$work/scripts" "$work/bin" "$work/build_tauri_debug/bin"
cp "$root/scripts/build-sidecars.sh" "$root/scripts/clean-stale-cmake-cache.sh" "$root/scripts/verify-release-binaries.py" "$work/scripts/"
printf '#!/bin/sh\nexit 0\n' > "$work/scripts/check-toolchain.sh"
printf '#!/bin/sh\nexit 0\n' > "$work/bin/cmake"
chmod +x "$work/bin/cmake"
printf fake > "$work/build_tauri_debug/delta-server"
printf fake > "$work/build_tauri_debug/bin/llama-server"
PATH="$work/bin:$PATH" bash "$work/scripts/build-sidecars.sh" --debug --target x86_64-apple-darwin
# Release checks must not run on debug binaries or require absent release metadata.
test ! -e "$work/src-tauri/binaries/release-build-info.json"

# Existing sidecars must be refreshed before launching the desktop app too.
cp "$root/Makefile" "$work/Makefile"
mkdir -p "$work/engine/vendor/llama.cpp"
touch "$work/engine/vendor/llama.cpp/CMakeLists.txt"
printf '#!/bin/sh\necho "host: x86_64-apple-darwin"\n' > "$work/bin/rustc"
printf '#!/bin/sh\necho cargo >> "$DEV_ORDER"\n' > "$work/bin/cargo"
printf '#!/bin/sh\necho sidecars >> "$DEV_ORDER"\nexit "${DEV_BUILD_EXIT:-0}"\n' > "$work/scripts/build-sidecars.sh"
chmod +x "$work/bin/rustc" "$work/bin/cargo" "$work/scripts/build-sidecars.sh"
export DEV_ORDER="$work/dev-order"
for installed in yes no; do
    if [[ "$installed" == no ]]; then rm -f "$work/src-tauri/binaries/"*; fi
    : > "$DEV_ORDER"
    (cd "$work" && PATH="$work/bin:$PATH" make dev)
    if [[ "$(cat "$DEV_ORDER")" != $'sidecars\ncargo' ]]; then
        echo "Desktop launched without refreshing $installed sidecars" >&2
        exit 1
    fi
done
: > "$DEV_ORDER"
if (cd "$work" && PATH="$work/bin:$PATH" DEV_BUILD_EXIT=7 make dev); then
    echo 'Desktop startup must stop when the sidecar build fails' >&2
    exit 1
fi
[[ "$(cat "$DEV_ORDER")" == sidecars ]]
echo 'Desktop development refreshes sidecars and refuses failed builds'

printf '#!/bin/sh\necho Darwin\n' > "$work/bin/uname"
chmod +x "$work/bin/uname"
for compiler in /usr/bin/cc /usr/bin/clang; do
    mkdir -p "$work/compiler-cache"
    printf 'CMAKE_C_COMPILER:FILEPATH=%s\n' "$compiler" > "$work/compiler-cache/CMakeCache.txt"
    PATH="$work/bin:$PATH" bash "$work/scripts/clean-stale-cmake-cache.sh" "$work/compiler-cache"
    if [[ ! -f "$work/compiler-cache/CMakeCache.txt" ]]; then
        echo "Apple compiler $compiler must not cause the cached build to be deleted" >&2
        exit 1
    fi
done
