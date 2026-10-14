#!/usr/bin/env bash
# Tests package-cli.sh and collect-tauri-bundles.sh against a fake build and fake bundles.
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
WORK="$(mktemp -d "${TMPDIR:-/tmp}/delta-release-packaging.XXXXXX")"
trap 'rm -rf "$WORK"' EXIT
fail() { echo "FAIL: $*" >&2; exit 1; }

# A copy of the repository files the scripts read, so nothing in the real tree changes.
mkdir -p "$WORK/repo/scripts" "$WORK/repo/public"
cp "$REPO_ROOT/scripts/package-cli.sh" "$REPO_ROOT/scripts/collect-tauri-bundles.sh" "$WORK/repo/scripts/"
echo "9.8.7" > "$WORK/repo/version.txt"
echo '<html></html>' > "$WORK/repo/public/index.html"
git -C "$WORK/repo" init -q && git -C "$WORK/repo" -c user.email=t@t -c user.name=t commit -q --allow-empty -m t

build="$WORK/build"
mkdir -p "$build/bin"
for f in delta delta-server bin/llama-server; do echo "$f" > "$build/$f"; done
echo metal > "$build/default.metallib"

"$WORK/repo/scripts/package-cli.sh" "$build" aarch64-apple-darwin "$WORK/out/delta-cli-macos-arm64.tar.gz" v9.8.7 >/dev/null
listing="$(tar -tzf "$WORK/out/delta-cli-macos-arm64.tar.gz")"
for want in delta-cli-9.8.7/bin/delta delta-cli-9.8.7/bin/delta-server delta-cli-9.8.7/bin/llama-server \
  delta-cli-9.8.7/bin/default.metallib delta-cli-9.8.7/webui/index.html delta-cli-9.8.7/BUILD_INFO; do
  grep -qx "$want" <<<"$listing" || fail "archive is missing $want"
done
(cd "$WORK/out" && { sha256sum -c delta-cli-macos-arm64.tar.gz.sha256 2>/dev/null || shasum -a 256 -c delta-cli-macos-arm64.tar.gz.sha256; } >/dev/null) ||
  fail "checksum does not verify next to the archive"

"$WORK/repo/scripts/package-cli.sh" "$build" x86_64-unknown-linux-gnu "$WORK/out/delta-cli-linux-x86_64.tar.gz" >/dev/null
tar -tzf "$WORK/out/delta-cli-linux-x86_64.tar.gz" | grep -q metallib && fail "a Linux archive has the Metal library"

rm "$build/bin/llama-server"
if "$WORK/repo/scripts/package-cli.sh" "$build" x86_64-unknown-linux-gnu "$WORK/out/x.tar.gz" >/dev/null 2>&1; then
  fail "packaging without llama-server succeeded"
fi

bundle="$WORK/repo/src-tauri/target/aarch64-apple-darwin/release/bundle"
mkdir -p "$bundle/dmg" "$bundle/macos/Delta.app/Contents"
echo dmg > "$bundle/dmg/Delta_9.8.7_aarch64.dmg"
"$WORK/repo/scripts/collect-tauri-bundles.sh" aarch64-apple-darwin "$WORK/mac" >/dev/null
[[ -f "$WORK/mac/Delta_9.8.7_aarch64.dmg" && -f "$WORK/mac/Delta_aarch64.app.tar.gz" ]] ||
  fail "macOS bundles were not collected under their release names"

bundle="$WORK/repo/src-tauri/target/release/bundle"
mkdir -p "$bundle/deb" "$bundle/rpm"
echo deb > "$bundle/deb/Delta_9.8.7_amd64.deb"
echo rpm > "$bundle/rpm/Delta-9.8.7-1.x86_64.rpm"
if "$WORK/repo/scripts/collect-tauri-bundles.sh" x86_64-unknown-linux-gnu "$WORK/linux" >/dev/null 2>&1; then
  fail "a Linux build without its AppImage was collected"
fi

echo "Release packaging tests passed."
