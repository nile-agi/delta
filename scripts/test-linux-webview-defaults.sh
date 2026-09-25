#!/usr/bin/env bash
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"

if grep -q 'set_var("WEBKIT_DISABLE_COMPOSITING_MODE"' "$REPO_ROOT/src-tauri/src/lib.rs"; then
  echo "FAIL: Linux startup still forces software compositing" >&2
  exit 1
fi

grep -q 'libwayland-client\.so\.0' "$REPO_ROOT/scripts/check-linux-runtime.sh" || {
  echo "FAIL: Linux runtime check does not verify the required host Wayland client" >&2
  exit 1
}

echo "Linux WebKit rendering defaults and host dependencies are verified."
