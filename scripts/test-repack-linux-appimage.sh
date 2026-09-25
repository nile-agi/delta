#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
fixture="$(mktemp -d)"
trap 'rm -rf "$fixture"' EXIT

mkdir -p "$fixture/source/usr/bin" "$fixture/source/usr/lib" "$fixture/packed"
touch "$fixture/source/usr/bin/delta-app"
touch "$fixture/source/usr/lib/libwayland-client.so.0"
touch "$fixture/source/usr/lib/libwebkit2gtk-4.1.so.0"
cat >"$fixture/source/AppRun" <<'EOF'
#!/usr/bin/env bash
exec usr/bin/delta-app
EOF
chmod +x "$fixture/source/AppRun" "$fixture/source/usr/bin/delta-app"

cat >"$fixture/Delta.AppImage" <<'EOF'
#!/usr/bin/env bash
set -euo pipefail
case "${1:-}" in
  --appimage-extract) cp -R "$FAKE_APPDIR_SOURCE" squashfs-root ;;
  --appimage-offset) echo 4 ;;
  *) exit 1 ;;
esac
EOF
chmod +x "$fixture/Delta.AppImage"
cp "$fixture/Delta.AppImage" "$fixture/Delta.template"

cat >"$fixture/appimagetool" <<'EOF'
#!/usr/bin/env bash
set -euo pipefail
source_dir="${@: -2:1}"
output="${@: -1}"
[[ "$1" == "--runtime-file" ]] || exit 10
[[ -s "$2" ]] || exit 11
[[ ! -e "$source_dir/usr/lib/libwayland-client.so.0" ]]
[[ -e "$source_dir/usr/lib/libwebkit2gtk-4.1.so.0" ]]
if [[ "${FAKE_PACK_MODE:-}" == "fail" ]]; then
  printf 'partial output' >"$output"
  exit 12
fi
if [[ "${FAKE_PACK_MODE:-}" == "corrupt" ]]; then
  printf '#!/usr/bin/env bash\nexit 13\n' >"$output"
  chmod +x "$output"
  exit 0
fi
rm -rf "$FAKE_PACKED_APPDIR"
cp -R "$source_dir" "$FAKE_PACKED_APPDIR"
cat >"$output" <<'INNER'
#!/usr/bin/env bash
set -euo pipefail
[[ "${1:-}" == "--appimage-extract" ]] || exit 1
cp -R "$FAKE_PACKED_APPDIR" squashfs-root
INNER
chmod +x "$output"
EOF
chmod +x "$fixture/appimagetool"

export FAKE_APPDIR_SOURCE="$fixture/source"
export FAKE_PACKED_APPDIR="$fixture/packed/AppDir"
APPIMAGETOOL_PATH="$fixture/appimagetool" \
  "$SCRIPT_DIR/repack-linux-appimage.sh" "$fixture/Delta.AppImage"

cmp -s "$fixture/Delta.template" "$fixture/Delta.AppImage" && {
  echo "FAIL: successful repack did not replace the original image" >&2
  exit 1
}
[[ -x "$fixture/Delta.AppImage" ]]

for mode in fail corrupt; do
  candidate="$fixture/${mode}.AppImage"
  cp "$fixture/Delta.template" "$candidate"
  if FAKE_PACK_MODE="$mode" APPIMAGETOOL_PATH="$fixture/appimagetool" \
    "$SCRIPT_DIR/repack-linux-appimage.sh" "$candidate" >/dev/null 2>&1; then
    echo "FAIL: $mode appimagetool output was accepted" >&2
    exit 1
  fi
  cmp -s "$fixture/Delta.template" "$candidate" || {
    echo "FAIL: $mode repack changed the original image" >&2
    exit 1
  }
done

if "$SCRIPT_DIR/repack-linux-appimage.sh" "$fixture/missing.AppImage" >/dev/null 2>&1; then
  echo "FAIL: missing input was accepted" >&2
  exit 1
fi

if APPIMAGETOOL_PATH="$fixture/missing-tool" \
  "$SCRIPT_DIR/repack-linux-appimage.sh" "$fixture/Delta.AppImage" >/dev/null 2>&1; then
  echo "FAIL: missing appimagetool was accepted" >&2
  exit 1
fi

echo "AppImage repacking tests passed."
