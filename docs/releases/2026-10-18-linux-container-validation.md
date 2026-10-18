# Linux container runtime investigation

Date: 2026-10-18. Branch: `fix/engine-runtime-readiness`.

## Environments

The official `fedora:44` and `ubuntu:24.04` images ran as `linux/amd64` under OrbStack on the local ARM Mac. Ubuntu 24.04 represents Mint 22's distribution base; it is not a complete Mint desktop installation. Tests ran sequentially with CPU inference, two threads, one slot, context 2,048 and batch/microbatch 64. Existing model files were mounted read-only. No model weights were downloaded.

Image digests:

- Fedora 44: `sha256:43b29f65a41eb9c35e1cd5323e3bdf3b655c2357a9f4f1ff2f9c2798e5045d80`.
- Ubuntu 24.04: `sha256:68ee22004a9109b44973313b02ff7b1de250f1de0f5ecb628d73f579aaff33ac`.
- Mint 22.3 CI image: `sha256:304bbf7ec2a61252962c5210467d231a141661490e8962f52fd4df7688370369`.

The additional `linuxmintd/mint22.3-amd64:latest` image is used by [Linux Mint's own build workflow](https://github.com/linuxmint/github-actions/blob/master/.github/workflows/do-builds.yml). Inspection showed Ubuntu 24.04 userspace, Mint Zena repositories and `mint-dev-tools` 1.5.4. It is a Mint build environment, not a full Mint desktop. Its approximately 3 GB image and test container were removed after testing to recover space; reports were retained.

The VM has approximately 4 GiB RAM. The Fedora container was capped at 3 GiB and the Ubuntu container at 2 GiB; model tests were not run concurrently. The test process-tree memory guard was 2 GiB.

## Released engine baseline

The engine binaries came from the downloaded `Delta_1.0.18_amd64.deb` release candidate artifact. `llama-server --version` reports `0.4.0-dev`, commit `f3f1a8f`, built with GNU 11.4.0 for Linux x86_64. Both binaries resolve their shared-library dependencies in Fedora 44. Fedora emits a libcurl version-information warning; it did not prevent these requests from succeeding.

Released binary SHA-256 values:

- `delta-server`: `5157f4a70b47f088a5e8cfb3038bfac38cf564c6e637fb9bed996d85a72573c8`.
- `llama-server`: `ad0493207cc0d6fd225817160f8f1da4c1413ce1bd5f90229b4d06f23654344b`.

| Runtime | Model | Selection claimed loaded while worker was unloaded | Generation after explicit worker load | Peak process-tree RSS |
| --- | --- | --- | --- | --- |
| Ubuntu 24.04 | Gemma 3 270M Q8 | Reproduced | Passed | 425.4 MiB |
| Ubuntu 24.04 | Qwen 2.5 0.5B Q4 | Reproduced | Passed | 616.4 MiB |
| Fedora 44 | Gemma 3 270M Q8 | Reproduced | Passed | 416.3 MiB |
| Fedora 44 | Qwen 2.5 0.5B Q4 | Reproduced | Passed | 615.8 MiB |
| Mint 22.3 CI image | Gemma 3 270M Q8 | Reproduced | Passed | 425.2 MiB |
| Mint 22.3 CI image | Qwen 2.5 0.5B Q4 | Reproduced | Passed | 618.4 MiB |

All six runs verified the actual worker context, rejected an unknown model, and kept the router healthy after that rejection. Nonempty replies contained zero usage counts, independently reproducing the statistics problem. No model-load crash or signal 9 occurred in these runs.

The baseline probe deliberately calls `/models/load` after capturing the mismatch to determine whether the worker can actually run. Consequently, its successful generation is not evidence that the release's ordinary model-selection flow works. The candidate's acceptance probe must generate without that manual request.

## Modified engine in Fedora

The current source built successfully in Fedora 44 with GCC 16.2, the portable x86-64 CPU policy and local `-O1 -DNDEBUG` flags to limit the emulated build cost. Production optimization settings were not changed. This Fedora-built binary is a local test artifact, not a replacement for the release pipeline's Ubuntu-baseline binary.

Four test binaries passed: model runtime, numeric/file validation, failure classification and owned-process cleanup. All three wrapper suites passed: readiness, diagnostics and context changes.

| Model / worker | Cancel/retry and verified readiness | Generation | Peak RSS |
| --- | --- | --- | --- |
| Gemma 270M / locally built worker | Passed | Passed (21.292 s) | 418.1 MiB |
| Qwen 0.5B / locally built worker | Passed | Exceeded the probe's 90 s request limit | 609.9 MiB |
| Qwen 0.5B / optimized release worker | Passed | Passed (59.360 s) | 613.3 MiB |

Both successful runs passed all seven acceptance checks, including generation without a manual `/models/load`, actual context/slots, invalid-selection isolation, source-file preservation and process cleanup. The timed-out run also preserved the model file and cleaned its live processes. Its failure is retained in the report rather than counted as a successful generation.

The controlled Qwen comparison used the identical modified Delta executable and changed only which worker binary it launched. This places the observed generation slowdown in the worker-build/emulation comparison, rather than the readiness controller. It does not isolate compiler optimization from the worker's other build differences, and does not establish native performance.

Candidate `delta-server` SHA-256: `4f08d980beb1b51997474b29e0c1f88592a56bab19a19f373ef38274bfb83caa`. Candidate model reports are `fedora-candidate-{gemma,qwen}.json` and `fedora-candidate-release-worker-qwen.json` in the local evidence directory.

## Keeping CI cost bounded

Ordinary PR checks do not run the Docker distro matrix. The existing Linux release job checks the final Debian package and AppImage in Ubuntu 22.04 by default; it reuses one model-fixture download across the two checks. No additional build jobs were added for these distributions.

For a local investigation, explicitly select a wider matrix when invoking the release wrapper:

```sh
DELTA_VERIFY_PACKAGED_RUNTIME=1 \
DELTA_RUNTIME_IMAGES='ubuntu:22.04 ubuntu:24.04 fedora:44 linuxmintd/mint22.3-amd64:latest' \
scripts/tauri-release-build.sh build --target x86_64-unknown-linux-gnu
```

This opt-in runs both package types on Ubuntu and Mint, and only the AppImage on Fedora. An individual existing artifact can instead be checked with `scripts/check-packaged-runtime.py --help`. Package-gate tests assert the small default matrix, the optional wider matrix, and failure propagation before upload. The checker installs its process-monitor dependency, explicitly uses `linux/amd64`, and retains a diagnostic report on dependency failures.

## Limits and evidence

These containers exercise the Linux engine and its dependencies. They do not exercise the installed desktop's WebKit rendering, GPU drivers, display server, SELinux host policy, or the affected user's RAM and swap pressure. CPU emulation also makes timings unsuitable for native performance comparisons. A successful container run cannot explain an external SIGKILL or establish that the original Fedora/Mint incident is resolved.

Local raw reports, environment records, binary hashes and logs are under `/private/tmp/delta-fedora-runtime/reports`. Temporary profiles are isolated from the user's Delta configuration. The modified engine build is tested separately from this release baseline; final AppImage and Debian-package gates remain required for release acceptance.
