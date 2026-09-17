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
echo "  webkit compositing: ${WEBKIT_DISABLE_COMPOSITING_MODE:-unset}"

missing=0
if command -v ldconfig >/dev/null 2>&1; then
  for library in libwebkit2gtk-4.1.so.0 libgtk-3.so.0 libEGL.so.1 libGL.so.1; do
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
  echo "Install the Fedora WebKitGTK/GTK/EGL runtime packages before launching the AppImage." >&2
  echo "For a blank window, try: GDK_BACKEND=x11 ./Delta_*.AppImage" >&2
  echo "Only as a sandbox fallback: WEBKIT_FORCE_SANDBOX=0 ./Delta_*.AppImage" >&2
  exit 1
fi

echo "Linux WebView runtime libraries are present."
