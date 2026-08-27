#pragma once

/**
 * DHATS - Delta Hardware Acceleration & Telemetry Specification
 * Hardware Monitor Module
 *
 * Provides cross-platform hardware telemetry via Dynamic Telemetry Loading (DTL).
 * Dynamically probes for vendor SDKs (NVML, ROCm SMI, IOKit) at runtime so
 * a single binary works across all hardware configurations without link-time
 * dependencies on vendor libraries.
 */

#include <string>
#include <vector>
#include <mutex>
#include <atomic>
#include <cstdint>

#ifdef _WIN32
#include <windows.h>
#endif

namespace delta {

// ============================================================
// Data Structures
// ============================================================

struct GPUMetrics {
    std::string name;           // e.g. "Apple M2 Pro", "NVIDIA RTX 4090"
    std::string backend;        // "Metal", "CUDA", "Vulkan", "ROCm", "CPU"
    float vram_used_gb  = 0.0f;
    float vram_total_gb = 0.0f;
    int   gpu_util_pct  = 0;
    float temp_c        = 0.0f;
    float power_w       = 0.0f;
};

struct HardwareMetrics {
    float system_ram_used_gb  = 0.0f;
    float system_ram_total_gb = 0.0f;
    float cpu_util_pct        = 0.0f;
    float cpu_temp_c          = 0.0f;
    float system_power_w      = 0.0f;
    std::vector<GPUMetrics> gpus;
    int   rpc_node_count = 0;
    int64_t timestamp_ms = 0;
};

struct OffloadPlan {
    int ngl = 0;               // GPU layers (0 = CPU-only)
    bool all_layers = false;   // true => -ngl 999
    int batch = 512;
    int ubatch = 512;
    float budget_gb = 0.0f;    // GPU-visible memory budget
    float available_gb = 0.0f; // budget - in-use - headroom
    bool cpu_only = false;
};

// ============================================================
// HardwareMonitor Class
// ============================================================

class HardwareMonitor {
public:
    HardwareMonitor();
    ~HardwareMonitor();

    // Non-copyable
    HardwareMonitor(const HardwareMonitor&) = delete;
    HardwareMonitor& operator=(const HardwareMonitor&) = delete;

    /**
     * Poll current hardware metrics. Thread-safe.
     */
    HardwareMetrics get_metrics();

    /**
     * GGUF-aware Auto-Offload: calculate max -ngl given model size and free VRAM.
     * Returns -1 for "all layers" if everything fits, 0 for CPU-only.
     */
    int calculate_auto_ngl(long long model_size_bytes, int n_layers, int ctx_size);

    /**
     * DHATS Brain: GPU memory budget query (Metal recommendedMaxWorkingSetSize on Apple,
     * total VRAM on discrete GPUs). Returns 0 if no GPU detected.
     */
    float gpu_budget_gb() const;

    /**
     * DHATS Brain: Plan optimal offload configuration based on model size,
     * layer count, context size, and current system memory usage.
     * Returns conservative -ngl, batch sizes, and ubatch sizes that won't OOM.
     */
    OffloadPlan plan_offload(long long model_size_bytes, int n_layers, int ctx_size) const;

    /**
     * Returns true if any GPU backend was detected.
     */
    bool has_gpu() const;

    /**
     * Returns the detected backend name for logging.
     */
    std::string get_primary_backend() const;

private:
    // Platform-specific probes (Dynamic Telemetry Loading)
    void probe_nvidia_nvml();
    void probe_apple_metal();
    void probe_amd_rocm();

    // Metric collectors
    float collect_system_ram_used_gb();
    float collect_system_ram_total_gb();
    float collect_cpu_util_pct();
    float collect_cpu_temp_c();
    float collect_system_power_w();

    void collect_nvidia_metrics(std::vector<GPUMetrics>& out);
    void collect_apple_metrics(std::vector<GPUMetrics>& out);
    void collect_amd_metrics(std::vector<GPUMetrics>& out);

    // NVML dynamic function pointers
    void* nvml_lib_handle_ = nullptr;
    bool  nvml_available_  = false;

    // ROCm SMI dynamic function pointers
    void* rocm_lib_handle_ = nullptr;
    bool  rocm_available_  = false;

    // Apple Metal (compile-time, always available on macOS)
    bool  metal_available_ = false;

    // CPU delta tracking state
#ifdef _WIN32
    FILETIME prev_idle_time_, prev_kernel_time_, prev_user_time_;
#elif defined(__APPLE__)
    uint64_t prev_cpu_used_ = 0;
    uint64_t prev_cpu_total_ = 0;
#else
    unsigned long long prev_cpu_used_ = 0;
    unsigned long long prev_cpu_total_ = 0;
#endif

    mutable std::mutex mtx_;
};

} // namespace delta