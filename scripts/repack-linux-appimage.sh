#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
# shellcheck source=appimage-graphics-libs.sh
source "$SCRIPT_DIR/appimage-graphics-libs.sh"

fail() {
  echo "ERROR: $*" >&2
  exit 1
}

[[ $# -eq 1 ]] || fail "usage: $0 <AppImage>"

input="$1"
[[ -f "$input" ]] || fail "AppImage not found: $input"
[[ -x "$input" ]] || fail "AppImage is not executable: $input"

tool="${APPIMAGETOOL_PATH:-}"
[[ -n "$tool" ]] || fail "APPIMAGETOOL_PATH is not set"
[[ -x "$tool" ]] || fail "appimagetool is not executable: $tool"

input_dir="$(cd "$(dirname "$input")" && pwd)"
input_name="$(basename "$input")"
input="$input_dir/$input_name"
tool="$(cd "$(dirname "$tool")" && pwd)/$(basename "$tool")"

work_dir="$(mktemp -d "${TMPDIR:-/tmp}/delta-appimage.XXXXXX")"
output="$input_dir/.${input_name}.repacked.$$.AppImage"
runtime="$work_dir/runtime-x86_64"
cleanup() {
  rm -rf "$work_dir"
  rm -f "$output"
}
trap cleanup EXIT

if ! runtime_offset="$("$input" --appimage-offset)"; then
  fail "could not read the AppImage runtime offset"
fi
[[ "$runtime_offset" =~ ^[0-9]+$ ]] || fail "invalid AppImage runtime offset: $runtime_offset"
(( runtime_offset > 0 )) || fail "AppImage runtime offset must be positive"
head -c "$runtime_offset" "$input" >"$runtime"
[[ -s "$runtime" ]] || fail "could not preserve the original AppImage runtime"

(
  cd "$work_dir"
  "$input" --appimage-extract >/dev/null
)

appdir="$work_dir/squashfs-root"
remove_forbidden_graphics_libraries "$appdir"
assert_valid_appimage_appdir "$appdir" || fail "extracted AppImage layout is invalid"

ARCH=x86_64 APPIMAGE_EXTRACT_AND_RUN=1 \
  "$tool" --runtime-file "$runtime" "$appdir" "$output" >/dev/null
[[ -s "$output" ]] || fail "appimagetool did not create an output image"
chmod +x "$output"

repacked_dir="$work_dir/repacked"
mkdir "$repacked_dir"
(
  cd "$repacked_dir"
  "$output" --appimage-extract >/dev/null
)
assert_valid_appimage_appdir "$repacked_dir/squashfs-root" ||
  fail "repacked AppImage validation failed"

mv -f "$output" "$input"

echo "Repacked AppImage with host graphics libraries: $input"
