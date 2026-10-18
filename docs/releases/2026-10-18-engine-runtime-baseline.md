# Small-model engine runtime baseline

Date: 2026-10-18. Branch: `fix/engine-runtime-readiness`.

## Scope

These runs test the existing engine before implementation of the runtime design. At the time of testing, the branch differs from `dev` only by the design document. Results establish a baseline and reproduce a readiness defect; they do not validate a new runtime controller or resolve the affected Mint installation.

Used the existing portable x86_64 binaries under Rosetta on the local 8 GiB ARM Mac. Each model ran separately in a temporary profile using a symlink to its existing installed file. No downloads were needed. CPU only, two threads, one parallel slot, batch/microbatch 64, requested context 2,048, generation limit 32 tokens, temperature zero, tools disabled. A process-tree memory guard was set at 2 GiB.

The parallel count was also set through `LLAMA_ARG_N_PARALLEL=1`; this experiment does not validate the wrapper's `--parallel` propagation. Timings are single observations, not a performance comparison.

## Results

| Check | Gemma 3 270M Q8 | Qwen 2.5 0.5B Q4 |
| --- | --- | --- |
| Existing model file size | 291,545,600 bytes | 491,400,032 bytes |
| `/api/models/use` reports loaded while worker remains unloaded | Reproduced | Reproduced |
| Explicit `/models/load` reaches loaded worker | Passed | Passed |
| Worker props report context 2,048 and one slot | Passed | Passed |
| Generation through Delta `/v1/chat/completions` | Passed | Passed |
| Missing model returns 404 and leaves router healthy | Passed | Passed |
| Selection request duration | 2.078 seconds | 2.075 seconds |
| Selection through explicit load and loaded-state observation | 3.869 seconds | 5.641 seconds |
| Generation request duration | 6.101 seconds | 12.574 seconds |
| Peak combined observed process RSS | 499.7 MiB | 663.7 MiB |
| Memory guard triggered | No | No |

Read the GGUF headers to verify quantized tensors rather than relying on filenames. Gemma contains 127 Q8_0 tensors and 109 F32 tensors. Qwen contains 12 Q4_K, 12 Q6_K, 133 Q5_0, 13 Q8_0 and 121 F32 tensors.

## Reproduced defect

For both models, `/api/models/use` returned HTTP 200 with `success: true`, `loaded: true` and `message: "Model loaded successfully!"`. An immediate router `/models` query showed the exact selected alias in state `unloaded`. The test had not issued any generation request or explicit load at that point.

The probe then explicitly posted the alias to `/models/load`, observed its loaded state, checked model-specific `/props?model=<alias>&autoload=false`, and requested a short reply through Delta. Both generated nonempty text. This isolates the misleading readiness response from actual model compatibility: these local model files can load and generate with the tested engine.

The existing frontend's separate readiness polling helps protect against this response. Backend clients must not infer worker readiness from that response until the runtime contract is repaired.

## Evidence and cleanup

Raw probe, JSON responses and native logs are retained locally under `/private/tmp/delta-runtime-baseline/`. Temporary profiles were removed after each run. Owned processes were terminated; neither run recorded a surviving descendant requiring separate cleanup. Existing installed model files were opened through symlinks and were not modified by the probe.

Tested binary SHA-256 values:

- `delta-server`: `8340abc557dd17cdf0706ee405c9e4f0288f0da8bc39e7e6e59b9c601a2624c2`
- `llama-server`: `ce4c64637709016a95e78915440ef112e495df8f4540f6c0cc0b9caf9cd0c23e`

## Next acceptance check

After implementing the approved runtime design, repeat this protocol and require the selection response to remain loading until the exact worker is ready. Then exercise stale attempts, cancellation, timeout, owned process cleanup and bounded recovery using deterministic process fixtures. Final Linux packages and the affected Mint machine still require separate execution; these macOS runs provide no Linux package compatibility evidence.
