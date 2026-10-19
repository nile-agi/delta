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

# tauri-action uploads only after this command succeeds. Test the final repacked
# AppImage and final Debian package before allowing the action to attach assets.
if [[ "${DELTA_VERIFY_PACKAGED_RUNTIME:-0}" == 1 ]]; then
  runtime_work="$(mktemp -d "${TMPDIR:-/tmp}/delta-packaged-runtime.XXXXXX")"
  trap 'rm -rf "$runtime_work"' EXIT
  report_root="${DELTA_RUNTIME_REPORT_DIR:-$runtime_work/reports}"
  deb_manifest="$runtime_work/debs"
  find "$APPIMAGE_ROOT" -type f -name '*.deb' -print0 > "$deb_manifest"
  debs=()
  while IFS= read -r -d '' package; do debs+=("$package"); done < "$deb_manifest"
  if [[ ${#debs[@]} -ne 1 ]]; then
    echo "ERROR: expected exactly one final Debian package, found ${#debs[@]}" >&2
    exit 1
  fi
  # Keep release minutes bounded. Broader distro checks are available for local
  # investigations through an explicit space-separated DELTA_RUNTIME_IMAGES value.
  read -r -a runtime_images <<< "${DELTA_RUNTIME_IMAGES:-ubuntu:22.04}"
  for runtime in "${runtime_images[@]}"; do
    for package in "${debs[0]}" "${images[0]}"; do
      if [[ "$runtime" == fedora:* && "$package" == *.deb ]]; then continue; fi
      python3 "$REPO_ROOT/scripts/check-packaged-runtime.py" \
        --artifact "$package" --runtime "$runtime" \
        --fixtures-manifest "$REPO_ROOT/scripts/runtime-fixtures.json" \
        --fixture-cache "$runtime_work/models" \
        --output "$report_root/${runtime//:/-}/$(basename "$package").json"
    done
  done
fi
