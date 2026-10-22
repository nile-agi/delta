#!/bin/sh
# Installs the Delta CLI on macOS or Linux:
#   curl -fsSL https://github.com/nile-agi/delta-release/releases/latest/download/install.sh | sh
# Set DELTA_VERSION=<tag> (e.g. v1.0.18-rc.5) to install a specific release.
set -eu

REPO="https://github.com/nile-agi/delta-release"
DEST="$HOME/.local/share/delta-cli"

fail() { echo "delta install: $*" >&2; exit 1; }
command -v curl >/dev/null 2>&1 || fail "curl is required"

case "$(uname -s)/$(uname -m)" in
  Darwin/arm64) ASSET=delta-cli-macos-arm64.tar.gz ;;
  Darwin/x86_64) ASSET=delta-cli-macos-x86_64.tar.gz ;;
  Linux/x86_64|Linux/amd64) ASSET=delta-cli-linux-x86_64.tar.gz ;;
  *) fail "no CLI build for $(uname -s) $(uname -m); see $REPO/releases" ;;
esac

# Resolve "latest" to its tag so the versioned checksum file can be found.
TAG="${DELTA_VERSION:-}"
if [ -z "$TAG" ]; then
  TAG="$(curl -fsSLI -o /dev/null -w '%{url_effective}' "$REPO/releases/latest")"
  TAG="${TAG##*/}"
fi
case "$TAG" in v*) ;; *) fail "could not find the latest release" ;; esac
BASE="$REPO/releases/download/$TAG"

TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT
echo "Downloading Delta CLI $TAG ($ASSET)..."
curl -fL --progress-bar -o "$TMP/$ASSET" "$BASE/$ASSET" || fail "download failed"

if curl -fsSL -o "$TMP/sums" "$BASE/checksums-${TAG#v}.sha256" 2>/dev/null; then
  want="$(awk -v f="$ASSET" '$2 == f || $2 == "*" f { print $1 }' "$TMP/sums")"
  if [ -n "$want" ]; then
    if command -v sha256sum >/dev/null 2>&1; then got="$(sha256sum "$TMP/$ASSET" | awk '{print $1}')"
    else got="$(shasum -a 256 "$TMP/$ASSET" | awk '{print $1}')"; fi
    [ "$got" = "$want" ] || fail "checksum mismatch for $ASSET"
  fi
fi

rm -rf "$DEST"
mkdir -p "$DEST"
tar -xzf "$TMP/$ASSET" -C "$DEST" --strip-components=1
# The programs are unsigned; clear the quarantine flag so macOS lets them run.
[ "$(uname -s)" = Darwin ] && xattr -dr com.apple.quarantine "$DEST" 2>/dev/null || true

case "${SHELL:-}" in
  */zsh) RC="$HOME/.zshrc" ;;
  */bash) RC="$HOME/.bashrc" ;;
  *) RC="$HOME/.profile" ;;
esac
# shellcheck disable=SC2016 # written literally so the rc file expands it
LINE='export PATH="$HOME/.local/share/delta-cli/bin:$PATH"'
grep -qsF "$LINE" "$RC" || printf '\n%s\n' "$LINE" >> "$RC"

if [ "$(uname -s)" = Linux ] && command -v ldconfig >/dev/null 2>&1 &&
   ! ldconfig -p 2>/dev/null | grep -q 'libcurl\.so\.4'; then
  echo "Note: delta needs libcurl. On Ubuntu/Debian run: sudo apt install -y curl"
fi

# A launcher in a folder already on PATH makes `delta` work in this terminal right away.
LINKED=""
for dir in "$HOME/.local/bin" "$HOME/bin" /usr/local/bin; do
  case ":$PATH:" in *":$dir:"*) ;; *) continue ;; esac
  if [ -d "$dir" ] && [ -w "$dir" ]; then
    printf '#!/bin/sh\nexec "%s/bin/delta" "$@"\n' "$DEST" > "$dir/delta" && chmod +x "$dir/delta" && LINKED="$dir" && break
  fi
done

"$DEST/bin/delta" --version 2>/dev/null || true
if [ -n "$LINKED" ]; then
  echo "Installed. Run: delta"
else
  echo "Installed. Run: delta   (in this terminal, first run: export PATH=\"$DEST/bin:\$PATH\")"
fi
