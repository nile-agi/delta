#include "cpu_level.h"

#include <filesystem>

#if defined(__x86_64__) || defined(_M_X64)
#if defined(_MSC_VER)
#include <intrin.h>
#else
#include <cpuid.h>
#endif
#endif

namespace delta {

bool cpu_supports_x86_64_v3() {
#if defined(__x86_64__) || defined(_M_X64)
    unsigned int a = 0, b = 0, c = 0, d = 0;
#if defined(_MSC_VER)
    int r[4];
    __cpuid(r, 0);
    if (r[0] < 7)
        return false;
    __cpuidex(r, 1, 0);
    c = static_cast<unsigned int>(r[2]);
#else
    if (__get_cpuid_max(0, nullptr) < 7 || !__get_cpuid(1, &a, &b, &c, &d))
        return false;
#endif
    const bool fma = c & (1u << 12), osxsave = c & (1u << 27), avx = c & (1u << 28), f16c = c & (1u << 29);
    if (!(fma && osxsave && avx && f16c))
        return false;
    // The OS must save the AVX (YMM) registers, or AVX instructions fault.
#if defined(_MSC_VER)
    const unsigned long long xcr0 = _xgetbv(0);
#else
    unsigned int lo = 0, hi = 0;
    __asm__ volatile("xgetbv" : "=a"(lo), "=d"(hi) : "c"(0));
    const unsigned long long xcr0 = (static_cast<unsigned long long>(hi) << 32) | lo;
#endif
    if ((xcr0 & 6) != 6)
        return false;
#if defined(_MSC_VER)
    __cpuidex(r, 7, 0);
    b = static_cast<unsigned int>(r[1]);
#else
    __cpuid_count(7, 0, a, b, c, d);
#endif
    const bool avx2 = b & (1u << 5), bmi2 = b & (1u << 8);
    return avx2 && bmi2;
#else
    return false;
#endif
}

bool is_v3_llama_server(const std::string& file_name) {
    return file_name.rfind("llama-server-v3", 0) == 0;
}

std::string fastest_llama_server(const std::string& llama_server, bool cpu_has_v3) {
    if (!cpu_has_v3 || llama_server.empty())
        return llama_server;
    namespace fs = std::filesystem;
    const fs::path path(llama_server);
    const std::string name = path.filename().string();
    const std::string prefix = "llama-server";
    if (name.rfind(prefix, 0) != 0 || is_v3_llama_server(name))
        return llama_server;
    // llama-server[.exe] -> llama-server-v3[.exe]; Tauri's dev names keep their target-triple suffix.
    const fs::path v3 = path.parent_path() / (prefix + "-v3" + name.substr(prefix.size()));
    std::error_code ec;
    return fs::is_regular_file(v3, ec) ? v3.string() : llama_server;
}

} // namespace delta
