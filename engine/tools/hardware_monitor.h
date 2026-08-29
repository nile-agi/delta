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
// Rough transformer-layer estimate from GGUF size (used for KV-cache math)
inline int estimate_model_layers(long long size_bytes) {
    if (size_bytes > 10LL * 1024 * 1024 * 1024) return 80;
    if (size_bytes > 5LL  * 1024 * 1024 * 1024) return 48;
    if (size_bytes > 2LL  * 1024 * 1024 * 1024) return 32;
    if (size_bytes > 1LL  * 1024 * 1024 * 1024) return 28;
    return 24;
}

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
    
    // NEW: Multi-tier distribution
    int gpu_layers = 0;        // Layers on GPU
    int cpu_layers = 0;        // Layers on CPU/RAM
    float gpu_mem_needed = 0.0f;  // GB needed on GPU
    float cpu_mem_needed = 0.0f;  // GB needed on CPU/RAM
    bool efficient = true;     // Can run efficiently
    std::string efficiency_warning;  // Why it won't be efficient
    std::string recommendation;      // Suggested alternative
};

struct ResourceHog {
    std::string name;
    int pid;
    float cpu_pct;
    float ram_gb;
    std::string type;  // "browser", "game", "editor", "other"
    std::string suggestion;
};

struct ModelEfficiency {
    std::string model_name;
    std::string display_name;
    long long size_bytes;
    bool can_run;
    bool efficient;
    float gpu_mem_needed;
    float ram_needed;
    std::string warning;
    std::string recommendation;
};

struct ModelCompatibility {
    bool can_run;
    bool efficient;
    std::string warning;
    std::string recommendation;
    int suggested_context;
    float gpu_mem_needed;
    float ram_needed;
    int max_layers_on_gpu;
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
    float gpu_available_gb() const;

    /**
     * DHATS Brain: RAM memory budget query.
     * Returns available RAM in GB (total - used - 2GB reserve for OS).
     */
    float ram_available_gb() const;

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

    /**
     * Intelligent multi-tier offload: distribute model across GPU VRAM + CPU/RAM.
     * Returns optimal split and warns if model won't run efficiently.
     */
    OffloadPlan plan_tiered_offload(long long model_size_bytes, int n_layers, int ctx_size) const;
    
    /**
     * Check if a model can run efficiently on this hardware.
     */
    bool can_run_efficiently(long long model_size_bytes, int n_layers, int ctx_size) const;
    
    /**
     * Get list of recommended models for this hardware.
     */
    std::vector<std::string> get_recommended_model_sizes() const;

    /**
     * Analyze all installed models against current hardware state.
     * Returns which models can run efficiently, which can't, and recommendations.
     */
    std::vector<ModelEfficiency> analyze_installed_models() const;

    /**
     * Get list of resource-heavy processes that could be closed to free resources.
     */
    std::vector<ResourceHog> get_resource_hogs() const;

    /**
     * Get the current top model recommendation based on hardware state.
     */
    ModelEfficiency get_top_model_recommendation() const;

    /**
     * Check if a specific model can run with given context size.
     * @param model_size_bytes Model file size
     * @param n_layers Number of transformer layers in the model
     * @param requested_ctx Requested context window size
     * @return Compatibility assessment with recommendations
     */
    ModelCompatibility check_model_compatibility(
        long long model_size_bytes, 
        int n_layers, 
        int requested_ctx
    ) const;

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

    float estimate_model_ram(long long model_size_bytes) const;
    std::string classify_process(const std::string& name) const;

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