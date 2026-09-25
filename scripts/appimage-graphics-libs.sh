#!/usr/bin/env bash

is_forbidden_graphics_library() {
  local name="${1##*/}"

  case "$name" in
    libwayland-client.so | libwayland-client.so.* | \
      libwayland-cursor.so | libwayland-cursor.so.* | \
      libwayland-egl.so | libwayland-egl.so.* | \
      libwayland-server.so | libwayland-server.so.* | \
      libxkbcommon.so | libxkbcommon.so.* | \
      libxcb-randr.so | libxcb-randr.so.* | \
      libxcb-render.so | libxcb-render.so.* | \
      libxcb-shm.so | libxcb-shm.so.* | \
      libXau.so | libXau.so.* | \
      libXdmcp.so | libXdmcp.so.* | \
      libEGL.so | libEGL.so.* | \
      libGL.so | libGL.so.* | \
      libGLX.so | libGLX.so.* | \
      libOpenGL.so | libOpenGL.so.* | \
      libgbm.so | libgbm.so.* | \
      libdrm.so | libdrm.so.*)
      return 0
      ;;
    *)
      return 1
      ;;
  esac
}

find_forbidden_graphics_libraries() {
  local appdir="$1"
  local manifest
  local path

  [[ -d "$appdir" ]] || {
    echo "AppDir does not exist: $appdir" >&2
    return 2
  }

  manifest="$(mktemp "${TMPDIR:-/tmp}/delta-appimage-files.XXXXXX")" || return 2
  if ! find "$appdir" \( -type f -o -type l \) -print0 >"$manifest"; then
    rm -f "$manifest"
    return 2
  fi

  while IFS= read -r -d '' path; do
    if is_forbidden_graphics_library "$path"; then
      printf '%s\n' "$path"
    fi
  done <"$manifest"
  rm -f "$manifest"
}

remove_forbidden_graphics_libraries() {
  local appdir="$1"
  local manifest
  local path
  local removal_status=0

  [[ -d "$appdir" ]] || {
    echo "AppDir does not exist: $appdir" >&2
    return 2
  }

  manifest="$(mktemp "${TMPDIR:-/tmp}/delta-appimage-files.XXXXXX")" || return 2
  if ! find "$appdir" \( -type f -o -type l \) -print0 >"$manifest"; then
    rm -f "$manifest"
    return 2
  fi

  while IFS= read -r -d '' path; do
    if is_forbidden_graphics_library "$path"; then
      echo "Removing bundled graphics interface: ${path#"$appdir"/}"
      if ! rm -f -- "$path"; then
        removal_status=2
        break
      fi
    fi
  done <"$manifest"
  rm -f "$manifest"
  return "$removal_status"
}

assert_no_forbidden_graphics_libraries() {
  local appdir="$1"
  local found

  if ! found="$(find_forbidden_graphics_libraries "$appdir")"; then
    echo "Could not inspect AppImage graphics libraries" >&2
    return 2
  fi
  if [[ -n "$found" ]]; then
    echo "Forbidden graphics libraries remain in AppImage:" >&2
    printf '%s\n' "$found" >&2
    return 1
  fi
}

assert_valid_appimage_appdir() {
  local appdir="$1"

  [[ -x "$appdir/AppRun" ]] || {
    echo "AppImage AppRun is missing or not executable" >&2
    return 1
  }
  [[ -x "$appdir/usr/bin/delta-app" ]] || {
    echo "AppImage entry point usr/bin/delta-app is missing or not executable" >&2
    return 1
  }
  assert_no_forbidden_graphics_libraries "$appdir"
}
