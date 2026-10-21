#!/usr/bin/env bash
set -euo pipefail
binary="$1"
target="$2"
# "native" for the AVX2 llama-server-v3: QEMU 6.2 cannot emulate AVX2, the runner's CPU can.
mode="${3:-portable}"
root="$(cd "$(dirname "$0")/.." && pwd)"
work="$(mktemp -d "${TMPDIR:-/tmp}/delta-release-runtime.XXXXXX")"
trap 'rm -rf "$work"' EXIT
# Immutable upstream fixture; independent of the user's model downloads or settings.
curl -fL --retry 2 --max-time 90 \
  https://huggingface.co/ggml-org/models/resolve/499bc8821c6b12b4e53c5bffcb21ec206f212d81/tinyllamas/stories260K.gguf \
  -o "$work/model.gguf"
args=(--binary "$binary" --model "$work/model.gguf")
if [[ "$target" == x86_64-unknown-linux-gnu && "$mode" != native ]]; then args+=(--qemu); fi
python3 "$root/scripts/smoke-model-runtime.py" "${args[@]}"
