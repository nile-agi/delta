#!/usr/bin/env bash
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
work="$(mktemp -d "${TMPDIR:-/tmp}/delta-web-assets.XXXXXX")"
trap 'rm -rf "$work"' EXIT
mkdir -p "$work/scripts" "$work/public/_app"
cp "$root/scripts/verify-tauri-release-assets.sh" "$root/scripts/appimage-graphics-libs.sh" "$work/scripts/"
printf 'app bundle' > "$work/public/_app/app.js"
cp "$root/web/app/static/startup-guard.js" "$root/web/app/static/failure-report.js" "$work/public/"
cat > "$work/public/index.html" <<'HTML'
<div id="app-loading"></div>
<script src="/failure-report.js"></script>
<script src="/startup-guard.js"></script>
HTML
verify() { bash "$work/scripts/verify-tauri-release-assets.sh" --web-only; }
reject() {
 if verify > "$work/error" 2>&1; then
  echo "FAIL: accepted $1" >&2; exit 1
 fi
 grep -q "$2" "$work/error" || { cat "$work/error" >&2; exit 1; }
}
verify
mv "$work/public/startup-guard.js" "$work/startup-guard.js"
reject 'missing startup helper' 'startup-guard.js'
mv "$work/startup-guard.js" "$work/public/startup-guard.js"
cp "$work/public/index.html" "$work/index.html"
sed '/src="\/startup-guard.js"/d' "$work/index.html" > "$work/public/index.html"
reject 'unreferenced startup helper' 'startup-guard.js'
cp "$work/index.html" "$work/public/index.html"
printf '// empty stale helper\n' > "$work/public/startup-guard.js"
reject 'stale startup helper' '__DELTA_PORT__'
cp "$root/web/app/static/startup-guard.js" "$work/public/startup-guard.js"
sed '/src="\/failure-report.js"/d' "$work/index.html" > "$work/public/index.html"
reject 'unreferenced report helper' 'failure-report.js'
cp "$work/index.html" "$work/public/index.html"
printf '// empty stale report helper\n' > "$work/public/failure-report.js"
reject 'stale report helper' 'diagnostic reporting'
rm "$work/public/failure-report.js"
reject 'missing report helper' 'failure-report.js'
echo 'Tauri web asset validation tests passed.'
