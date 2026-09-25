#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
fixture="$(mktemp -d)"
trap 'rm -rf "$fixture"' EXIT

make_appdir() {
  local root="$1"
  mkdir -p "$root/usr/bin" "$root/usr/lib"
  cat >"$root/AppRun" <<'EOF'
#!/usr/bin/env bash
exec usr/bin/delta-app
EOF
  cat >"$root/usr/bin/delta-app" <<'EOF'
#!/usr/bin/env bash
exit 0
EOF
  chmod +x "$root/AppRun" "$root/usr/bin/delta-app"
  touch "$root/usr/lib/libwebkit2gtk-4.1.so.0"
}

make_appdir "$fixture/good"
cat >"$fixture/Delta.AppImage" <<'EOF'
#!/usr/bin/env bash
set -euo pipefail
[[ "${1:-}" == "--appimage-extract" ]]
cp -R "$VERIFY_APPDIR_SOURCE" squashfs-root
EOF
chmod +x "$fixture/Delta.AppImage"

export VERIFY_APPDIR_SOURCE="$fixture/good"
"$SCRIPT_DIR/verify-tauri-release-assets.sh" --appimage "$fixture/Delta.AppImage"

cp -R "$fixture/good" "$fixture/bad"
touch "$fixture/bad/usr/lib/libxkbcommon.so.0"
export VERIFY_APPDIR_SOURCE="$fixture/bad"
if "$SCRIPT_DIR/verify-tauri-release-assets.sh" --appimage "$fixture/Delta.AppImage" \
  >/dev/null 2>&1; then
  echo "FAIL: verifier accepted a bundled display library" >&2
  exit 1
fi

echo "Linux AppImage verification tests passed."
