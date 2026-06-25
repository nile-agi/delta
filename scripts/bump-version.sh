#!/usr/bin/env bash
#
# Bump the version across all project files from a single VERSION file.
#
# Usage:
#   echo "1.2.0" > VERSION && ./scripts/bump-version.sh
#   ./scripts/bump-version.sh 1.2.0    # writes VERSION then updates everything

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
VERSION_FILE="$REPO_ROOT/VERSION"

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
    sed -i.bak -E "0,/^version = \"[0-9]+\.[0-9]+\.[0-9]+\"/s//version = \"$VERSION\"/" \
        "$REPO_ROOT/src-tauri/Cargo.toml"
    rm -f "$REPO_ROOT/src-tauri/Cargo.toml.bak"
    echo "  Updated src-tauri/Cargo.toml"
fi

# dist/macports/Portfile
if [[ -f "$REPO_ROOT/dist/macports/Portfile" ]]; then
    sed -i.bak -E "s/^version[[:space:]]+[0-9]+\.[0-9]+\.[0-9]+/version             $VERSION/" \
        "$REPO_ROOT/dist/macports/Portfile"
    rm -f "$REPO_ROOT/dist/macports/Portfile.bak"
    echo "  Updated dist/macports/Portfile"
fi

# dist/nix/default.nix
if [[ -f "$REPO_ROOT/dist/nix/default.nix" ]]; then
    sed -i.bak -E "s/version = \"[0-9]+\.[0-9]+\.[0-9]+\"/version = \"$VERSION\"/" \
        "$REPO_ROOT/dist/nix/default.nix"
    rm -f "$REPO_ROOT/dist/nix/default.nix.bak"
    echo "  Updated dist/nix/default.nix"
fi

# dist/winget/delta-cli.yaml
if [[ -f "$REPO_ROOT/dist/winget/delta-cli.yaml" ]]; then
    sed -i.bak -E "s/PackageVersion: [0-9]+\.[0-9]+\.[0-9]+/PackageVersion: $VERSION/" \
        "$REPO_ROOT/dist/winget/delta-cli.yaml"
    sed -i.bak -E "s|/v[0-9]+\.[0-9]+\.[0-9]+/|/v$VERSION/|" \
        "$REPO_ROOT/dist/winget/delta-cli.yaml"
    rm -f "$REPO_ROOT/dist/winget/delta-cli.yaml.bak"
    echo "  Updated dist/winget/delta-cli.yaml"
fi

echo ""
echo "Done. VERSION=$VERSION synced to all project files."
echo "CMakeLists.txt reads VERSION at configure time — no sed needed."
