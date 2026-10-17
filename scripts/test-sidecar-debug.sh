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
