#include "runtime_failure.h"
#include <algorithm>
#include <cctype>
namespace delta {
RuntimeFailure classify_runtime_failure(const ProcessEvidence& evidence) {
    std::string text = evidence.errors;
    std::transform(text.begin(), text.end(), text.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    auto has = [&](const char* part) { return text.find(part) != std::string::npos; };
    RuntimeFailure result{"unknown_exit", "Model process exited. Copy the report for details.", evidence.exit_code,
                          evidence.signal};
    if (evidence.signal == 4 || static_cast<uint32_t>(evidence.exit_code) == 0xc000001dU) {
        result.code = "cpu_compatibility";
        result.message =
            "This engine used an instruction your CPU cannot run. Copy the report and report this release.";
        result.action = "report";
    } else if (has("error while loading shared libraries") || has("symbol lookup error") ||
               (has("version ") && has("not found")) || has("dyld:")) {
        result.code = "dependency";
        result.message =
            "The engine could not load a required library or symbol. Copy the report for the dependency details.";
        result.action = "report";
    } else if ((has("cuda") || has("metal") || has("vulkan") || has("hip") || has("gpu")) &&
               (has("out of memory") || has("allocation failed") || has("cudamalloc") || has("failed to allocate") ||
                has("outofdevicememory"))) {
        result.code = "gpu_memory";
        result.message = "The GPU could not allocate enough memory for this model.";
    } else if ((has("cuda") || has("metal") || has("vulkan") || has("hip")) &&
               (has("failed to initialize") || has("initialization error") || has("no devices"))) {
        result.code = "gpu_initialization";
        result.message = "The GPU backend could not initialize.";
    } else if (has("failed to allocate host memory") || has("std::bad_alloc") || has("cannot allocate memory") ||
               has("insufficient system memory")) {
        result.code = "system_memory";
        result.message = "There is not enough system memory. Close other apps or try a smaller context or model.";
        result.action = "smaller_context";
    } else if ((has("gguf") && (has("invalid") || has("truncated") || has("unsupported"))) ||
               has("unknown model architecture")) {
        result.code = "invalid_model";
        result.message =
            "The model file is incomplete, invalid or unsupported. Verify its download before trying again.";
        result.action = "verify_model";
    }
    return result;
}
RecoveryAction recovery_action(const RuntimeFailure& failure, const RuntimeConfig& config, unsigned retries) {
    return retries == 0 && config.gpu_layers > 0 &&
                   (failure.code == "gpu_memory" || failure.code == "gpu_initialization")
               ? RecoveryAction::RetryCpu
               : RecoveryAction::Stop;
}
} // namespace delta
