#!/usr/bin/env bash
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
APPIMAGE_ROOT="${TAURI_RELEASE_APPIMAGE_DIR:-$REPO_ROOT/src-tauri/target}"
TAURI_CLI="${TAURI_CLI_PATH:-$REPO_ROOT/web/app/node_modules/.bin/tauri}"

cd "$REPO_ROOT"
if [[ ! -x "$TAURI_CLI" ]]; then
  echo "ERROR: Tauri CLI is not executable: $TAURI_CLI" >&2
  exit 1
fi
"$TAURI_CLI" "$@"

images=()
image_manifest="$(mktemp "${TMPDIR:-/tmp}/delta-appimages.XXXXXX")"
trap 'rm -f "$image_manifest"' EXIT
if ! find "$APPIMAGE_ROOT" -type f -name '*.AppImage' -print0 >"$image_manifest"; then
  echo "ERROR: could not discover AppImages under $APPIMAGE_ROOT" >&2
  exit 1
fi
while IFS= read -r -d '' image; do
  images+=("$image")
done <"$image_manifest"
rm -f "$image_manifest"
trap - EXIT

if [[ ${#images[@]} -ne 1 ]]; then
  echo "ERROR: expected exactly one AppImage under $APPIMAGE_ROOT, found ${#images[@]}" >&2
  exit 1
fi

"$REPO_ROOT/scripts/repack-linux-appimage.sh" "${images[0]}"
"$REPO_ROOT/scripts/verify-tauri-release-assets.sh" --appimage "${images[0]}"
