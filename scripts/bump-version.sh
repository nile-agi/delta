#!/usr/bin/env bash
#
# Bump the version across all project files from a single VERSION file.
#
# Usage:
#   echo "1.2.0" > VERSION && ./scripts/bump-version.sh
#   ./scripts/bump-version.sh 1.2.0    # writes VERSION then updates everything

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
VERSION_FILE="$REPO_ROOT/version.txt"

if [[ $# -ge 1 ]]; then
    echo "$1" > "$VERSION_FILE"
fi

VERSION=$(tr -d '[:space:]' < "$VERSION_FILE")

if [[ -z "$VERSION" ]]; then
    echo "Error: VERSION file is empty" >&2
    exit 1
fi

echo "Bumping to version $VERSION"

# src-tauri/tauri.conf.json
if [[ -f "$REPO_ROOT/src-tauri/tauri.conf.json" ]]; then
    sed -i.bak -E "s/\"version\": \"[0-9]+\.[0-9]+\.[0-9]+\"/\"version\": \"$VERSION\"/" \
        "$REPO_ROOT/src-tauri/tauri.conf.json"
    rm -f "$REPO_ROOT/src-tauri/tauri.conf.json.bak"
    echo "  Updated src-tauri/tauri.conf.json"
fi

# src-tauri/Cargo.toml
if [[ -f "$REPO_ROOT/src-tauri/Cargo.toml" ]]; then
    awk -v version="$VERSION" '
        /^\[/ { in_package = ($0 ~ /^\[package\][[:space:]]*$/) }
        in_package && /^version[[:space:]]*=/ {
            ending = ($0 ~ /\r$/) ? "\r" : ""
            $0 = "version = \"" version "\"" ending
        }
        { print }
    ' "$REPO_ROOT/src-tauri/Cargo.toml" > "$REPO_ROOT/src-tauri/Cargo.toml.tmp"
    mv "$REPO_ROOT/src-tauri/Cargo.toml.tmp" "$REPO_ROOT/src-tauri/Cargo.toml"
    echo "  Updated src-tauri/Cargo.toml"
fi

# The package manifests in dist/ describe the latest *published* release and carry its
# checksums, which only exist once it is out: scripts/update-package-manifests.cjs updates them.

echo ""
echo "Done. VERSION=$VERSION synced to all project files."
echo "CMakeLists.txt reads VERSION at configure time — no sed needed."
