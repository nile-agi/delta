# Local engine runtime validation

Date: 2026-10-18. Branch: `fix/engine-runtime-readiness`.

## Real-model acceptance

The rebuilt portable x86_64 engine ran under Rosetta on the local 8 GiB ARM Mac. Both models were already installed; no weights were downloaded or changed. Each run used a separate temporary HOME, CPU only, two threads, one slot, context 2,048 and batch/microbatch 64. The process-tree memory guard was 2 GiB.

| Check | Gemma 3 270M Q8 | Qwen 2.5 0.5B Q4 |
| --- | --- | --- |
| Selection reports loading until verified readiness | Passed | Passed |
| Backend loads the selected worker without a manual load request | Passed | Passed |
| Cancel then retry produces a fresh attempt | Passed | Passed |
| Actual worker context 2,048 and one slot | Passed | Passed |
| Nonempty generation through Delta API | Passed | Passed |
| Invalid selection leaves engine usable | Passed | Passed |
| Model file size/mtime preserved | Passed | Passed |
| Owned processes cleaned up | Passed | Passed |
| Selection through readiness | 2.253 seconds | 4.104 seconds |
| Generation request duration | 2.769 seconds | 14.568 seconds |
| Peak combined observed RSS | 524.3 MiB | 658.3 MiB |

Tested `delta-server` SHA-256: `1dffdca9c0c419ba8a5c67f12133bb9e57b7871021203df8afde5672de43a412`.

The earlier [baseline](2026-10-18-engine-runtime-baseline.md) reported a loaded model while its worker remained unloaded, on both models. These runs require `loaded:false` on acceptance, then obtain readiness through the authoritative attempt status. There is no explicit `/models/load` call in the real-model probe.

Timings are single observations with different cache conditions. They establish functional acceptance, not a speed comparison or reliability percentage. Native ARM acceleration, Windows, Linux and final installers were not exercised by these local model runs.

## Reproduce locally

Run sequentially with the newly built binary, replacing paths for the local installation:

```sh
python3 scripts/test-runtime-models.py --binary /path/to/delta-server \
  --model-dir /path/to/models --model gemma3:270m --output /tmp/gemma-runtime.json
python3 scripts/test-runtime-models.py --binary /path/to/delta-server \
  --model-dir /path/to/models --model qwen2.5:0.5b --output /tmp/qwen-runtime.json
```

The script creates temporary profiles and symlinks to the supplied weights, monitors process memory, and cleans its owned processes/profile on completion or failure. Raw results and native logs from this session are under `/private/tmp/delta-runtime-baseline/{gemma,qwen}-fixed.{json,log}`.

## Deterministic regression coverage

- Controller tests cover fresh/stale attempts, bounded retention, cancellation, post-ready failure, one CPU fallback, and an unchanged deadline/settings during recovery.
- Process tests cover unrelated-process isolation, argument quoting without a shell, final stderr, exit status and bounded cleanup when another process retains the pipe.
- Wrapper process tests cover exact alias/context/slots, malformed props, missing attempt IDs, confirmed GPU failure followed by one CPU retry, failed CPU fallback and temporary context changes that preserve saved overrides.
- Native diagnostics/startup suite: 18 tests passed, including ABI/symbol preservation, private-path redaction, generation/exit correlation and unloading before backend termination. The standalone startup suite also passes all five tests without external crate dependencies.
- Frontend server suite: 187 tests passed across 27 files. Svelte check had zero errors and seven existing warnings. Browser/simulated desktop startup suite: eight tests passed.

## Missing Iterator during desktop startup

The modern PDF.js bundle referenced `Iterator.prototype` while being imported through the application's PDF helper. On a WebKit runtime without the global `Iterator`, this prevented the app from starting before the user opened a PDF. Iterator helpers were added in [Safari 18.4](https://webkit.org/blog/16574/webkit-features-in-safari-18-4/); the reported macOS 15.3.1 failure is consistent with that compatibility gap.

The helper now imports PDF.js's maintained legacy build and its matching legacy worker. A production-startup regression removes `globalThis.Iterator` before initialization in both browser and simulated desktop modes; these failed before the import change and now pass. Three browser tests also pass real PDF extraction and rendering checks. These tests exercise compatibility behavior in Chromium, not the affected Mac's actual WebKit installation; native Intel/M2 confirmation remains required.

## Release acceptance still required

Run the final-package Linux gates in CI and collect confirmation or a complete failure report from the affected Mint machine. The local results do not establish that the reported Mint regression is resolved. Docker became available for the subsequent [Fedora and Ubuntu container investigation](2026-10-18-linux-container-validation.md); its engine results are recorded separately from the macOS runs above.
