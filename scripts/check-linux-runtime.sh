#!/usr/bin/env bash
set -u

if [[ "$(uname -s)" != "Linux" ]]; then
  echo "Linux runtime check skipped on $(uname -s)."
  exit 0
fi

echo "Delta Linux runtime diagnostics"
echo "  session: ${XDG_SESSION_TYPE:-unknown}"
echo "  wayland: ${WAYLAND_DISPLAY:-unset}"
echo "  x11: ${DISPLAY:-unset}"
echo "  appimage: ${APPIMAGE:-not running from AppImage}"
echo "  webkit compositing: ${WEBKIT_DISABLE_COMPOSITING_MODE:-unset}"
echo "  library path: ${LD_LIBRARY_PATH:-system default}"

if [[ "${WEBKIT_DISABLE_COMPOSITING_MODE:-}" == "1" ]]; then
  echo "  warning: WebKit software compositing is enabled; unset it to restore GPU rendering" >&2
fi

missing=0
if command -v ldconfig >/dev/null 2>&1; then
  for library in \
    libwebkit2gtk-4.1.so.0 \
    libgtk-3.so.0 \
    libwayland-client.so.0 \
    libxkbcommon.so.0 \
    libxcb-randr.so.0 \
    libxcb-render.so.0 \
    libxcb-shm.so.0 \
    libXau.so.6 \
    libXdmcp.so.6 \
    libEGL.so.1 \
    libGL.so.1; do
    if ldconfig -p 2>/dev/null | grep -q "${library}"; then
      echo "  ok: ${library}"
    else
      echo "  missing: ${library}" >&2
      missing=1
    fi
  done
else
  echo "  warning: ldconfig is unavailable; runtime library presence could not be checked" >&2
fi

if (( missing != 0 )); then
  echo "Install the Fedora WebKitGTK, GTK, display, and EGL runtime packages before launching the AppImage." >&2
  echo "On Fedora, the RPM package is preferred because it declares these dependencies." >&2
  echo "For a blank window, try: GDK_BACKEND=x11 ./Delta_*.AppImage" >&2
  exit 1
fi

echo "Linux WebView runtime libraries are present; Delta will use the host graphics stack."
