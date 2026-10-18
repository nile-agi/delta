#include "runtime_failure.h"
#include "runtime_config.h"
#include <iostream>
#include <stdexcept>
using namespace delta;
int main() {
    try {
        auto check = [](bool ok) {
            if (!ok)
                throw std::runtime_error("failure classification or retry budget mismatch");
        };
        RuntimeConfig config;
        config.gpu_layers = 20;
        auto gpu = classify_runtime_failure({"ggml_cuda: cudaMalloc failed: out of memory", 1, 0});
        check(gpu.code == "gpu_memory");
        check(recovery_action(gpu, config, 0) == RecoveryAction::RetryCpu);
        check(recovery_action(gpu, config, 1) == RecoveryAction::Stop);
        config.gpu_layers = 0;
        check(recovery_action(gpu, config, 0) == RecoveryAction::Stop);
        check(classify_runtime_failure({"failed to allocate host memory", 1, 0}).code == "system_memory");
        check(classify_runtime_failure({"model failed", 132, 4}).code == "cpu_compatibility");
        check(classify_runtime_failure({"", static_cast<int>(0xc000001dU), 0}).code == "cpu_compatibility");
        check(classify_runtime_failure({"libstdc++.so.6: version `GLIBCXX_3.4.30' not found", 1, 0}).code ==
              "dependency");
        check(classify_runtime_failure({"symbol lookup error: undefined symbol: ggml_init", 1, 0}).code ==
              "dependency");
        check(classify_runtime_failure({"dyld: Library not loaded: libfoo", 1, 0}).code == "dependency");
        check(classify_runtime_failure({"invalid GGUF magic", 1, 0}).code == "invalid_model");
        check(classify_runtime_failure({"something unfamiliar", 1, 0}).code == "unknown_exit");
        std::cout << "Failure classification and single CPU retry budget checks passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
