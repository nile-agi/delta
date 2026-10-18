# Engine runtime readiness and recovery

Date: 2026-10-18

Branch: `fix/engine-runtime-readiness`, based on `dev` at `a7b2380`.

Status: Proposed design for review. Product implementation has not started.

## Goal and constraints

Make packaged Delta start and load models predictably, explain failures with usable evidence, and verify the engine shipped to users. The reported Mint regression is the motivating case: an older release candidate worked, while later packages failed. There is no recent complete report from the affected machine, so this design does not identify its current root cause.

Success means a selected model becomes usable only after its worker is ready, load attempts end in an observable state, recovery is bounded, and reports preserve actionable failure details. Testing must include final Linux packages and quantized weights. Acceptance on the affected Mint machine remains necessary.

Keep the existing C++ engine, llama.cpp router, Tauri host and Svelte frontend. Preserve explicit user overrides and existing model files. Conserve local disk and memory; remove any model downloaded specifically for testing afterward. This work excludes agent tool behavior, general chat timeouts, model quality and a new orchestration framework.

## Evidence and alternatives

Current source behavior:

- `engine/delta_server_wrapper.cpp` considers generic `/health` sufficient for readiness. Router health does not establish that a selected worker is loaded. A resource rejection can also fall back to a healthy idle router.
- `engine/model_api_server.cpp` uses that result for `/api/models/use` and its successful-load message.
- `web/app/src/lib/stores/models.svelte.ts` already polls per-model readiness and triggers loading through `ModelsService`; retain this protection while moving lifecycle ownership into the backend. Its selected-model ID guard cannot distinguish two attempts for the same model.
- The wrapper stores `max_parallel_` without using it in command construction. Preflight, requested context, worker context and parallel slots need one effective configuration.
- Tauri startup kills `llama-server` and `delta-server` by process name. That can terminate unrelated servers.
- POSIX child stderr passes through keyword filters before native reporting. Loader messages about missing ABI versions or symbols can be lost. Windows child stderr capture differs from POSIX.
- Release runtime checks exercise staged sidecars with a small F32 fixture. They do not establish that extracted final packages load quantized models through Delta's selection API.

The earlier release investigation found AVX-512 instructions in the failing v1.0.17 Linux binary, absent from the working candidate and later portable build. That supports the original CPU compatibility regression; it does not explain every later Mint failure.

Options considered:

1. Add diagnostics alone: smaller change, but success semantics and recovery races remain.
2. Revert to the working candidate: useful as an operational comparison, but discards subsequent fixes and cannot establish the latest failure cause.
3. Repair lifecycle ownership inside the existing engine: recommended. It provides one readiness decision and bounded recovery without replacing the runtime.

## Runtime ownership and states

Introduce a focused engine runtime controller used by the wrapper and model API. It owns the child handles, resolved launch configuration, current selection attempt, state transitions and terminal failure. HTTP handlers only submit operations or read snapshots; they must not wait for a full model load while holding request or state locks.

Track engine and selected-model states separately:

| Scope | States |
| --- | --- |
| Engine | stopped, starting, available, failed |
| Selection attempt | loading, ready, failed, cancelled, timed_out |

An available engine can have no selected model. Each selection receives an opaque attempt ID and an internal monotonically increasing generation. Switching models, retrying the same model, changing runtime configuration or cancelling invalidates the previous generation before cleanup begins. Callbacks, child output and polling results carry that generation; stale results cannot alter the current state.

Serialize worker mutations through the controller. A newer selection cancels the older load, waits for owned-worker cleanup, then starts. Cancellation is idempotent. Retain terminal attempt snapshots in a bounded in-memory collection of the most recent 16 attempts; unknown or evicted IDs return 404.

## Readiness contract and API

Preserve existing `/api/models/use` request fields and response compatibility fields. Return HTTP 200 for an accepted selection, with `success: true`, `attempt_id`, `state`, `model_name`, `model_alias`, `ctx_size`, and `loaded`. `success` means accepted; `loaded` is true only for state `ready`. Use a loading message until readiness is established. Invalid input or preflight rejection returns a suitable 4xx response with a structured failure; never substitute an idle-router success for the requested load.

Add `GET /api/models/runtime?attempt_id=...` for the attempt snapshot and `POST /api/models/runtime/cancel` with `attempt_id`. Cancellation of an old attempt cannot cancel a newer one. Status includes effective configuration and a safe structured error where relevant. Apply the existing local API access controls to these routes.

The controller establishes readiness in this order:

1. Confirm the owned engine is alive and available.
2. Resolve the exact router alias and call supported `POST /models/load` with `{ "model": alias }` once per execution attempt.
3. Poll the exact model's router state until loaded, failed or deadline. Do not accept another model's status or a non-router list as unconditional proof.
4. Fetch the selected worker's `/props?model=<encoded alias>&autoload=false`, checking the worker's reported context and slot configuration against the resolved launch configuration. Normalize the worker's total/per-slot context fields before comparison.
5. Publish `ready` only if the generation is still current and the child remains alive. A configuration mismatch is a structured failure, not permission to enable chat.

For a direct `-m` worker, verify its health, model identity and props through the same readiness interface. Avoid generating a warm-up answer solely to declare readiness; real generation belongs in integration tests.

The frontend consumes the authoritative attempt snapshot, retains its stale-selection guard and adds attempt identity. Remove frontend load triggering once the backend owns it; avoid two independent loaders. Populate slot/context displays from verified worker configuration. Show loading, cancel and retry states in plain language. A failed status request is a connection problem, never evidence that the model is ready.

The engine startup deadline is 30 seconds and the model-load deadline is 180 seconds, both measured by a monotonic clock. Each status probe has a bounded request timeout of at most two seconds and never extends the deadline. A selection has one 180-second budget including any automatic recovery. Deadline expiry invalidates the attempt, initiates owned-worker cleanup and returns `timed_out`; a late success cannot change it. A manual retry creates a new attempt and deadline. These deadlines do not change chat generation timeouts.

## Resolve configuration once

Create one effective configuration before preflight: model identity/path, total context, parallel slots, batch and microbatch sizes, GPU layers and optional draft model. Feed the same values to resource estimation, launch arguments and reporting.

Keep current context caps and valid saved overrides. Validate numeric values using the engine's supported ranges. Pass the resolved parallel count explicitly instead of storing an unused value. Treat total and per-slot context consistently with the vendored server; do not display a requested value as an observed value.

Retain the portable CPU release policy. Validate GGUF magic, supported header/version and basic bounds before launch. Check exact file size or hash only when recorded authoritative download metadata provides it; catalog sizes are estimates. Preserve the existing temporary-download-and-rename flow, adding validation before promotion where exact metadata exists. Do not delete or redownload an existing user file automatically.

## Failure classification and recovery

Preserve original stage, exit code/signal and sanitized stderr alongside the classification. Prefer explicit process evidence and recognized native errors over broad text guesses. Unknown failures stay unknown.

| Evidence | Action |
| --- | --- |
| GPU initialization/allocation failure, GPU layers were attempted | At most one automatic CPU-only retry within the same selection deadline; report fallback configuration |
| System RAM allocation failure | Stop and offer an explicit smaller-memory retry; do not silently overwrite saved settings |
| Missing library, symbol or ABI version | Stop with dependency details; no repeat launch with the same configuration |
| Illegal-instruction signal or equivalent process evidence | Stop with CPU compatibility details; no same-binary retry |
| Invalid, truncated or unsupported model | Stop with verify/redownload guidance; preserve the file |
| Deadline reached | Stop the load and offer retry/cancel guidance |
| Unknown exit | Stop with diagnostic report controls |

Consolidate existing OOM auto-heal behavior under this retry budget so it cannot multiply attempts. An automatic CPU fallback changes only this attempt's effective configuration. Persisted overrides remain unchanged. A user-approved lower-memory retry creates a new attempt and displays its proposed context/parallel changes before submission.

## Process ownership and diagnostics

Replace process-name-wide startup cleanup with child ownership. Tauri owns its Delta backend handle; the backend owns the router and worker process group or Windows job. Shutdown, retry and cancellation terminate only those owned processes. Use graceful termination with a two-second bound followed by forced termination of the owned group/job. Reap children and close handles. Avoid PID-only persistence across app restarts because PID reuse cannot establish ownership.

Replace detached readers capturing wrapper lifetime with owned readers that are drained and joined during teardown. Keep critical process output available before publishing a terminal report, with a bounded drain so descendants holding pipes cannot stall shutdown. Windows receives equivalent stderr pipe capture. Attribute all output and exit events to their runtime generation.

Recognize loader errors at the first capture boundary, including Linux shared-library, symbol lookup and ABI version failures, and macOS dyld failures. Remove reliance on broad `error/fail/warning` filtering for these messages. Continue bounded, allowlisted reporting and redaction in Rust; normalize safe library basenames and ABI versions before redacting private paths.

Reports include app/tag/commit/target, OS, CPU policy/features, stage, model basename, effective runtime configuration, attempt state, exit/signal and sanitized recent native errors. Exclude chat, documents, secrets and full private paths. Existing Copy/Download report controls remain available. Public issue reporting opens `https://github.com/nile-agi/delta-release/issues/new/choose`; submission and attachments remain user-controlled, with no automatic upload.

## Verification and release gates

Use deterministic process fixtures first, then real packaged binaries:

- Unit tests cover transitions, stale same-model attempts, cancellation, monotonic deadlines, error classification and the single-retry budget.
- Process integration tests cover healthy idle router with unloaded worker, rejected load, wrong alias/context, child exit, CPU fallback, stderr draining and preservation of an unrelated running process.
- Frontend tests cover loading/ready/failure, connection loss, retry/cancel and stale attempt responses; use actual verified context in the ready view.
- Diagnostics tests cover Linux missing-library/version/symbol errors, macOS dyld errors and Windows process exits, including path/secret redaction.
- Extend release checks to extract final `.deb` and AppImage artifacts and execute the extracted Delta and llama binaries in clean Ubuntu 22.04 and 24.04 runtimes. Check dynamic dependencies from those extracted locations without build-tree library paths. Headless extraction is sufficient for the engine gate; retain desktop startup tests separately.
- Through Delta's selection API, load immutable small Q4 and Q8 fixtures, assert their tensors actually use the intended quantized types, verify selected-worker readiness and effective context, then generate tokens. Also exercise failed-load reporting and process cleanup. Pin fixture revisions and checksums and enforce a small download budget; never accept an all-F32 fixture as quantized coverage.
- Keep the existing baseline-CPU execution gate. A portable ISA check and a quantized model API smoke test are complementary gates.
- Before declaring the Mint regression resolved, run the candidate package on the affected Mint machine and collect the full report on failure. Local macOS checks and Linux CI alone cannot establish that result.

No Linux runtime is currently available locally. Linux package execution must be evidenced in CI or on a Linux test host; report this limit explicitly during implementation review.

## Delivery boundaries

Implement in reviewable stages: controller/readiness and effective config; frontend integration; owned process teardown and diagnostics; bounded recovery; final-package release gates. Keep commits atomic with a one- or two-line body and no generated/co-author attribution. Each stage needs focused regression evidence before committing. Do not publish a release or claim the reported device fixed based solely on this design.

The next artifact, after approval of this written spec, is a concrete implementation plan with file boundaries, tests and execution order.
