# Engine Runtime Readiness Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans for inline execution, or superpowers:subagent-driven-development if the user selects delegation. Implement task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make model loading truthful and recoverable, and demonstrate the fix through locally runnable small models and packaged Linux tests.

**Architecture:** A C++ runtime controller owns selection attempts and publishes authoritative snapshots. The existing wrapper supplies process operations, the model API accepts requests without waiting for a full load, and the frontend follows attempt status. Separate owned-process and failure-classification helpers keep platform code and recovery policy out of HTTP handlers.

**Tech Stack:** C++17, cpp-httplib, nlohmann JSON, Python standard-library process tests, Svelte/TypeScript/Vitest, Rust/Tauri and GitHub Actions.

**Spec:** `docs/superpowers/specs/2026-10-18-engine-runtime-readiness-design.md`

## Global Constraints

- Work on `fix/engine-runtime-readiness`, based on `dev`; preserve unrelated untracked `.plans/` and `scripts/eval/`.
- Keep existing model files and valid saved overrides; no automatic redownloads or settings resets.
- Engine startup deadline: 30 seconds. Selection deadline: 180 seconds, including automatic recovery. Use a monotonic clock and probes of at most two seconds bounded by remaining time.
- Keep the most recent 16 attempt snapshots. Unknown or evicted IDs return 404.
- One automatic CPU retry only after confirmed GPU failure when GPU layers were attempted. Graceful owned-process termination has a two-second bound before force termination.
- Retain portable CPU build policy and current macOS deployment floor. Add no product dependencies or new runtime framework.
- Reports exclude chat, documents, secrets and full private paths. Public reporting is user-controlled at `https://github.com/nile-agi/delta-release/issues/new/choose`.
- Atomic commits with one- or two-line bodies, no co-author/generated attribution. Do not push, open a PR or publish a release as part of this plan.
- Run Gemma 3 270M and Qwen 2.5 0.5B sequentially from existing local files, isolated profiles, CPU-only two threads, one slot, 2,048 context, batch/microbatch 64 and a 2 GiB process-tree RSS guard. No model download is needed locally.
- Local macOS results cannot establish Linux/Mint compatibility. Remove any additional model downloaded for testing afterward.

## Review Focus

- Two retries for the same model: late completion from the earlier attempt must never mark the newer one ready (Task 1).
- A malformed/missing alias or malformed worker props: keep chat disabled and report a bounded failure, even when router health is 200 (Task 2).
- An idle client or child inheriting the stderr pipe: cancellation and shutdown must still finish and preserve final failure evidence (Task 4).
- A lost selection response followed by reconnect: the UI must recover current attempt identity without starting a duplicate load (Task 3).
- A final package that accidentally resolves build-host libraries or tests only F32 tensors: release verification must fail (Task 6).

## File boundaries

Create `engine/model_runtime.{h,cpp}` for attempt state, deadlines and worker scheduling; `engine/runtime_config.{h,cpp}` for validated effective configuration and GGUF preflight; `engine/runtime_failure.{h,cpp}` for classification/retry policy; and `engine/owned_process.{h,cpp}` for native process ownership and output lifetime.

Adapt `engine/delta_server_wrapper.cpp`, `engine/server_health.{h,cpp}` and `engine/model_api_server.{h,cpp}` at their existing boundaries. Add focused C++ test executables to `CMakeLists.txt`; do not reorganize unrelated engine files or edit vendored llama.cpp.

Frontend changes stay in `web/app/src/lib/services/models.ts`, `web/app/src/lib/stores/models.svelte.ts`, their tests, and `web/app/src/lib/components/app/chat/ChatForm/ChatFormModelSelector.svelte` for load-state actions. Rust changes stay in `src-tauri/src/lib.rs` and `src-tauri/src/diagnostics.rs`.

### Task 1: Attempt identity and deterministic lifecycle

**Files:** Create `engine/model_runtime.{h,cpp}`, the shared value-type headers `engine/runtime_config.h` and `engine/runtime_failure.h`, and `tests/test_model_runtime.cpp`; modify `CMakeLists.txt`.

**Interfaces:** In namespace `delta`, define `RuntimeConfig` as the value type described in Task 2 and `RuntimeFailure` as the evidence/error value type consumed in Task 5; these headers must be complete so this task builds independently. Define `EngineState`, `ModelLoadState`, and `RuntimeSnapshot` with attempt ID, generation, model alias, engine/selection state, effective config and optional failure. `ModelRuntime::select(const RuntimeConfig&) -> RuntimeSnapshot`, `current() const -> optional<RuntimeSnapshot>`, `get(const string&) const -> optional<RuntimeSnapshot>`, `cancel(const string&) -> optional<RuntimeSnapshot>` and `shutdown() -> void` are the public controller interface. Inject a monotonic clock and process/readiness hooks so tests do not wait 180 seconds. One owned worker thread serializes operations; never hold the snapshot mutex during HTTP/process waits.

- [ ] Write `test_model_runtime.cpp` cases asserting: router available is not selection ready; ready requires verified worker; same-model retry increments generation; stale completion cannot overwrite; old-ID cancel cannot cancel current; cancel is idempotent; attempt 17 evicts attempt 1; fake-clock deadline expires at 180 seconds and remains terminal. Include shutdown while a probe is pending.
- [ ] Add EXCLUDE_FROM_ALL target `delta-model-runtime-tests`, run it and record the expected missing-interface failure.
- [ ] Implement the controller transitions and bounded snapshot retention. A new selection invalidates the old generation immediately and queues owned cleanup before the next launch.
- [ ] Build and run `delta-model-runtime-tests`; all cases must pass without real models. Run under a temporary HOME.
- [ ] Commit as `feat(runtime): track model load attempts and terminal states`, with a short body explaining stale-attempt protection.

### Task 2: Authoritative backend readiness and effective configuration

**Files:** Create `engine/runtime_config.cpp`, `tests/test_runtime_config.cpp`, `scripts/test-wrapper-readiness.py`; modify the shared config header, wrapper, model API header/source, server health header/source, `CMakeLists.txt`, `tests/test_model_runtime.cpp`, and `scripts/test-wrapper-context.py`.

**Interfaces:** `RuntimeConfig` holds model name/path/alias, total context, parallel slots, batch, microbatch, GPU layers and draft path. `resolve_runtime_config(...) -> RuntimeConfig` takes registry defaults, validated saved overrides and explicit request/CLI values, preserving that precedence; failures return the existing structured API error shape. `WorkerProbeResult` contains worker state, verified total/per-slot context and slot count or a failure. `probe_model_worker(int port, const RuntimeConfig&, duration timeout) -> WorkerProbeResult` checks the exact alias and props. Replace the bool switch callback with controller submission/status/cancel callbacks using `RuntimeSnapshot`; update all callers together.

- [ ] Extend the fake-worker process test to assert that `/api/models/use` returns `loaded:false`, state `loading` and an attempt ID while the worker is unloaded. Assert exactly one backend `/models/load` call. Fake a loaded worker with wrong alias/context, missing props or malformed JSON; it must never reach ready. Include direct-worker identity/props verification.
- [ ] Run `python3 scripts/test-wrapper-readiness.py --binary /private/tmp/delta-portable-x64/delta-server` against the baseline binary; record failure on its premature `loaded:true`. Keep the fake worker independent of real model downloads.
- [ ] Implement config resolution and GGUF header/bounds validation. Pass context, parallel slots, batch, microbatch and GPU layers explicitly to the worker. Normalize total/per-slot context from actual props before comparison. Validate authoritative exact download metadata only; never compare exact bytes to catalog estimates. Add unit cases for invalid numbers, valid saved overrides, truncated headers and missing exact integrity metadata.
- [ ] Wire the controller to launch/readiness operations. Add `GET /api/models/runtime`: absent attempt ID returns current snapshot or an idle snapshot, supplied unknown/evicted ID returns 404. Add `POST /api/models/runtime/cancel` requiring attempt ID. `/api/models/use` returns HTTP 200 accepted with compatibility fields, attempt/state and `loaded` true only if actually ready. Resource rejection stays 4xx rather than succeeding with an idle router. Preserve existing route access controls and test unauthorized access using configured credentials.
- [ ] Make the backend load the exact router alias once and poll status/props without autoload. Use 30-second engine and 180-second selection deadlines; request timeouts are clamped to remaining time. Keep public HTTP requests responsive during loading and cancellation.
- [ ] Build `delta-server`, `delta-model-runtime-tests`, `delta-runtime-config-tests` and `delta-model-context-tests`; run them and both wrapper context/readiness scripts. Reuse the existing portable build directory; inspect configure flags before reconfiguring to avoid the known repeated libc++ flag accumulation.
- [ ] Commit as `fix(runtime): verify selected worker before reporting readiness`.

### Task 3: Frontend follows the backend attempt

**Files:** Modify frontend service/store/model selector; create `web/app/src/lib/stores/models-runtime.test.ts` and `web/app/src/lib/services/models-runtime.test.ts`.

**Interfaces:** TypeScript `ModelRuntimeSnapshot` mirrors the backend JSON contract. `ModelsService.getRuntime(attemptId?: string): Promise<ModelRuntimeSnapshot>` and `cancelRuntime(attemptId: string): Promise<ModelRuntimeSnapshot>` are the status/cancel methods. `use()` returns the accepted attempt. The store exposes loading/error/current attempt plus retry and cancel methods, preserving existing exports used by components.

- [ ] Write tests: accepted loading disables chat; ready enables it using observed context/slots; failed/timed-out/cancelled stay disabled; same-model late response is ignored; a failed status fetch never means ready. A lost use response calls current-status recovery before permitting another load. Malformed snapshot is surfaced as a connection/protocol error.
- [ ] Run the focused tests with the repository's local Vitest executable/project configuration and record the expected missing-method/behavior failures. Use existing test conventions; do not install dependencies or rewrite the package lock.
- [ ] Implement authoritative polling and plain loading/retry/cancel actions. Remove frontend `triggerModelLoad` from this path. Preserve the selection guard and add attempt identity. Failed reconnect with unknown state leaves chat disabled and exposes explicit retry rather than silently launching.
- [ ] Run focused tests, local Svelte sync/check and `e2e/startup.test.ts` because startup consumes model state. Require no new diagnostics or startup failures.
- [ ] Commit as `fix(ui): follow authoritative model load attempts`.

### Task 4: Own process lifetime and capture final diagnostics

**Files:** Create `engine/owned_process.{h,cpp}`, `tests/test_owned_process.cpp`; modify wrapper, Tauri lib/diagnostics, `scripts/test-wrapper-diagnostics.py`, `CMakeLists.txt`.

**Interfaces:** `OwnedProcess::start(const ProcessSpec&, OutputCallback, ExitCallback) -> void`, `stop() -> void`, `alive() const -> bool`; callbacks include generation and structured exit code/signal. `ProcessSpec` contains argument vector, environment overrides, working directory and generation. POSIX uses an owned process group; Windows uses a job and retained child handle. Readers and handles are owned by this object, not detached callbacks capturing wrapper lifetime.

- [ ] Write process tests asserting an unrelated sentinel survives Delta start/stop; final loader stderr is captured before terminal status; stop with a pipe held open by a descendant is bounded; repeated stop and immediate restart are safe; old output cannot alter the new attempt. Include Windows stderr and job cleanup cases in Windows CI.
- [ ] Run the new tests and existing diagnostics script to establish failures before changing process ownership/filtering.
- [ ] Implement graceful two-second stop, force only owned group/job, child reap, bounded output drain and reader join. Launch through argv/native argument escaping, with paths containing spaces and quotes tested. Replace Tauri's name-wide `pkill`/`taskkill` with retained child ownership and normal backend shutdown. Audit all error and shutdown paths for handle cleanup.
- [ ] Normalize Linux missing-library, ABI-version and symbol errors plus macOS dyld failures before redaction. Keep safe library basenames/ABI versions, stage, generation, model basename and effective configuration. Add Rust allowlist/redaction tests with credentials, private paths and chat-looking lines. Keep report retention limits and manual reporting behavior; use the public issue chooser URL.
- [ ] Run `delta-owned-process-tests`, wrapper diagnostics/readiness scripts and focused Rust diagnostics tests. Do not launch the normal desktop app until the global-kill removal is verified. Run native platform process tests on their corresponding CI hosts before claiming cross-platform behavior.
- [ ] Commit process ownership and diagnostics in separate atomic commits, each with its corresponding tests.

### Task 5: Bounded recovery and local real-model acceptance

**Files:** Create `engine/runtime_failure.cpp`, `tests/test_runtime_failure.cpp`, `scripts/test-runtime-models.py`; modify the shared failure header, controller, wrapper and frontend retry UI; add `docs/releases/2026-10-18-engine-runtime-validation.md` with actual results.

**Interfaces:** `classify_runtime_failure(const ProcessEvidence&) -> RuntimeFailure`; `recovery_action(const RuntimeFailure&, const RuntimeConfig&, unsigned retries_used) -> RecoveryAction`, where actions are stop, retry_cpu, offer_smaller_context, verify_model, or manual_retry. Only confirmed GPU initialization/allocation evidence with GPU layers attempted and zero prior retries permits `retry_cpu`. Explicit user-approved lower-memory retry submits a fresh config through `select()`.

- [ ] Write cases for GPU failure then CPU success, CPU failure with no further retry, RAM failure with no silent config mutation, illegal instruction, missing ABI/symbol/library, invalid GGUF, timeout and unknown exit. Advance fake clock during recovery and assert the original 180-second budget is retained. Verify persisted overrides are unchanged.
- [ ] Run `delta-runtime-failure-tests` before implementation and record expected failure. Implement classification and consolidate existing OOM and automatic-draft retries under the selection budget; do not leave independent restart loops that multiply attempts.
- [ ] Run failure/controller/process/frontend tests and require all to pass. Preserve original sanitized evidence even when a classification is unknown or fallback succeeds.
- [ ] Build the actual modified `delta-server`. Create a reusable stdlib Python probe accepting `--binary`, `--model-dir`, `--model`, `--output`, `--context 2048` and `--rss-limit-mib 2048`; isolate HOME and profiles, select through Delta only, poll attempt status, verify worker props, generate at most 32 tokens, reject missing models, exercise cancellation/retry and clean owned descendants in `finally`. Guard model files as read-only inputs and verify cleanup even on failure.
- [ ] Run sequentially with `--model gemma3:270m` and `--model qwen2.5:0.5b` using the existing installed model directory. Require no manual `/models/load` call, no premature loaded response, actual context 2,048/one slot, nonempty generation and no memory guard trigger. Record binary hashes, timings, effective config and comparison against `docs/releases/2026-10-18-engine-runtime-baseline.md`; do not describe two runs as a reliability percentage.
- [ ] Run existing six engine test executables, context/diagnostics/readiness scripts and affected web tests. Commit recovery, reusable real-model probe and measured validation report as separate cohesive commits.

### Task 6: Final-package Linux acceptance gates

**Files:** Create `scripts/check-packaged-runtime.py`, `scripts/runtime-fixtures.json`, `scripts/test-packaged-runtime.py`; modify `scripts/check-release-runtime.sh`, `.github/workflows/release.yml`, `.github/workflows/ci.yml`.

**Interfaces:** Package checker accepts `--artifact`, `--runtime ubuntu:22.04|ubuntu:24.04`, `--fixtures-manifest` and `--output`. It extracts `.deb`/AppImage without installing on the host, resolves each package's actual engine paths, and calls Task 5's probe in clean Linux runtimes. Manifest entries hold exact immutable revision URL, SHA-256, byte length and expected quantized tensor types. Downloads are checksum-verified and share a total 800 MiB budget.

- [ ] Write checker tests with fake extracted trees: missing binary, wrong library search root, checksum mismatch, oversized download, all-F32 fixture masquerading as Q4/Q8 and a probe failure must each reject the artifact. Inspect final artifacts only; do not substitute staged binaries.
- [ ] Run `python3 scripts/test-packaged-runtime.py` before implementation and record expected failure.
- [ ] Implement extraction/path isolation and fixture verification. Resolve official immutable revisions/checksums for small Q4 and Q8 fixtures, recording exact metadata in the manifest; require intended quantized tensors by reading GGUF tensor headers. Reuse fixture downloads across artifact checks and delete temporary downloads afterward.
- [ ] Add post-packaging release jobs before upload/publish for both `.deb` and final repacked AppImage on Ubuntu 22.04/24.04. Use extracted binaries without build-tree `LD_LIBRARY_PATH`; report dependency resolution from clean runtime. Exercise router selection, context, generation, load error and cleanup through Delta. Retain the existing qemu64 baseline CPU gate and desktop startup tests.
- [ ] Run checker unit tests and `actionlint` locally. Run the real package matrix only where a Linux/container runtime is available; record CI links and results rather than claiming local Linux verification. A failed package gate prevents publication.
- [ ] Commit as `ci: verify final Linux packages load quantized models` with a short body.

## Final acceptance and handoff

- [ ] Self-review the complete diff against each spec section and run a fresh branch review after implementation.
- [ ] Require both local small-model runs and all applicable regression gates; list native platform tests still pending explicitly.
- [ ] Provide the Mint tester a candidate artifact only after authorized build/distribution, request load/generation confirmation or full Copy report, and leave Mint resolution unclaimed until that evidence exists.
- [ ] Report atomic commit list, local results, CI/platform limits and remaining issues. Pushing, PR creation and release publication remain separate authorized steps.
