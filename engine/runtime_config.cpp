#include "runtime_config.h"
#include <cstdlib>
#include <fstream>
#include <filesystem>
#include <stdexcept>
#include <cstring>
namespace delta {
int runtime_env_int(const char* name, int fallback) {
    const char* text = std::getenv(name);
    if (!text || !*text)
        return fallback;
    try {
        size_t end = 0;
        int value = std::stoi(text, &end);
        if (text[end] != '\0')
            throw std::invalid_argument("Trailing characters");
        return value;
    } catch (...) {
        throw std::invalid_argument(std::string("Invalid numeric runtime setting: ") + name);
    }
}
void validate_runtime_config(const RuntimeConfig& c) {
    if (c.parallel < 1 || c.parallel > 64 || c.context < 512 || c.context > 16777216 || c.context / c.parallel < 512 ||
        c.context % c.parallel != 0)
        throw std::invalid_argument(
            "Context must provide at least 512 tokens per slot and divide evenly between 1 to 64 slots.");
    if (c.batch < 1 || c.batch > 65536 || c.microbatch < 1 || c.microbatch > c.batch)
        throw std::invalid_argument(
            "Batch sizes must be positive, at most 65536, with microbatch no larger than batch.");
    if (c.gpu_layers < 0 || c.gpu_layers > 999)
        throw std::invalid_argument("GPU layers must be between 0 and 999.");
}
void validate_model_file(const std::string& path, std::optional<uint64_t> exact_size) {
    std::error_code error;
    const auto size = std::filesystem::file_size(path, error);
    if (error || size < 24 || (exact_size && size != *exact_size))
        throw std::invalid_argument(
            "Model file is missing, incomplete or has an unexpected size. Verify the download.");
    std::ifstream input(path, std::ios::binary);
    char magic[4];
    uint32_t version = 0;
    uint64_t tensors = 0, metadata = 0;
    input.read(magic, 4);
    input.read(reinterpret_cast<char*>(&version), 4);
    input.read(reinterpret_cast<char*>(&tensors), 8);
    input.read(reinterpret_cast<char*>(&metadata), 8);
    if (!input || std::memcmp(magic, "GGUF", 4) != 0 || (version != 2 && version != 3) || tensors == 0 ||
        tensors > size / 24 || metadata > size / 12)
        throw std::invalid_argument("Model has an invalid or unsupported GGUF header. Verify the download.");
}
} // namespace delta
