#pragma once
#include <string>
#include <optional>
#include <cstdint>
namespace delta {
struct RuntimeConfig {
    uint64_t generation = 0;
    std::string model_name, model_path, model_alias, draft_path;
    int context = 8192;
    int parallel = 1;
    int batch = 512;
    int microbatch = 128;
    int gpu_layers = 0;
};
void validate_runtime_config(const RuntimeConfig&);
void validate_model_file(const std::string&, std::optional<uint64_t> exact_size = std::nullopt);
int runtime_env_int(const char* name, int fallback);
} // namespace delta
