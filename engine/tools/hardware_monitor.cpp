/**
 * DHATS - Delta Hardware Acceleration & Telemetry Specification
 * Hardware Monitor Implementation
 *
 * Uses Dynamic Telemetry Loading (DTL): vendor SDKs are loaded at runtime
 * via dlopen/LoadLibrary. If a vendor SDK is not present, the probe fails
 * gracefully and the system falls back to CPU-only metrics.
 */

#include "hardware_monitor.h"
#include <iostream>
#include <cstring>
#include <chrono>
#include <cstdio>

// ============================================================
// Platform Headers
// ============================================================

#ifdef _WIN32
  #include <windows.h>
  #define DELTA_DLOPEN(lib)     LoadLibraryA(lib)
  #define DELTA_DLSYM(h, sym)   GetProcAddress((HMODULE)h, sym)
  #define DELTA_DLCLOSE(h)      FreeLibrary((HMODULE)h)
#else
  #include <dlfcn.h>
  #define DELTA_DLOPEN(lib)     dlopen(lib, RTLD_LAZY)
  #define DELTA_DLSYM(h, sym)   dlsym(h, sym)
  #define DELTA_DLCLOSE(h)      dlclose(h)
#endif

#ifdef __APPLE__
  #include <sys/sysctl.h>
  #include <sys/types.h>
  #include <mach/mach.h>
  #include <mach/mach_host.h>
  #include <IOKit/IOKitLib.h>
  #include <IOKit/graphics/IOGraphicsLib.h>
#elif defined(_WIN32)
  // GlobalMemoryStatusEx is in windows.h (already included)
#else
  // Linux
  #include <fstream>
  #include <string>
#endif

namespace delta {

// ============================================================
// NVML Type Definitions (for dynamic loading)
// ============================================================

#define NVML_SUCCESS 0

typedef int nvmlReturn_t;

struct nvmlMemory_t {
    unsigned long long total;
    unsigned long long free;
    unsigned long long used;
};

struct nvmlUtilization_t {
    unsigned int gpu;
    unsigned int memory;
};

// NVML function pointer types
typedef nvmlReturn_t (*nvmlInit_v2_t)(void);
typedef nvmlReturn_t (*nvmlShutdown_t)(void);
typedef nvmlReturn_t (*nvmlDeviceGetCount_v2_t)(unsigned int*);
typedef nvmlReturn_t (*nvmlDeviceGetHandleByIndex_v2_t)(unsigned int, void**);
typedef nvmlReturn_t (*nvmlDeviceGetName_t)(void*, char*, unsigned int);
typedef nvmlReturn_t (*nvmlDeviceGetMemoryInfo_t)(void*, nvmlMemory_t*);
typedef nvmlReturn_t (*nvmlDeviceGetUtilizationRates_t)(void*, nvmlUtilization_t*);
typedef nvmlReturn_t (*nvmlDeviceGetTemperature_t)(void*, int, unsigned int*);
typedef nvmlReturn_t (*nvmlDeviceGetPowerUsage_t)(void*, unsigned int*);

// Global NVML function pointers
static nvmlInit_v2_t                g_nvmlInit              = nullptr;
static nvmlShutdown_t               g_nvmlShutdown           = nullptr;
static nvmlDeviceGetCount_v2_t      g_nvmlDeviceGetCount     = nullptr;
static nvmlDeviceGetHandleByIndex_v2_t g_nvmlDeviceGetHandle = nullptr;
static nvmlDeviceGetName_t          g_nvmlDeviceGetName      = nullptr;
static nvmlDeviceGetMemoryInfo_t    g_nvmlDeviceGetMemoryInfo = nullptr;
static nvmlDeviceGetUtilizationRates_t g_nvmlDeviceGetUtilization = nullptr;
static nvmlDeviceGetTemperature_t   g_nvmlDeviceGetTemperature = nullptr;
static nvmlDeviceGetPowerUsage_t    g_nvmlDeviceGetPowerUsage = nullptr;

// ============================================================
// Constructor / Destructor
// ============================================================

HardwareMonitor::HardwareMonitor() {
    std::cout << "[DHATS] Initializing Hardware Monitor (Dynamic Telemetry Loading)..." << std::endl;

    probe_nvidia_nvml();
    probe_apple_metal();
    probe_amd_rocm();

    std::string backend = get_primary_backend();
    std::cout << "[DHATS] Primary acceleration backend: " << backend << std::endl;
}

HardwareMonitor::~HardwareMonitor() {
    if (nvml_lib_handle_) {
        if (g_nvmlShutdown) g_nvmlShutdown();
        DELTA_DLCLOSE(nvml_lib_handle_);
        nvml_lib_handle_ = nullptr;
    }
    if (rocm_lib_handle_) {
        DELTA_DLCLOSE(rocm_lib_handle_);
        rocm_lib_handle_ = nullptr;
    }
    std::cout << "[DHATS] Hardware Monitor shut down." << std::endl;
}

// ============================================================
// DTL Probe: NVIDIA NVML
// ============================================================

void HardwareMonitor::probe_nvidia_nvml() {
    // Try multiple library names across platforms
#ifdef _WIN32
    const char* lib_names[] = {"nvml.dll", nullptr};
#elif defined(__APPLE__)
    const char* lib_names[] = {"libnvidia-ml.dylib", nullptr};
#else
    const char* lib_names[] = {"libnvidia-ml.so.1", "libnvidia-ml.so", nullptr};
#endif

    for (int i = 0; lib_names[i] != nullptr; i++) {
        nvml_lib_handle_ = DELTA_DLOPEN(lib_names[i]);
        if (nvml_lib_handle_) break;
    }

    if (!nvml_lib_handle_) {
        std::cout << "[DHATS] NVML not found — NVIDIA telemetry unavailable." << std::endl;
        return;
    }

    // Resolve function pointers
    g_nvmlInit                = (nvmlInit_v2_t)DELTA_DLSYM(nvml_lib_handle_, "nvmlInit_v2");
    g_nvmlShutdown            = (nvmlShutdown_t)DELTA_DLSYM(nvml_lib_handle_, "nvmlShutdown");
    g_nvmlDeviceGetCount      = (nvmlDeviceGetCount_v2_t)DELTA_DLSYM(nvml_lib_handle_, "nvmlDeviceGetCount_v2");
    g_nvmlDeviceGetHandle     = (nvmlDeviceGetHandleByIndex_v2_t)DELTA_DLSYM(nvml_lib_handle_, "nvmlDeviceGetHandleByIndex_v2");
    g_nvmlDeviceGetName       = (nvmlDeviceGetName_t)DELTA_DLSYM(nvml_lib_handle_, "nvmlDeviceGetName");
    g_nvmlDeviceGetMemoryInfo = (nvmlDeviceGetMemoryInfo_t)DELTA_DLSYM(nvml_lib_handle_, "nvmlDeviceGetMemoryInfo");
    g_nvmlDeviceGetUtilization = (nvmlDeviceGetUtilizationRates_t)DELTA_DLSYM(nvml_lib_handle_, "nvmlDeviceGetUtilizationRates");
    g_nvmlDeviceGetTemperature = (nvmlDeviceGetTemperature_t)DELTA_DLSYM(nvml_lib_handle_, "nvmlDeviceGetTemperature");
    g_nvmlDeviceGetPowerUsage = (nvmlDeviceGetPowerUsage_t)DELTA_DLSYM(nvml_lib_handle_, "nvmlDeviceGetPowerUsage");

    if (!g_nvmlInit || g_nvmlInit() != NVML_SUCCESS) {
        std::cout << "[DHATS] NVML loaded but initialization failed (driver mismatch?)." << std::endl;
        DELTA_DLCLOSE(nvml_lib_handle_);
        nvml_lib_handle_ = nullptr;
        return;
    }

    nvml_available_ = true;
    unsigned int count = 0;
    if (g_nvmlDeviceGetCount) g_nvmlDeviceGetCount(&count);
    std::cout << "[DHATS] NVML active — " << count << " NVIDIA GPU(s) detected." << std::endl;
}

// ============================================================
// DTL Probe: Apple Metal / IOKit
// ============================================================

void HardwareMonitor::probe_apple_metal() {
#ifdef __APPLE__
    metal_available_ = true;
    std::cout << "[DHATS] Apple Metal backend available." << std::endl;
#else
    metal_available_ = false;
#endif
}

// ============================================================
// DTL Probe: AMD ROCm SMI
// ============================================================

void HardwareMonitor::probe_amd_rocm() {
#ifdef _WIN32
    const char* lib_names[] = {"amd_smm.dll", nullptr};
#elif defined(__APPLE__)
    const char* lib_names[] = {nullptr}; // No ROCm on macOS
#else
    const char* lib_names[] = {"librocm_smi64.so", "librocm_smi64.so.1", nullptr};
#endif

    for (int i = 0; lib_names[i] != nullptr; i++) {
        rocm_lib_handle_ = DELTA_DLOPEN(lib_names[i]);
        if (rocm_lib_handle_) break;
    }

    if (!rocm_lib_handle_) {
        std::cout << "[DHATS] ROCm SMI not found — AMD telemetry unavailable." << std::endl;
        return;
    }

    rocm_available_ = true;
    std::cout << "[DHATS] ROCm SMI active — AMD GPU telemetry enabled." << std::endl;
}

// ============================================================
// System RAM Collection (Cross-Platform)
// ============================================================

float HardwareMonitor::collect_system_ram_total_gb() {
#ifdef _WIN32
    MEMORYSTATUSEX status;
    status.dwLength = sizeof(status);
    if (GlobalMemoryStatusEx(&status)) {
        return (float)(status.ullTotalPhys / (1024.0 * 1024.0 * 1024.0));
    }
    return 0.0f;

#elif defined(__APPLE__)
    int64_t memsize = 0;
    size_t len = sizeof(memsize);
    int mib[2] = {CTL_HW, HW_MEMSIZE};
    if (sysctl(mib, 2, &memsize, &len, NULL, 0) == 0) {
        return (float)(memsize / (1024.0 * 1024.0 * 1024.0));
    }
    return 0.0f;

#else
    // Linux: parse /proc/meminfo
    std::ifstream meminfo("/proc/meminfo");
    std::string line;
    while (std::getline(meminfo, line)) {
        if (line.find("MemTotal:") == 0) {
            long long kb = 0;
            sscanf(line.c_str(), "MemTotal: %lld kB", &kb);
            return (float)(kb / (1024.0 * 1024.0));
        }
    }
    return 0.0f;
#endif
}

float HardwareMonitor::collect_system_ram_used_gb() {
#ifdef _WIN32
    MEMORYSTATUSEX status;
    status.dwLength = sizeof(status);
    if (GlobalMemoryStatusEx(&status)) {
        unsigned long long used = status.ullTotalPhys - status.ullAvailPhys;
        return (float)(used / (1024.0 * 1024.0 * 1024.0));
    }
    return 0.0f;

#elif defined(__APPLE__)
    mach_msg_type_number_t count = HOST_VM_INFO64_COUNT;
    vm_statistics64_data_t vmstat;
    if (host_statistics64(mach_host_self(), HOST_VM_INFO64,
                          (host_info64_t)&vmstat, &count) == KERN_SUCCESS) {
        vm_size_t page_size;
        host_page_size(mach_host_self(), &page_size);
        unsigned long long used = (vmstat.active_count + vmstat.wire_count) * page_size;
        return (float)(used / (1024.0 * 1024.0 * 1024.0));
    }
    return 0.0f;

#else
    // Linux
    std::ifstream meminfo("/proc/meminfo");
    std::string line;
    long long total_kb = 0, avail_kb = 0;
    while (std::getline(meminfo, line)) {
        if (line.find("MemTotal:") == 0) {
            sscanf(line.c_str(), "MemTotal: %lld kB", &total_kb);
        } else if (line.find("MemAvailable:") == 0) {
            sscanf(line.c_str(), "MemAvailable: %lld kB", &avail_kb);
        }
    }
    return (float)((total_kb - avail_kb) / (1024.0 * 1024.0));
#endif
}

// ============================================================
// GPU Metric Collectors
// ============================================================

void HardwareMonitor::collect_nvidia_metrics(std::vector<GPUMetrics>& out) {
    if (!nvml_available_ || !g_nvmlDeviceGetCount) return;

    unsigned int device_count = 0;
    if (g_nvmlDeviceGetCount(&device_count) != NVML_SUCCESS) return;

    for (unsigned int i = 0; i < device_count; i++) {
        void* device = nullptr;
        if (g_nvmlDeviceGetHandle(i, &device) != NVML_SUCCESS) continue;

        GPUMetrics gpu;
        gpu.backend = "CUDA";

        // Name
        char name_buf[256] = {0};
        if (g_nvmlDeviceGetName && g_nvmlDeviceGetName(device, name_buf, sizeof(name_buf)) == NVML_SUCCESS) {
            gpu.name = name_buf;
        } else {
            gpu.name = "NVIDIA GPU " + std::to_string(i);
        }

        // Memory
        nvmlMemory_t mem;
        if (g_nvmlDeviceGetMemoryInfo && g_nvmlDeviceGetMemoryInfo(device, &mem) == NVML_SUCCESS) {
            gpu.vram_used_gb  = (float)(mem.used / (1024.0 * 1024.0 * 1024.0));
            gpu.vram_total_gb = (float)(mem.total / (1024.0 * 1024.0 * 1024.0));
        }

        // Utilization
        nvmlUtilization_t util;
        if (g_nvmlDeviceGetUtilization && g_nvmlDeviceGetUtilization(device, &util) == NVML_SUCCESS) {
            gpu.gpu_util_pct = (int)util.gpu;
        }

        // Temperature (sensor 0 = NVML_TEMPERATURE_GPU)
        unsigned int temp = 0;
        if (g_nvmlDeviceGetTemperature && g_nvmlDeviceGetTemperature(device, 0, &temp) == NVML_SUCCESS) {
            gpu.temp_c = (float)temp;
        }

        // Power (in milliwatts)
        unsigned int power_mw = 0;
        if (g_nvmlDeviceGetPowerUsage && g_nvmlDeviceGetPowerUsage(device, &power_mw) == NVML_SUCCESS) {
            gpu.power_w = (float)(power_mw / 1000.0);
        }

        out.push_back(gpu);
    }
}

void HardwareMonitor::collect_apple_metrics(std::vector<GPUMetrics>& out) {
#ifdef __APPLE__
    if (!metal_available_) return;

    GPUMetrics gpu;
    gpu.backend = "Metal";

    // Get chip name via sysctl
    char chip_name[128] = {0};
    size_t len = sizeof(chip_name);
    if (sysctlbyname("machdep.cpu.brand_string", chip_name, &len, NULL, 0) == 0) {
        gpu.name = chip_name;
    } else {
        gpu.name = "Apple GPU";
    }

    // Apple Silicon: shared memory — total RAM = total VRAM
    int64_t memsize = 0;
    size_t mlen = sizeof(memsize);
    int mib[2] = {CTL_HW, HW_MEMSIZE};
    if (sysctl(mib, 2, &memsize, &mlen, NULL, 0) == 0) {
        gpu.vram_total_gb = (float)(memsize / (1024.0 * 1024.0 * 1024.0));
    }

    // Used memory (approximate via vm_statistics)
    mach_msg_type_number_t count = HOST_VM_INFO64_COUNT;
    vm_statistics64_data_t vmstat;
    if (host_statistics64(mach_host_self(), HOST_VM_INFO64,
                          (host_info64_t)&vmstat, &count) == KERN_SUCCESS) {
        vm_size_t page_size;
        host_page_size(mach_host_self(), &page_size);
        unsigned long long used = (vmstat.active_count + vmstat.wire_count +
                                   vmstat.compressor_page_count) * page_size;
        gpu.vram_used_gb = (float)(used / (1024.0 * 1024.0 * 1024.0));
    }

    // GPU utilization via IOKit (best-effort)
    gpu.gpu_util_pct = 0; // IOKit doesn't easily expose %; leave 0
    gpu.temp_c = 0.0f;   // Requires private APIs; leave 0
    gpu.power_w = 0.0f;  // Requires private APIs; leave 0

    out.push_back(gpu);
#endif
}

void HardwareMonitor::collect_amd_metrics(std::vector<GPUMetrics>& out) {
    (void)out;
    if (!rocm_available_) return;
    // TODO: Implement ROCm SMI dynamic function calls
    // Similar pattern to NVML above
}

// ============================================================
// Public API
// ============================================================

HardwareMetrics HardwareMonitor::get_metrics() {
    std::lock_guard<std::mutex> lock(mtx_);

    HardwareMetrics m;
    m.timestamp_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    // System RAM
    m.system_ram_total_gb = collect_system_ram_total_gb();
    m.system_ram_used_gb  = collect_system_ram_used_gb();

    // GPU metrics (all vendors)
    collect_nvidia_metrics(m.gpus);
    collect_apple_metrics(m.gpus);
    collect_amd_metrics(m.gpus);

    return m;
}

int HardwareMonitor::calculate_auto_ngl(long long model_size_bytes, int n_layers, int ctx_size) {
    std::lock_guard<std::mutex> lock(mtx_);

    // No GPU → CPU only
    if (!nvml_available_ && !metal_available_ && !rocm_available_) {
        return 0;
    }

    float total_vram_gb = 0.0f;
    float used_vram_gb  = 0.0f;

    if (nvml_available_) {
        // Sum all NVIDIA GPUs
        unsigned int count = 0;
        if (g_nvmlDeviceGetCount && g_nvmlDeviceGetCount(&count) == NVML_SUCCESS) {
            for (unsigned int i = 0; i < count; i++) {
                void* dev = nullptr;
                if (g_nvmlDeviceGetHandle(i, &dev) != NVML_SUCCESS) continue;
                nvmlMemory_t mem;
                if (g_nvmlDeviceGetMemoryInfo(dev, &mem) == NVML_SUCCESS) {
                    total_vram_gb += (float)(mem.total / (1024.0 * 1024.0 * 1024.0));
                    used_vram_gb  += (float)(mem.used  / (1024.0 * 1024.0 * 1024.0));
                }
            }
        }
    } else if (metal_available_) {
#ifdef __APPLE__
        int64_t memsize = 0;
        size_t len = sizeof(memsize);
        int mib[2] = {CTL_HW, HW_MEMSIZE};
        if (sysctl(mib, 2, &memsize, &len, NULL, 0) == 0) {
            total_vram_gb = (float)(memsize / (1024.0 * 1024.0 * 1024.0));
        }
        // Used memory
        mach_msg_type_number_t count = HOST_VM_INFO64_COUNT;
        vm_statistics64_data_t vmstat;
        if (host_statistics64(mach_host_self(), HOST_VM_INFO64,
                              (host_info64_t)&vmstat, &count) == KERN_SUCCESS) {
            vm_size_t page_size;
            host_page_size(mach_host_self(), &page_size);
            used_vram_gb = (float)((vmstat.active_count + vmstat.wire_count) * page_size
                           / (1024.0 * 1024.0 * 1024.0));
        }
#endif
    }

    if (total_vram_gb <= 0.0f) return 0;

    // Reserve 1.5 GB for OS / display / KV cache headroom
    float available_gb = total_vram_gb - used_vram_gb - 1.5f;
    if (available_gb <= 0.0f) return 0;

    float model_size_gb = (float)(model_size_bytes / (1024.0 * 1024.0 * 1024.0));

    // Estimate KV cache: ~0.5 MB per 1K tokens for 7B-class models
    float kv_cache_gb = (float)(ctx_size * 0.5 * n_layers) / (1024.0 * 1024.0);

    float total_needed_gb = model_size_gb + kv_cache_gb;

    if (available_gb >= total_needed_gb) {
        return -1; // All layers fit — offload everything
    }

    // Partial offload: estimate how many layers fit
    float per_layer_gb = model_size_gb / (float)std::max(n_layers, 1);
    if (per_layer_gb <= 0.0f) return 0;

    int layers_that_fit = (int)((available_gb - kv_cache_gb) / per_layer_gb);
    return std::max(0, std::min(layers_that_fit, n_layers));
}

bool HardwareMonitor::has_gpu() const {
    return nvml_available_ || metal_available_ || rocm_available_;
}

std::string HardwareMonitor::get_primary_backend() const {
    if (nvml_available_) return "CUDA (NVIDIA)";
    if (metal_available_) return "Metal (Apple)";
    if (rocm_available_) return "ROCm (AMD)";
    return "CPU";
}

} // namespace delta