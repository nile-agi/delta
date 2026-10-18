#pragma once
#include <string>
namespace delta {
struct RuntimeConfig {
    std::string model_name, model_path, model_alias, draft_path;
    int context = 8192;
    int parallel = 1;
    int batch = 512;
    int microbatch = 128;
    int gpu_layers = 0;
};
} // namespace delta
