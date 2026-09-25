#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
fixture="$(mktemp -d)"
trap 'rm -rf "$fixture"' EXIT

mkdir -p \
  "$fixture/source/usr/bin" \
  "$fixture/source/usr/lib" \
  "$fixture/target/release/bundle/appimage" \
  "$fixture/repacked"

cat >"$fixture/source/AppRun" <<'EOF'
#!/usr/bin/env bash
exec usr/bin/delta-app
EOF
cat >"$fixture/source/usr/bin/delta-app" <<'EOF'
#!/usr/bin/env bash
exit 0
EOF
chmod +x "$fixture/source/AppRun" "$fixture/source/usr/bin/delta-app"
touch "$fixture/source/usr/lib/libwayland-client.so.0"
touch "$fixture/source/usr/lib/libwebkit2gtk-4.1.so.0"

cat >"$fixture/target/release/bundle/appimage/Delta.AppImage" <<'EOF'
#!/usr/bin/env bash
set -euo pipefail
case "${1:-}" in
  --appimage-extract) cp -R "$FAKE_INITIAL_APPDIR" squashfs-root ;;
  --appimage-offset) echo 4 ;;
  *) exit 1 ;;
esac
EOF
chmod +x "$fixture/target/release/bundle/appimage/Delta.AppImage"

cat >"$fixture/appimagetool" <<'EOF'
#!/usr/bin/env bash
set -euo pipefail
source_dir="${@: -2:1}"
output="${@: -1}"
[[ "$1" == "--runtime-file" ]] || exit 10
[[ -s "$2" ]] || exit 11
rm -rf "$FAKE_REPACKED_APPDIR"
cp -R "$source_dir" "$FAKE_REPACKED_APPDIR"
cat >"$output" <<'INNER'
#!/usr/bin/env bash
set -euo pipefail
[[ "${1:-}" == "--appimage-extract" ]]
cp -R "$FAKE_REPACKED_APPDIR" squashfs-root
INNER
chmod +x "$output"
EOF
chmod +x "$fixture/appimagetool"

cat >"$fixture/tauri" <<'EOF'
#!/usr/bin/env bash
printf '%s\n' "$*" >"$FAKE_TAURI_LOG"
EOF
chmod +x "$fixture/tauri"

export FAKE_INITIAL_APPDIR="$fixture/source"
export FAKE_REPACKED_APPDIR="$fixture/repacked/AppDir"
export FAKE_TAURI_LOG="$fixture/tauri.log"
export APPIMAGETOOL_PATH="$fixture/appimagetool"
export TAURI_RELEASE_APPIMAGE_DIR="$fixture/target"
export TAURI_CLI_PATH="$fixture/tauri"

"$SCRIPT_DIR/tauri-release-build.sh" build --target x86_64-unknown-linux-gnu
grep -q '^build --target x86_64-unknown-linux-gnu$' "$fixture/tauri.log"
[[ ! -e "$fixture/repacked/AppDir/usr/lib/libwayland-client.so.0" ]]
[[ -e "$fixture/repacked/AppDir/usr/lib/libwebkit2gtk-4.1.so.0" ]]

cp "$fixture/target/release/bundle/appimage/Delta.AppImage" \
  "$fixture/target/release/bundle/appimage/Second.AppImage"
if "$SCRIPT_DIR/tauri-release-build.sh" build >/dev/null 2>&1; then
  echo "FAIL: wrapper accepted multiple AppImages" >&2
  exit 1
fi

rm "$fixture/target/release/bundle/appimage/Second.AppImage"
mkdir -p "$fixture/failing-find"
cat >"$fixture/failing-find/find" <<'EOF'
#!/usr/bin/env bash
printf '%s\0' "$TAURI_RELEASE_APPIMAGE_DIR/release/bundle/appimage/Delta.AppImage"
exit 7
EOF
chmod +x "$fixture/failing-find/find"
if PATH="$fixture/failing-find:$PATH" \
  "$SCRIPT_DIR/tauri-release-build.sh" build >/dev/null 2>&1; then
  echo "FAIL: wrapper ignored an AppImage discovery failure" >&2
  exit 1
fi

echo "Tauri release build wrapper tests passed."
