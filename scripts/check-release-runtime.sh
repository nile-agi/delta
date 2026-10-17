#!/usr/bin/env bash
set -euo pipefail
binary="$1"
target="$2"
root="$(cd "$(dirname "$0")/.." && pwd)"
work="$(mktemp -d "${TMPDIR:-/tmp}/delta-release-runtime.XXXXXX")"
trap 'rm -rf "$work"' EXIT
# Immutable upstream fixture; independent of the user's model downloads or settings.
curl -fL --retry 2 --max-time 90 \
  https://huggingface.co/ggml-org/models/resolve/499bc8821c6b12b4e53c5bffcb21ec206f212d81/tinyllamas/stories260K.gguf \
  -o "$work/model.gguf"
args=(--binary "$binary" --model "$work/model.gguf")
if [[ "$target" == x86_64-unknown-linux-gnu ]]; then args+=(--qemu); fi
python3 "$root/scripts/smoke-model-runtime.py" "${args[@]}"
