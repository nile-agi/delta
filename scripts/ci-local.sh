#!/usr/bin/env bash
#
# Runs CI's checks on this machine, the same ones CircleCI runs (.circleci/), without spending credits.
#
#   scripts/ci-local.sh                 # the areas changed since origin/dev, like CI on a PR
#   scripts/ci-local.sh engine frontend release   # or name them
#   QUICK=1 scripts/ci-local.sh frontend          # skip UI stories and production startup tests
#
# Engine: builds delta and delta-server and runs the engine tests natively; with Docker it also
# compiles the changed C++ files with GCC 11 (Ubuntu 22.04, as CI). Windows is not covered.

set -uo pipefail
cd "$(dirname "$0")/.."

BASE="${BASE:-origin/dev}"
BUILD_DIR="${BUILD_DIR:-build}"
JOBS="${JOBS:-3}"
RESULTS=()
FAILED=0

# CI pins pnpm 11; a newer local pnpm changes Storybook's resolution.
if [[ "$(pnpm --version 2>/dev/null)" == 11.* ]]; then PNPM=(pnpm); else PNPM=(npx -y pnpm@11); fi

step() {
  local name="$1"
  shift
  echo -e "\n\033[1m▶ $name\033[0m"
  if "$@"; then
    RESULTS+=("  ok    $name")
  else
    RESULTS+=("  FAIL  $name")
    FAILED=1
  fi
}

changed_areas() {
  local files
  files="$(git diff --name-only "$(git merge-base "$BASE" HEAD)" HEAD; git diff --name-only HEAD)"
  grep -qE '^(web/app/|package.json|pnpm-lock.yaml|pnpm-workspace.yaml|\.circleci/)' <<<"$files" && echo frontend
  grep -qE '^(engine/|tests/|CMakeLists.txt|version.txt|\.gitmodules|\.circleci/)' <<<"$files" && echo engine
  grep -qE '^(scripts/|\.github/|\.circleci/|dist/|src-tauri/|version.txt)' <<<"$files" && echo release
}

engine_configure() {
  [[ -f "$BUILD_DIR/CMakeCache.txt" ]] && return 0
  local args=(-DCMAKE_BUILD_TYPE=Release -DBUILD_TESTS=OFF -DBUILD_WEBUI=OFF -DLLAMA_OPENSSL=OFF -DUSE_CURL=ON)
  [[ "$(uname)" == Darwin ]] && args+=(-DGGML_METAL=ON) || args+=(-DGGML_METAL=OFF)
  cmake -S . -B "$BUILD_DIR" "${args[@]}"
}

engine_tests() {
  local t
  for t in delta-document-tests delta-update-tests delta-harness-tests delta-openai-tests delta-ocr-tests; do
    "./$BUILD_DIR/$t" >"/tmp/ci-local-$t.log" 2>&1 || { tail -30 "/tmp/ci-local-$t.log"; return 1; }
    echo "$t: $(tail -1 "/tmp/ci-local-$t.log")"
  done
}

gcc11_check() {
  command -v docker >/dev/null && docker info >/dev/null 2>&1 || { echo "Docker not running; skipped"; return 0; }
  local files
  # Against the working tree, so uncommitted changes are compiled too.
  files="$({ git diff --name-only "$(git merge-base "$BASE" HEAD)" -- 'engine/*.cpp' 'tests/*.cpp'
    git ls-files --others --exclude-standard -- 'engine/*.cpp' 'tests/*.cpp'; } |
    grep -v '^engine/vendor/' | sort -u | tr '\n' ' ')"
  [[ -z "$files" ]] && { echo "no changed C++ files"; return 0; }
  local inc="-Iengine/vendor/sqlite3 -Iengine -Iengine/vendor -Iengine/vendor/llama.cpp -Iengine/vendor/llama.cpp/include
    -Iengine/vendor/llama.cpp/common -Iengine/vendor/llama.cpp/tools/server -Iengine/vendor/llama.cpp/vendor
    -Iengine/vendor/llama.cpp/vendor/nlohmann -Iengine/vendor/llama.cpp/vendor/cpp-httplib -Iengine/agent
    -Iengine/vendor/llama.cpp/ggml/include"
  docker run --rm -v "$PWD":/src -w /src ubuntu:22.04 bash -c "
    apt-get update -qq >/dev/null && apt-get install -y -qq g++ libcurl4-openssl-dev >/dev/null 2>&1 || exit 1
    rc=0
    for f in $files; do g++ -std=c++17 -fsyntax-only $(echo $inc) \"\$f\" || rc=1; echo \"  \$f\"; done
    exit \$rc"
}

run_engine() {
  step "engine: configure" engine_configure
  step "engine: build delta, delta-server and tests" cmake --build "$BUILD_DIR" -j "$JOBS" --target delta \
    --target delta-server --target delta-document-tests --target delta-update-tests \
    --target delta-harness-tests --target delta-openai-tests --target delta-ocr-tests
  step "engine: tests" engine_tests
  step "engine: GCC 11 compile of changed files" gcc11_check
}

run_frontend() {
  local web=("${PNPM[@]}" --filter webui)
  step "frontend: svelte-check" "${web[@]}" check
  step "frontend: unit tests" "${web[@]}" exec vitest run --project=server
  step "frontend: browser component tests" "${web[@]}" exec vitest run --project=client --browser.headless \
    --no-file-parallelism
  if [[ -z "${QUICK:-}" ]]; then
    step "frontend: UI stories" "${web[@]}" exec vitest run --project=ui --browser.headless --no-file-parallelism \
      --testTimeout=60000
  fi
  step "frontend: build" "${web[@]}" build
  step "frontend: build output" test -s public/index.html
  if [[ -z "${QUICK:-}" ]]; then
    step "frontend: production startup" env DELTA_TEST_PREBUILT=1 "${web[@]}" exec playwright test \
      e2e/startup.test.ts --workers=1
  fi
}

run_release() {
  step "release: node script tests" node --test scripts/release-notes.test.cjs scripts/bump-version.test.cjs \
    scripts/update-package-manifests.test.cjs scripts/github-release.test.cjs
  local t
  for t in test-appimage-graphics-libs test-linux-webview-defaults test-repack-linux-appimage \
    test-verify-linux-appimage test-tauri-release-build test-release-packaging; do
    step "release: $t" bash "scripts/$t.sh"
  done
  if command -v circleci >/dev/null; then
    step "release: CircleCI config" bash -c 'circleci config validate .circleci/config.yml &&
      circleci config validate .circleci/continue.yml'
  fi
  step "release: root lockfile matches package.json" "${PNPM[@]}" install --frozen-lockfile --lockfile-only
}

# Plain loops and guarded expansions: macOS still ships bash 3.2 (no mapfile, strict empty arrays).
AREAS=("$@")
if [[ $# -eq 0 ]]; then
  AREAS=()
  while read -r area; do [[ -n "$area" ]] && AREAS+=("$area"); done < <(changed_areas)
fi
[[ ${#AREAS[@]} -eq 0 ]] && { echo "Nothing CI would check changed since $BASE."; exit 0; }
echo "Checking: ${AREAS[*]} (base $BASE)"

for area in "${AREAS[@]}"; do
  case "$area" in
    engine) run_engine ;;
    frontend) run_frontend ;;
    release) run_release ;;
    *) echo "Unknown area: $area (use engine, frontend or release)"; exit 2 ;;
  esac
done

echo -e "\n\033[1mSummary\033[0m"
printf '%s\n' ${RESULTS[@]+"${RESULTS[@]}"}
exit "$FAILED"
