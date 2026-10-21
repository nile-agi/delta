#ifndef DELTA_CPU_LEVEL_H
#define DELTA_CPU_LEVEL_H

#include <string>

namespace delta {

// x86-64-v3: AVX2, FMA, F16C and BMI2, with the OS saving AVX state. False on other architectures.
bool cpu_supports_x86_64_v3();

// x86_64 releases ship llama-server (any CPU) and llama-server-v3 (AVX2, several times faster).
// Returns the v3 sibling of `llama_server` when it exists and this CPU can run it, else `llama_server`.
std::string fastest_llama_server(const std::string& llama_server, bool cpu_has_v3 = cpu_supports_x86_64_v3());

// True for a file name that is the AVX2 build, which must never be picked as the baseline.
bool is_v3_llama_server(const std::string& file_name);

} // namespace delta

#endif // DELTA_CPU_LEVEL_H
