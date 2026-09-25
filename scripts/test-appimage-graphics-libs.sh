#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
# shellcheck source=appimage-graphics-libs.sh
source "$SCRIPT_DIR/appimage-graphics-libs.sh"

fail() {
  echo "FAIL: $*" >&2
  exit 1
}

assert_forbidden() {
  local name="$1"
  is_forbidden_graphics_library "$name" || fail "expected $name to be forbidden"
}

assert_allowed() {
  local name="$1"
  if is_forbidden_graphics_library "$name"; then
    fail "expected $name to remain bundled"
  fi
}

for name in \
  libwayland-client.so.0 \
  libwayland-cursor.so.0.22.0 \
  libwayland-egl.so.1 \
  libwayland-server.so \
  libxkbcommon.so.0 \
  libxcb-randr.so.0 \
  libxcb-render.so.0 \
  libxcb-shm.so.0 \
  libXau.so.6 \
  libXdmcp.so.6 \
  libEGL.so.1 \
  libGL.so.1.7.0 \
  libGLX.so.0 \
  libOpenGL.so.0 \
  libgbm.so.1 \
  libdrm.so.2; do
  assert_forbidden "$name"
done

for name in libwebkit2gtk-4.1.so.0 libglib-2.0.so.0 libdrm_amdgpu.so.1 libwayland-extra.so.0; do
  assert_allowed "$name"
done

find() {
  echo "simulated traversal failure" >&2
  return 1
}
if assert_no_forbidden_graphics_libraries . >/dev/null 2>&1; then
  fail "verification ignored a filesystem traversal failure"
fi
if remove_forbidden_graphics_libraries . >/dev/null 2>&1; then
  fail "sanitizer ignored a filesystem traversal failure"
fi
unset -f find

fixture="$(mktemp -d)"
trap 'rm -rf "$fixture"' EXIT
mkdir -p "$fixture/usr/lib/nested"
touch \
  "$fixture/usr/lib/libwayland-client.so.0" \
  "$fixture/usr/lib/libxkbcommon.so.0" \
  "$fixture/usr/lib/libxcb-randr.so.0" \
  "$fixture/usr/lib/libxcb-render.so.0" \
  "$fixture/usr/lib/libxcb-shm.so.0" \
  "$fixture/usr/lib/libXau.so.6" \
  "$fixture/usr/lib/libXdmcp.so.6" \
  "$fixture/usr/lib/libEGL.so.1" \
  "$fixture/usr/lib/libwebkit2gtk-4.1.so.0" \
  "$fixture/usr/lib/nested/libgbm.so.1"
ln -s libwayland-client.so.0 "$fixture/usr/lib/libwayland-client.so"

if assert_no_forbidden_graphics_libraries "$fixture" >/dev/null 2>&1; then
  fail "verification accepted an AppDir with bundled graphics libraries"
fi

remove_forbidden_graphics_libraries "$fixture"
assert_no_forbidden_graphics_libraries "$fixture"

[[ -f "$fixture/usr/lib/libwebkit2gtk-4.1.so.0" ]] || fail "allowed library was removed"
[[ ! -e "$fixture/usr/lib/libwayland-client.so.0" ]] || fail "Wayland client library remains"
for name in libxkbcommon.so.0 libxcb-randr.so.0 libxcb-render.so.0 libxcb-shm.so.0 libXau.so.6 libXdmcp.so.6; do
  [[ ! -e "$fixture/usr/lib/$name" ]] || fail "$name remains"
done
[[ ! -L "$fixture/usr/lib/libwayland-client.so" ]] || fail "Wayland client symlink remains"
[[ ! -e "$fixture/usr/lib/libEGL.so.1" ]] || fail "EGL library remains"
[[ ! -e "$fixture/usr/lib/nested/libgbm.so.1" ]] || fail "nested GBM library remains"

mkdir -p "$fixture/usr/bin"
touch "$fixture/usr/bin/delta-app"
chmod +x "$fixture/usr/bin/delta-app"
cat >"$fixture/AppRun" <<'EOF'
#!/usr/bin/env bash
exec usr/bin/delta-app
EOF
chmod +x "$fixture/AppRun"
assert_valid_appimage_appdir "$fixture"

chmod -x "$fixture/AppRun"
if assert_valid_appimage_appdir "$fixture" >/dev/null 2>&1; then
  fail "verification accepted a non-executable AppRun"
fi

echo "AppImage graphics library tests passed."
