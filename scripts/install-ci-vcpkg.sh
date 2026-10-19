#!/usr/bin/env bash
# Retry only dependency transport failures; preserve package/compiler failures.
set -euo pipefail
log=$(mktemp)
trap 'rm -f "$log"' EXIT
for attempt in 1 2 3; do
  if vcpkg "$@" >"$log" 2>&1; then
    cat "$log"
    exit 0
  else
    status=$?
  fi
  cat "$log"
  if [[ "$attempt" -eq 3 ]] ||
     ! grep -Eq 'curl operation failed with error code (5|6|7|18|28|35|52|55|56)( |\()' "$log"; then
    exit "$status"
  fi
  printf '::notice::Dependency download failed; retrying vcpkg (%s/3).\n' "$((attempt + 1))"
  sleep "$((attempt * 2))"
done
