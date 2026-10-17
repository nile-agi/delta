# MiniCPM harness evaluation — 17 October 2026

## Findings and changes

Two independent defects were confirmed:

- Large tool schemas were included even when the model did not need tools. The harness now starts with category discovery when the initial schemas exceed the smaller of 1,024 tokens or a quarter of the context window. Loaded categories remain available; discovery choices reflect permissions and attached documents. Compact instructions preserve history and distinguish discovery from executing a task.
- A missing registry context limit became the model's training context. In addition, the wrapper's router reuse shortcut ignored context changes: the API reported 8,192 while the actual worker used 131,072. Registered models without a declared limit now default to 8,192; saved overrides still apply. Resource checks and launch use the same limit, and changing it restarts the router with the new arguments.

These changes improve the tested greeting path. They do **not** establish that agent tool use is reliable across models or that the Mint model-loading crash is fixed.

## Method

- Mac14,7, ARM host, 8 GiB RAM; portable x86_64 backend running under Rosetta. These are not native Linux or Windows performance measurements.
- Official OpenBMB Q4_K_M weights, pinned revisions and verified SHA-256 hashes; one loaded model at a time.
- Two CPU threads, GPU layers zero, one worker, batch/ubatch 64, 8,192-token context, temperature zero and thinking disabled. No documents, file tools or shell tools.
- Separate temporary profiles; no installed user models or conversations changed. Each variant received a plain warm-up before the greeting. Greeting output budget was 2,048 tokens; follow-ups used 256. Run budgets were five iterations for 1B and three for 2B, so follow-up durations are descriptive rather than a controlled model ranking.
- Previous and fixed backends used the same weights. The old backend received an explicit `-c 8192`; the fixed backend received model selection without a context override. Its actual worker context was verified through `/props`, rather than trusting the model-selection response alone.
- Combined process RSS was monitored with a 2.5 GiB stop limit. Neither model exceeded it.

The isolated 1B registry entry used a Q6-named symlink to the official Q4 weights. The actual quantization remained Q4_K_M; this did not change the installed registry or user model files.

## Results

| Model | Previous tools-enabled greeting | Fixed first greeting text | Fixed processed prompt tokens | Fixed peak combined RSS |
| --- | --- | --- | ---: | ---: |
| MiniCPM5 1B Q4_K_M | HTTP timeout at 126.8 s | 20.3 s | 696 | 904.5 MiB |
| MiniCPM5 2B Q4_K_M | HTTP timeout at 127.0 s | 62.7 s | 696 | 1,695.4 MiB |

Both fixed workers used 8,192 tokens. A prior successful 1B run with the full tool set processed 3,246 prompt tokens for the same greeting; the timeout runs above did not complete and therefore have no final prompt-usage count.

Follow-up requests tested conversation facts (project **Cedar**, deadline **Friday**) and a read-only calendar search for 1–2 March 2027 against an empty test database:

| Model | History follow-up | Calendar follow-up |
| --- | --- | --- |
| 1B | Eventually correct, 234.8 s. Unnecessary discovery and memory calls, including an invented `project` group that the harness rejected. | Discovery and `list_events` succeeded, 200.2 s total. Final text repeated the planned action and reached its output limit; this is not an acceptable final-answer quality result. |
| 2B | Correct direct answer, 63.4 s total, no tool calls. | HTTP timeout at 126.7 s before any tool invocation executed. **Failed.** |

Neither history request emitted a compaction event. This probe provides no evidence that over-compaction caused its history failures. The 1B model eventually used the supplied facts but did not consistently follow the instructions to answer from conversation history.

An earlier 1B draft discovered calendar tools but invented events without searching. The final compact guidance caused the subsequent probe to execute the real search, but did not eliminate repetition. One run per case at temperature zero cannot establish a model reliability percentage.

## Verification and remaining investigation

- All six CI engine test executables passed: 548 harness checks, 53 document checks, 35 update checks, 51 OpenAI compatibility checks, 36 OCR checks, and the model-context regression executable.
- A fake-worker process test first reproduced API context 8,192 versus actual child context 131,072. It now verifies initial selection, switching to 4,096, reusing the saved override, and retaining router mode. The existing child-error diagnostics test also passed.
- Workflow validation passed with `actionlint`.
- The 120-second low-data timeout was not increased. The remaining 2B calendar failure needs prefill/generation progress captured at timeout before attributing it to schema size, inference speed or a stalled backend.
- The affected Mint/Windows machines still need verification. No local backend crash or model-loading exit was reproduced here.

All downloaded weights, temporary profiles and test processes were removed after testing. Small raw reports and probe scripts were retained under `/private/tmp/delta-model-comparison` for this session.

## Weight provenance

| Official repository | Revision | File bytes | SHA-256 |
| --- | --- | ---: | --- |
| [OpenBMB MiniCPM5 1B GGUF](https://huggingface.co/openbmb/MiniCPM5-1B-GGUF) | `3d55fac80935ae6456986ad2384b5cbcc4d6c948` | 688,065,920 | `81b64d05a23b17b34c475f42b3e72fbde62d4b92cc34541f7a8031d0752deafa` |
| [OpenBMB MiniCPM5 2B GGUF](https://huggingface.co/openbmb/MiniCPM5-2B-GGUF) | `2079a22f3beaa4e306449978533478fe0522f4b3` | 1,561,318,368 | `ec2d5801640099e97d8d7e8003ad4d81f336e757811f03a26173dddf386602fd` |
