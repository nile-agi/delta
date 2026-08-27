#include "hardware_monitor.h"
#include <iostream>
#include <cstring>
#include <chrono>
#include <cstdio>
#include <fstream>
#include <string>

#ifdef _WIN32
  #include <windows.h>
  #define DELTA_DLOPEN(lib)     LoadLibraryA(lib)
  #define DELTA_DLSYM(h, sym)   GetProcAddress((HMODULE)h, sym)
  #define DELTA_DLCLOSE(h)      FreeLibrary((HMODULE)h)
  #include <wbemidl.h>
  #include <comdef.h>
  #pragma comment(lib, "wbemuuid.lib")
  #pragma comment(lib, "oleaut32.lib")
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
  #include <CoreFoundation/CoreFoundation.h>
  #include <objc/message.h>
  #include <objc/runtime.h>
#endif

namespace delta {

// ============================================================
// NVML & ROCm Type Definitions
// ============================================================
#define NVML_SUCCESS 0
typedef int nvmlReturn_t;
struct nvmlMemory_t { unsigned long long total, free, used; };
struct nvmlUtilization_t { unsigned int gpu, memory; };

typedef nvmlReturn_t (*nvmlInit_v2_t)(void);
typedef nvmlReturn_t (*nvmlShutdown_t)(void);
typedef nvmlReturn_t (*nvmlDeviceGetCount_v2_t)(unsigned int*);
typedef nvmlReturn_t (*nvmlDeviceGetHandleByIndex_v2_t)(unsigned int, void**);
typedef nvmlReturn_t (*nvmlDeviceGetName_t)(void*, char*, unsigned int);
typedef nvmlReturn_t (*nvmlDeviceGetMemoryInfo_t)(void*, nvmlMemory_t*);
typedef nvmlReturn_t (*nvmlDeviceGetUtilizationRates_t)(void*, nvmlUtilization_t*);
typedef nvmlReturn_t (*nvmlDeviceGetTemperature_t)(void*, int, unsigned int*);
typedef nvmlReturn_t (*nvmlDeviceGetPowerUsage_t)(void*, unsigned int*);

static nvmlInit_v2_t g_nvmlInit = nullptr;
static nvmlShutdown_t g_nvmlShutdown = nullptr;
static nvmlDeviceGetCount_v2_t g_nvmlDeviceGetCount = nullptr;
static nvmlDeviceGetHandleByIndex_v2_t g_nvmlDeviceGetHandle = nullptr;
static nvmlDeviceGetName_t g_nvmlDeviceGetName = nullptr;
static nvmlDeviceGetMemoryInfo_t g_nvmlDeviceGetMemoryInfo = nullptr;
static nvmlDeviceGetUtilizationRates_t g_nvmlDeviceGetUtilization = nullptr;
static nvmlDeviceGetTemperature_t g_nvmlDeviceGetTemperature = nullptr;
static nvmlDeviceGetPowerUsage_t g_nvmlDeviceGetPowerUsage = nullptr;

typedef int (*rsmi_init_t)(uint64_t);
typedef int (*rsmi_shut_down_t)(void);
typedef int (*rsmi_num_monitor_devices_t)(uint32_t*);
typedef int (*rsmi_dev_name_get_t)(uint32_t, char*, size_t);
typedef int (*rsmi_dev_memory_total_get_t)(uint32_t, int, uint64_t*);
typedef int (*rsmi_dev_memory_usage_get_t)(uint32_t, int, uint64_t*);
typedef int (*rsmi_dev_busy_percent_get_t)(uint32_t, uint32_t*);
typedef int (*rsmi_dev_temp_metric_get_t)(uint32_t, uint32_t, int64_t*);
typedef int (*rsmi_dev_power_ave_get_t)(uint32_t, uint32_t, uint64_t*);

static rsmi_init_t g_rsmi_init = nullptr;
static rsmi_shut_down_t g_rsmi_shut_down = nullptr;
static rsmi_num_monitor_devices_t g_rsmi_num_devices = nullptr;
static rsmi_dev_name_get_t g_rsmi_dev_name = nullptr;
static rsmi_dev_memory_total_get_t g_rsmi_mem_total = nullptr;
static rsmi_dev_memory_usage_get_t g_rsmi_mem_used = nullptr;
static rsmi_dev_busy_percent_get_t g_rsmi_busy = nullptr;
static rsmi_dev_temp_metric_get_t g_rsmi_temp = nullptr;
static rsmi_dev_power_ave_get_t g_rsmi_power = nullptr;

// ============================================================
// Apple GPU Utilization & SMC Helpers
// ============================================================

#ifdef __APPLE__
#define SMC_KERNEL_INDEX 2
#pragma pack(push, 1)
struct SMCVersion  { UInt8 major, minor, build, reserved; UInt16 release; };
struct SMCPLimit   { UInt16 version, length; UInt32 cpu, gpu, mem; };
struct SMCVector   { UInt8 v0, v1, v2; };
struct SMCKeyVal   { UInt32 dataType; UInt8 dataAttributes; UInt8 dataSize; UInt8 data[32]; };
struct SMCParam    { UInt32 key; SMCVersion vers; SMCPLimit pLimit; SMCVector v8r; SMCKeyVal keyData; };
#pragma pack(pop)

// Parse any SMC temperature encoding (sp78/sp87/fpe2/ui8/ui16/si16)
static float parse_smc_temp(const SMCKeyVal& kv) {
    if (kv.dataSize < 1) return 0.0f;
    const uint8_t* d = kv.data;
    char cc[5] = {(char)((kv.dataType >> 24) & 0xFF), (char)((kv.dataType >> 16) & 0xFF),
                  (char)((kv.dataType >> 8) & 0xFF), (char)(kv.dataType & 0xFF), 0};
    if (strncmp(cc, "fpe2", 4) == 0 && kv.dataSize >= 2) return (float)(((d[0] << 8) | d[1]) / 4.0f);
    if (strncmp(cc, "ui8", 3) == 0)  return (float)d[0];
    if (strncmp(cc, "ui16", 4) == 0 && kv.dataSize >= 2) return (float)((d[0] << 8) | d[1]);
    if (strncmp(cc, "si16", 4) == 0 && kv.dataSize >= 2) return (float)(int16_t)((d[0] << 8) | d[1]);
    if (kv.dataSize >= 2) return (float)(int8_t)d[0] + (float)d[1] / 256.0f; // sp78/sp87
    return 0.0f;
}

static float smc_cpu_temp_c() {
    io_service_t svc = IOServiceGetMatchingService(kIOMainPortDefault, IOServiceMatching("AppleSMC"));
    if (!svc) return 0.0f;
    io_connect_t conn = 0;
    if (IOServiceOpen(svc, mach_task_self(), 0, &conn) != KERN_SUCCESS) { IOObjectRelease(svc); return 0.0f; }

    // Intel CPU keys first, then Apple Silicon SoC/PMIC/proximity sensors
    static const char* keys[] = {
        "TC0D","TC0P","TC1D","TC1P","TC0E","TC0F",                 // Intel
        "Tp0P","Tp1P","Tp2P","Tp3P","Tp4P","Tp5P",                 // M-series PMIC/SoC
        "Tb0P","Tb1P","Tb2P","Te0P","Te1P","Te2P","Te3P",          // battery/efficiency
        "Tm0P","Tm1P","Tm2P","Ta0P","Ta1P","Th0P","Th1P","Th2P",   // memory/ane/heat-pipe
        nullptr };

    static std::vector<const char*> good;   // cache keys that returned sane values
    float best = 0.0f;

    auto try_key = [&](const char* k) {
        SMCParam in{}, out{};
        size_t outSize = sizeof(out);
        in.key = ((UInt32)k[0] << 24) | ((UInt32)k[1] << 16) | ((UInt32)k[2] << 8) | (UInt32)k[3];
        if (IOConnectCallStructMethod(conn, SMC_KERNEL_INDEX, &in, sizeof(in), &out, &outSize) == KERN_SUCCESS) {
            float t = parse_smc_temp(out.keyData);
            if (t > 1.0f && t < 110.0f) {
                if (std::find(good.begin(), good.end(), k) == good.end()) good.push_back(k);
                if (t > best) best = t;
            }
        }
    };

    if (!good.empty()) { for (auto k : good) try_key(k); }        // fast path after first hit
    else               { for (int i = 0; keys[i]; i++) try_key(keys[i]); }

    IOServiceClose(conn);
    IOObjectRelease(svc);
    return best;   // hottest real sensor; 0 only if the OS exposes nothing
}

// Real, always-available sensor on MacBooks (battery-board temperature).
// Apple exposes no public SoC temp on Apple Silicon; this is the honest
// fallback so the gauge never shows N/A on laptops.
static float battery_temp_c() {
    io_service_t bat = IOServiceGetMatchingService(kIOMainPortDefault,
                                                   IOServiceMatching("AppleSmartBattery"));
    if (!bat) return 0.0f;
    float t = 0.0f;
    CFNumberRef num = (CFNumberRef)IORegistryEntryCreateCFProperty(
        bat, CFSTR("Temperature"), kCFAllocatorDefault, kNilOptions);
    if (num) {
        int v = 0;
        if (CFNumberGetValue(num, kCFNumberIntType, &v)) {
            float bt = (float)v / 100.0f;               // centi-°C
            if (bt > 1.0f && bt < 110.0f) t = bt;
        }
        CFRelease(num);
    }
    IOObjectRelease(bat);
    return t;
}

// Apple Silicon fallback: read HID temperature services (real sensor events)
static float hid_max_temp_c() {
    void* iokit = dlopen("/System/Library/Frameworks/IOKit.framework/IOKit", RTLD_LAZY);
    if (!iokit) return 0.0f;
    typedef void* (*create_t)(CFAllocatorRef, int, CFDictionaryRef);
    typedef CFArrayRef (*copy_services_t)(void*);
    typedef void* (*copy_event_t)(void*, int, int32_t, uint64_t);
    typedef double (*get_float_t)(void*, int);
    create_t      create = (create_t)dlsym(iokit, "IOHIDEventSystemClientCreateWithType");
    copy_services_t copyS = (copy_services_t)dlsym(iokit, "IOHIDEventSystemClientCopyServices");
    copy_event_t  copyE = (copy_event_t)dlsym(iokit, "IOHIDEventServiceClientCopyEvent");
    get_float_t   getF  = (get_float_t)dlsym(iokit, "IOHIDEventGetFloatValue");
    float best = 0.0f;
    if (create && copyS && copyE && getF) {
        void* client = create(kCFAllocatorDefault, 0, NULL);
        if (client) {
            CFArrayRef services = copyS(client);
            if (services) {
                CFIndex n = CFArrayGetCount(services);
                for (CFIndex i = 0; i < n; i++) {
                    void* svc = (void*)CFArrayGetValueAtIndex(services, i);
                    void* ev = copyE(svc, 15 /*kIOHIDEventTypeTemperature*/, 0, 0);
                    if (ev) {
                        double t = getF(ev, (15 << 16) | 0x1);
                        if (t > 110.0 && t < 11000.0) t /= 100.0;   // ← centi-°C fix, HERE
                        if (t > 0.0 && t < 110.0 && t > best) best = (float)t;
                        CFRelease(ev);
                    }
                }
                CFRelease(services);
            }
            CFRelease(client);
        }
    }
    dlclose(iokit);
    return best;
}

static int apple_gpu_utilization_pct() {
    io_iterator_t iter;
    if (IOServiceGetMatchingServices(kIOMainPortDefault,
                                     IOServiceMatching("IOAccelerator"), &iter) != KERN_SUCCESS)
        return 0;
    int util = 0;
    io_object_t obj;
    while ((obj = IOIteratorNext(iter)) != 0) {
        CFDictionaryRef stats = (CFDictionaryRef)IORegistryEntryCreateCFProperty(
            obj, CFSTR("PerformanceStatistics"), kCFAllocatorDefault, kNilOptions);
        if (stats) {
            CFNumberRef num = (CFNumberRef)CFDictionaryGetValue(stats, CFSTR("Device Utilization %"));
            if (num) {
                int v = 0;
                CFNumberGetValue(num, kCFNumberIntType, &v);
                if (v > util) util = v;
            }
            CFRelease(stats);
        }
        IOObjectRelease(obj);
    }
    IOObjectRelease(iter);
    return util;
}
#endif

// ============================================================
// Windows CPU Temp via WMI
// ============================================================

#ifdef _WIN32
static float wmi_cpu_temp_c() {
    float temp = 0.0f;
    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    bool own = SUCCEEDED(hr);
    IWbemLocator* loc = nullptr;
    if (SUCCEEDED(CoCreateInstance(CLSID_WbemLocator, 0, CLSCTX_INPROC_SERVER,
                                   IID_IWbemLocator, (void**)&loc))) {
        IWbemServices* svc = nullptr;
        if (SUCCEEDED(loc->ConnectServer(_bstr_t(L"root\\WMI"), nullptr, nullptr, 0,
                                         0, 0, 0, &svc))) {
            CoSetProxyBlanket(svc, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, nullptr,
                              RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE, nullptr, EOAC_NONE);
            IEnumWbemClassObject* en = nullptr;
            if (SUCCEEDED(svc->ExecQuery(_bstr_t(L"WQL"),
                    _bstr_t(L"SELECT CurrentTemperature FROM MSAcpi_ThermalZoneTemperature"),
                    WBEM_FLAG_FORWARD_ONLY, nullptr, &en))) {
                IWbemClassObject* obj = nullptr;
                ULONG n = 0;
                long long sum = 0; int cnt = 0;
                while (en->Next(WBEM_INFINITE, 1, &obj, &n) == S_OK && obj) {
                    VARIANT v; VariantInit(&v);
                    if (SUCCEEDED(obj->Get(L"CurrentTemperature", 0, &v, 0, 0))) {
                        sum += v.llVal; cnt++; VariantClear(&v);   // tenths of Kelvin
                    }
                    obj->Release();
                }
                if (cnt) temp = (float)((sum / (double)cnt) / 10.0 - 273.15);
                en->Release();
            }
            svc->Release();
        }
        loc->Release();
    }
    if (own) CoUninitialize();
    return temp;
}
#endif

// ============================================================
// Constructor / Destructor
// ============================================================

HardwareMonitor::HardwareMonitor() {
    std::cout << "[DHATS] Initializing Hardware Monitor (Dynamic Telemetry Loading)..." << std::endl;
    probe_nvidia_nvml();
    probe_apple_metal();
    probe_amd_rocm();
    std::cout << "[DHATS] Primary acceleration backend: " << get_primary_backend() << std::endl;
}

HardwareMonitor::~HardwareMonitor() {
    if (nvml_lib_handle_) {
        if (g_nvmlShutdown) g_nvmlShutdown();
        DELTA_DLCLOSE(nvml_lib_handle_);
    }
    if (rocm_lib_handle_) {
        if (g_rsmi_shut_down) g_rsmi_shut_down();
        DELTA_DLCLOSE(rocm_lib_handle_);
    }
}

// ============================================================
// DTL Probes
// ============================================================

void HardwareMonitor::probe_nvidia_nvml() {
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
        DELTA_DLCLOSE(nvml_lib_handle_);
        nvml_lib_handle_ = nullptr;
        return;
    }

    nvml_available_ = true;
    unsigned int count = 0;
    if (g_nvmlDeviceGetCount) g_nvmlDeviceGetCount(&count);
    std::cout << "[DHATS] NVML active — " << count << " NVIDIA GPU(s) detected." << std::endl;
}

void HardwareMonitor::probe_apple_metal() {
#ifdef __APPLE__
    metal_available_ = true;
    std::cout << "[DHATS] Apple Metal backend available." << std::endl;
#else
    metal_available_ = false;
#endif
}

void HardwareMonitor::probe_amd_rocm() {
#ifndef _WIN32
    const char* lib_names[] = {"librocm_smi64.so", "librocm_smi64.so.1", "librocm_smi.so", nullptr};
    for (int i = 0; lib_names[i] != nullptr; i++) {
        rocm_lib_handle_ = DELTA_DLOPEN(lib_names[i]);
        if (rocm_lib_handle_) break;
    }

    if (!rocm_lib_handle_) {
        std::cout << "[DHATS] ROCm SMI not found — AMD telemetry unavailable." << std::endl;
        return;
    }

    g_rsmi_init = (rsmi_init_t)DELTA_DLSYM(rocm_lib_handle_, "rsmi_init");
    g_rsmi_shut_down = (rsmi_shut_down_t)DELTA_DLSYM(rocm_lib_handle_, "rsmi_shut_down");
    g_rsmi_num_devices = (rsmi_num_monitor_devices_t)DELTA_DLSYM(rocm_lib_handle_, "rsmi_num_monitor_devices");
    g_rsmi_dev_name = (rsmi_dev_name_get_t)DELTA_DLSYM(rocm_lib_handle_, "rsmi_dev_name_get");
    g_rsmi_mem_total = (rsmi_dev_memory_total_get_t)DELTA_DLSYM(rocm_lib_handle_, "rsmi_dev_memory_total_get");
    g_rsmi_mem_used = (rsmi_dev_memory_usage_get_t)DELTA_DLSYM(rocm_lib_handle_, "rsmi_dev_memory_usage_get");
    g_rsmi_busy = (rsmi_dev_busy_percent_get_t)DELTA_DLSYM(rocm_lib_handle_, "rsmi_dev_busy_percent_get");
    g_rsmi_temp = (rsmi_dev_temp_metric_get_t)DELTA_DLSYM(rocm_lib_handle_, "rsmi_dev_temp_metric_get");
    g_rsmi_power = (rsmi_dev_power_ave_get_t)DELTA_DLSYM(rocm_lib_handle_, "rsmi_dev_power_ave_get");

    if (!g_rsmi_init || g_rsmi_init(0) != 0) {
        DELTA_DLCLOSE(rocm_lib_handle_);
        rocm_lib_handle_ = nullptr;
        return;
    }

    rocm_available_ = true;
    uint32_t count = 0;
    if (g_rsmi_num_devices) g_rsmi_num_devices(&count);
    std::cout << "[DHATS] ROCm SMI active — " << count << " AMD GPU(s) detected." << std::endl;
#endif
}

// ============================================================
// Cross-Platform CPU & RAM Collectors
// ============================================================

float HardwareMonitor::collect_cpu_util_pct() {
#ifdef _WIN32
    FILETIME idle, kernel, user;
    if (!GetSystemTimes(&idle, &kernel, &user)) return 0.0f;
    
    ULARGE_INTEGER i_time, k_time, u_time;
    i_time.LowPart = idle.dwLowDateTime; i_time.HighPart = idle.dwHighDateTime;
    k_time.LowPart = kernel.dwLowDateTime; k_time.HighPart = kernel.dwHighDateTime;
    u_time.LowPart = user.dwLowDateTime; u_time.HighPart = user.dwHighDateTime;
    
    unsigned long long idle_diff = i_time.QuadPart - prev_idle_time_.QuadPart;
    unsigned long long kernel_diff = k_time.QuadPart - prev_kernel_time_.QuadPart;
    unsigned long long user_diff = u_time.QuadPart - prev_user_time_.QuadPart;
    
    prev_idle_time_ = i_time; prev_kernel_time_ = k_time; prev_user_time_ = u_time;
    
    unsigned long long total = kernel_diff + user_diff;
    if (total == 0) return 0.0f;
    return (float)((total - idle_diff) * 100.0 / total);

#elif defined(__APPLE__)
    mach_msg_type_number_t count = HOST_CPU_LOAD_INFO_COUNT;
    host_cpu_load_info_data_t cpustats;
    if (host_statistics(mach_host_self(), HOST_CPU_LOAD_INFO, (host_info_t)&cpustats, &count) != KERN_SUCCESS) return 0.0f;
    
    uint64_t used = cpustats.cpu_ticks[CPU_STATE_USER] + cpustats.cpu_ticks[CPU_STATE_SYSTEM] + cpustats.cpu_ticks[CPU_STATE_NICE];
    uint64_t total = used + cpustats.cpu_ticks[CPU_STATE_IDLE];
    
    uint64_t used_diff = used - prev_cpu_used_;
    uint64_t total_diff = total - prev_cpu_total_;
    
    prev_cpu_used_ = used; prev_cpu_total_ = total;
    
    if (total_diff == 0) return 0.0f;
    return (float)(used_diff * 100.0 / total_diff);

#else
    std::ifstream stat("/proc/stat");
    std::string line;
    if (std::getline(stat, line) && line.find("cpu ") == 0) {
        unsigned long long user, nice, system, idle, iowait, irq, softirq, steal;
        sscanf(line.c_str(), "cpu %llu %llu %llu %llu %llu %llu %llu %llu",
               &user, &nice, &system, &idle, &iowait, &irq, &softirq, &steal);
        
        unsigned long long total = user + nice + system + idle + iowait + irq + softirq + steal;
        unsigned long long used = total - idle - iowait;
        
        unsigned long long total_diff = total - prev_cpu_total_;
        unsigned long long used_diff = used - prev_cpu_used_;
        
        prev_cpu_total_ = total; prev_cpu_used_ = used;
        
        if (total_diff == 0) return 0.0f;
        return (float)(used_diff * 100.0 / total_diff);
    }
    return 0.0f;
#endif
}

float HardwareMonitor::collect_cpu_temp_c() {
#if defined(_WIN32)
    return wmi_cpu_temp_c();
#elif defined(__APPLE__)
    float t = smc_cpu_temp_c();                    // 1) true CPU die temp on Intel,
                                                   //    any exposed SMC sensor on M-series
    if (t <= 0.0f) t = hid_max_temp_c();           // 2) HID temperature services
    if (t <= 0.0f) t = battery_temp_c();           // 3) battery-board sensor (MacBooks)
    return t;                                      // 0 only on battery-less Macs
#else
    float best = 0.0f;
    for (int z = 0; z < 20; z++) {
        std::ifstream t("/sys/class/thermal/thermal_zone" + std::to_string(z) + "/temp");
        if (!t) continue;
        long long v = 0; t >> v;
        if (v > 0) { float c = v / 1000.0f; if (c > best) best = c; }
    }
    return best;
#endif
}

float HardwareMonitor::collect_system_power_w() {
#if defined(__linux__)
    static long long prev_e = 0; static auto prev_t = std::chrono::steady_clock::now();
    std::ifstream f("/sys/class/powercap/intel-rapl:0/energy_uj");
    if (!f) return 0.0f;
    long long e = 0; f >> e;
    auto now = std::chrono::steady_clock::now();
    double s = std::chrono::duration<double>(now - prev_t).count();
    float w = (prev_e && s > 0) ? (float)((e - prev_e) / 1e6 / s) : 0.0f;
    prev_e = e; prev_t = now;
    return w;
#elif defined(__APPLE__)
    // Battery discharge watts (real). On AC, Apple exposes no public system-power
    // sensor, so fall through to GPU power below.
    io_service_t bat = IOServiceGetMatchingService(kIOMainPortDefault, IOServiceMatching("AppleSmartBattery"));
    if (bat) {
        int amp = 0, volt = 0, ext = 1;
        CFNumberRef a = (CFNumberRef)IORegistryEntryCreateCFProperty(bat, CFSTR("Amperage"), kCFAllocatorDefault, kNilOptions);
        CFNumberRef v = (CFNumberRef)IORegistryEntryCreateCFProperty(bat, CFSTR("Voltage"), kCFAllocatorDefault, kNilOptions);
        CFBooleanRef e = (CFBooleanRef)IORegistryEntryCreateCFProperty(bat, CFSTR("ExternalConnected"), kCFAllocatorDefault, kNilOptions);
        if (a) { CFNumberGetValue(a, kCFNumberIntType, &amp); CFRelease(a); }
        if (v) { CFNumberGetValue(v, kCFNumberIntType, &volt); CFRelease(v); }
        if (e) { ext = CFBooleanGetValue(e); CFRelease(e); }
        IOObjectRelease(bat);
        if (!ext && amp && volt) return (float)(std::abs(amp) * volt / 1e6);
    }
    // Discrete-GPU Macs: sum real GPU power (NVML) as the measurable draw
    {
        float p = 0.0f; std::vector<GPUMetrics> g; collect_nvidia_metrics(g);
        for (auto& x : g) p += x.power_w;
        return p;
    }
#else
    // Windows: use real GPU power (NVML/ROCm) which dominates the draw on GPU machines.
    float p = 0.0f; std::vector<GPUMetrics> g;
    collect_nvidia_metrics(g); collect_amd_metrics(g);
    for (auto& x : g) p += x.power_w;
    return p;
#endif
}

float HardwareMonitor::collect_system_ram_total_gb() {
#ifdef _WIN32
    MEMORYSTATUSEX status; status.dwLength = sizeof(status);
    if (GlobalMemoryStatusEx(&status)) return (float)(status.ullTotalPhys / (1024.0 * 1024.0 * 1024.0));
    return 0.0f;
#elif defined(__APPLE__)
    int64_t memsize = 0; size_t len = sizeof(memsize);
    int mib[2] = {CTL_HW, HW_MEMSIZE};
    if (sysctl(mib, 2, &memsize, &len, NULL, 0) == 0) return (float)(memsize / (1024.0 * 1024.0 * 1024.0));
    return 0.0f;
#else
    std::ifstream meminfo("/proc/meminfo"); std::string line;
    while (std::getline(meminfo, line)) {
        if (line.find("MemTotal:") == 0) {
            long long kb = 0; sscanf(line.c_str(), "MemTotal: %lld kB", &kb);
            return (float)(kb / (1024.0 * 1024.0));
        }
    }
    return 0.0f;
#endif
}

float HardwareMonitor::collect_system_ram_used_gb() {
#ifdef _WIN32
    MEMORYSTATUSEX status; status.dwLength = sizeof(status);
    if (GlobalMemoryStatusEx(&status)) {
        unsigned long long used = status.ullTotalPhys - status.ullAvailPhys;
        return (float)(used / (1024.0 * 1024.0 * 1024.0));
    }
    return 0.0f;
#elif defined(__APPLE__)
    mach_msg_type_number_t count = HOST_VM_INFO64_COUNT;
    vm_statistics64_data_t vmstat;
    if (host_statistics64(mach_host_self(), HOST_VM_INFO64, (host_info64_t)&vmstat, &count) == KERN_SUCCESS) {
        vm_size_t page_size; host_page_size(mach_host_self(), &page_size);
        unsigned long long used = (vmstat.active_count + vmstat.wire_count) * page_size;
        return (float)(used / (1024.0 * 1024.0 * 1024.0));
    }
    return 0.0f;
#else
    std::ifstream meminfo("/proc/meminfo"); std::string line;
    long long total_kb = 0, avail_kb = 0;
    while (std::getline(meminfo, line)) {
        if (line.find("MemTotal:") == 0) sscanf(line.c_str(), "MemTotal: %lld kB", &total_kb);
        else if (line.find("MemAvailable:") == 0) sscanf(line.c_str(), "MemAvailable: %lld kB", &avail_kb);
    }
    return (float)((total_kb - avail_kb) / (1024.0 * 1024.0));
#endif
}

// ============================================================
// GPU Collectors
// ============================================================

void HardwareMonitor::collect_nvidia_metrics(std::vector<GPUMetrics>& out) {
    if (!nvml_available_ || !g_nvmlDeviceGetCount) return;
    unsigned int device_count = 0;
    if (g_nvmlDeviceGetCount(&device_count) != NVML_SUCCESS) return;

    for (unsigned int i = 0; i < device_count; i++) {
        void* device = nullptr;
        if (g_nvmlDeviceGetHandle(i, &device) != NVML_SUCCESS) continue;

        GPUMetrics gpu; gpu.backend = "CUDA";
        char name_buf[256] = {0};
        if (g_nvmlDeviceGetName && g_nvmlDeviceGetName(device, name_buf, sizeof(name_buf)) == NVML_SUCCESS) gpu.name = name_buf;
        else gpu.name = "NVIDIA GPU " + std::to_string(i);

        nvmlMemory_t mem;
        if (g_nvmlDeviceGetMemoryInfo && g_nvmlDeviceGetMemoryInfo(device, &mem) == NVML_SUCCESS) {
            gpu.vram_used_gb  = (float)(mem.used / (1024.0 * 1024.0 * 1024.0));
            gpu.vram_total_gb = (float)(mem.total / (1024.0 * 1024.0 * 1024.0));
        }
        nvmlUtilization_t util;
        if (g_nvmlDeviceGetUtilization && g_nvmlDeviceGetUtilization(device, &util) == NVML_SUCCESS) gpu.gpu_util_pct = (int)util.gpu;
        unsigned int temp = 0;
        if (g_nvmlDeviceGetTemperature && g_nvmlDeviceGetTemperature(device, 0, &temp) == NVML_SUCCESS) gpu.temp_c = (float)temp;
        unsigned int power_mw = 0;
        if (g_nvmlDeviceGetPowerUsage && g_nvmlDeviceGetPowerUsage(device, &power_mw) == NVML_SUCCESS) gpu.power_w = (float)(power_mw / 1000.0);

        out.push_back(gpu);
    }
}

void HardwareMonitor::collect_apple_metrics(std::vector<GPUMetrics>& out) {
#ifdef __APPLE__
    if (!metal_available_) return;
    GPUMetrics gpu; gpu.backend = "Metal";

    char chip_name[128] = {0}; size_t len = sizeof(chip_name);
    bool is_apple_silicon = false;
    if (sysctlbyname("machdep.cpu.brand_string", chip_name, &len, NULL, 0) == 0) {
        gpu.name = chip_name;
        if (std::string(chip_name).find("Apple") != std::string::npos) is_apple_silicon = true;
    } else {
        gpu.name = "Apple GPU";
    }

    if (is_apple_silicon) {
        int64_t memsize = 0; size_t mlen = sizeof(memsize);
        int mib[2] = {CTL_HW, HW_MEMSIZE};
        if (sysctl(mib, 2, &memsize, &mlen, NULL, 0) == 0) gpu.vram_total_gb = (float)(memsize / (1024.0 * 1024.0 * 1024.0));
        
        mach_msg_type_number_t count = HOST_VM_INFO64_COUNT;
        vm_statistics64_data_t vmstat;
        if (host_statistics64(mach_host_self(), HOST_VM_INFO64, (host_info64_t)&vmstat, &count) == KERN_SUCCESS) {
            vm_size_t page_size; host_page_size(mach_host_self(), &page_size);
            unsigned long long used = (vmstat.active_count + vmstat.wire_count + vmstat.compressor_page_count) * page_size;
            gpu.vram_used_gb = (float)(used / (1024.0 * 1024.0 * 1024.0));
        }
        
        gpu.gpu_util_pct = apple_gpu_utilization_pct();
    } else {
        gpu.name = "Intel Mac GPU";
        io_service_t acc = IOServiceGetMatchingService(kIOMainPortDefault, IOServiceMatching("IOAccelerator"));
        if (acc) {
            CFNumberRef tot = (CFNumberRef)IORegistryEntryCreateCFProperty(acc, CFSTR("VRAM,totalMB"), kCFAllocatorDefault, kNilOptions);
            if (tot) { int mb = 0; CFNumberGetValue(tot, kCFNumberIntType, &mb); gpu.vram_total_gb = mb / 1024.0f; CFRelease(tot); }
            gpu.gpu_util_pct = apple_gpu_utilization_pct();
            IOObjectRelease(acc);
        }
    }
    
    gpu.temp_c = 0.0f;
    gpu.power_w = 0.0f;
    
    out.push_back(gpu);
#endif
}

void HardwareMonitor::collect_amd_metrics(std::vector<GPUMetrics>& out) {
    if (!rocm_available_ || !g_rsmi_num_devices) return;
    uint32_t count = 0;
    if (g_rsmi_num_devices(&count) != 0) return;

    for (uint32_t i = 0; i < count; i++) {
        GPUMetrics gpu; gpu.backend = "ROCm";
        char name[128] = {0};
        if (g_rsmi_dev_name && g_rsmi_dev_name(i, name, sizeof(name)) == 0) gpu.name = name;
        else gpu.name = "AMD GPU " + std::to_string(i);

        uint64_t total = 0, used = 0;
        if (g_rsmi_mem_total && g_rsmi_mem_total(i, 0, &total) == 0) gpu.vram_total_gb = (float)(total / (1024.0 * 1024.0 * 1024.0));
        if (g_rsmi_mem_used && g_rsmi_mem_used(i, 0, &used) == 0) gpu.vram_used_gb = (float)(used / (1024.0 * 1024.0 * 1024.0));
        
        uint32_t busy = 0;
        if (g_rsmi_busy && g_rsmi_busy(i, &busy) == 0) gpu.gpu_util_pct = (int)busy;
        
        int64_t temp = 0;
        if (g_rsmi_temp && g_rsmi_temp(i, 0, &temp) == 0) gpu.temp_c = (float)(temp / 1000.0);
        
        uint64_t power = 0;
        if (g_rsmi_power && g_rsmi_power(i, 0, &power) == 0) gpu.power_w = (float)(power / 1000000.0);

        out.push_back(gpu);
    }
}

// ============================================================
// DHATS Brain: Live Available VRAM & Offload Planner
// ============================================================

float HardwareMonitor::gpu_available_gb() const {
    float avail = 0.0f;
    if (nvml_available_ && g_nvmlDeviceGetCount) {                 // NVIDIA: real free VRAM
        unsigned int c = 0;
        if (g_nvmlDeviceGetCount(&c) == NVML_SUCCESS)
            for (unsigned int i = 0; i < c; i++) {
                void* d = nullptr;
                if (g_nvmlDeviceGetHandle(i, &d) == NVML_SUCCESS) {
                    nvmlMemory_t m;
                    if (g_nvmlDeviceGetMemoryInfo(d, &m) == NVML_SUCCESS)
                        avail += (float)((m.total - m.used) / (1024.0 * 1024.0 * 1024.0));
                }
            }
        return avail;
    }
    if (rocm_available_ && g_rsmi_num_devices) {                   // AMD: real free VRAM
        uint32_t c = 0;
        if (g_rsmi_num_devices(&c) == 0)
            for (uint32_t i = 0; i < c; i++) {
                uint64_t t = 0, u = 0;
                if (g_rsmi_mem_total && g_rsmi_mem_total(i, 0, &t) == 0 &&
                    g_rsmi_mem_used  && g_rsmi_mem_used (i, 0, &u) == 0)
                    avail += (float)((t - u) / (1024.0 * 1024.0 * 1024.0));
            }
        return avail;
    }
#ifdef __APPLE__
    if (metal_available_) {
        int64_t memsize = 0; size_t len = sizeof(memsize);
        int mib[2] = {CTL_HW, HW_MEMSIZE};
        if (sysctl(mib, 2, &memsize, &len, NULL, 0) != 0) return 0.0f;
        float total = (float)(memsize / (1024.0 * 1024.0 * 1024.0));

        mach_msg_type_number_t cnt = HOST_VM_INFO64_COUNT;
        vm_statistics64_data_t vm;
        if (host_statistics64(mach_host_self(), HOST_VM_INFO64,
                              (host_info64_t)&vm, &cnt) != KERN_SUCCESS) return 0.0f;
        vm_size_t ps; host_page_size(mach_host_self(), &ps);

        // Reclaimable memory = what the GPU can actually claim right now
        float reclaimable = (float)((vm.free_count + vm.inactive_count +
                                      vm.purgeable_count) * (uint64_t)ps
                                    / (1024.0 * 1024.0 * 1024.0));
        float cap = total * 0.7f;                 // Metal recommended working-set cap
        float avail = std::min(reclaimable, cap) - 0.5f;   // small driver headroom
        return avail > 0.0f ? avail : 0.0f;
    }
#endif
    return 0.0f;
}

float HardwareMonitor::gpu_budget_gb() const { return gpu_available_gb(); } // budget == live free VRAM

OffloadPlan HardwareMonitor::plan_offload(long long model_size_bytes, int n_layers, int ctx_size) const {
    OffloadPlan p;
    if (n_layers <= 0) n_layers = 32;
    if (!has_gpu()) { p.cpu_only = true; return p; }

    const float headroom = 1.0f; // Safety margin for OS / UI / Metal driver
    float free_vram = gpu_available_gb();        // already accounts for other apps/tasks
    p.budget_gb    = free_vram;
    p.available_gb = free_vram - headroom;

    float model_gb = (float)(model_size_bytes / (1024.0 * 1024.0 * 1024.0));
    
    // Realistic KV-cache estimate: ~4 bytes per token per layer (fp16, 1024 KV-dim)
    float kv_gb = (4.0f * (float)ctx_size * 1024.0f * (float)n_layers) / (1024.0f * 1024.0f * 1024.0f);

    // Adaptive batch sizing: larger batches need more Metal scratch memory
    struct BS { int ubatch, batch; float scratch; };
    const BS opt[3] = {{512, 1024, 0.3f}, {1024, 2048, 0.6f}, {2048, 4096, 1.2f}};
    const BS* bs = &opt[0];
    
    if (p.available_gb - kv_gb - opt[2].scratch >= model_gb + 0.5f) bs = &opt[2];
    else if (p.available_gb - kv_gb - opt[1].scratch >= model_gb + 0.25f) bs = &opt[1];
    
    p.ubatch = bs->ubatch;
    p.batch = bs->batch;

    float weights_room = p.available_gb - kv_gb - bs->scratch;
    if (weights_room <= 0.0f) { p.cpu_only = true; p.ngl = 0; return p; }
    if (weights_room >= model_gb) { p.all_layers = true; p.ngl = 999; return p; }

    float per_layer = model_gb / (float)n_layers;
    p.ngl = std::max(0, std::min(n_layers, (int)(weights_room / per_layer)));
    return p;
}

// ============================================================
// Public API — SINGLE definition, calls all collectors
// ============================================================

HardwareMetrics HardwareMonitor::get_metrics() {
    std::lock_guard<std::mutex> lock(mtx_);
    HardwareMetrics m;
    m.timestamp_ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    m.system_ram_total_gb = collect_system_ram_total_gb();
    m.system_ram_used_gb  = collect_system_ram_used_gb();
    m.cpu_util_pct        = collect_cpu_util_pct();
    m.cpu_temp_c          = collect_cpu_temp_c();
    m.system_power_w      = collect_system_power_w();

    collect_nvidia_metrics(m.gpus);
    collect_apple_metrics(m.gpus);
    collect_amd_metrics(m.gpus);
    return m;
}

int HardwareMonitor::calculate_auto_ngl(long long model_size_bytes, int n_layers, int ctx_size) {
    std::lock_guard<std::mutex> lock(mtx_);
    OffloadPlan p = plan_offload(model_size_bytes, n_layers, ctx_size);
    if (p.cpu_only) return 0;
    return p.all_layers ? -1 : p.ngl;
}

bool HardwareMonitor::has_gpu() const { return nvml_available_ || metal_available_ || rocm_available_; }

std::string HardwareMonitor::get_primary_backend() const {
    if (nvml_available_) return "CUDA (NVIDIA)";
    if (metal_available_) return "Metal (Apple)";
    if (rocm_available_) return "ROCm (AMD)";
    return "CPU";
}

OffloadPlan HardwareMonitor::plan_tiered_offload(long long model_size_bytes, int n_layers, int ctx_size) const {
    OffloadPlan p;
    if (n_layers <= 0) n_layers = 32;
    
    float model_gb = (float)(model_size_bytes / (1024.0 * 1024.0 * 1024.0));
    float kv_gb = (4.0f * (float)ctx_size * 1024.0f * (float)n_layers) / (1024.0 * 1024.0 * 1024.0f);
    
    // Get available resources
    float gpu_free = gpu_available_gb();
    float ram_total = const_cast<HardwareMonitor*>(this)->collect_system_ram_total_gb();
    float ram_used = const_cast<HardwareMonitor*>(this)->collect_system_ram_used_gb();
    float ram_free = ram_total - ram_used - 2.0f; // 2GB headroom for OS
    
    // Adaptive batch sizing
    struct BS { int ubatch, batch; float scratch; };
    const BS opt[3] = {{512, 1024, 0.3f}, {1024, 2048, 0.6f}, {2048, 4096, 1.2f}};
    
    // Tier 1: Can everything fit in GPU?
    for (int i = 2; i >= 0; i--) {
        float gpu_needed = model_gb + kv_gb + opt[i].scratch;
        if (gpu_needed <= gpu_free) {
            p.gpu_layers = n_layers;
            p.cpu_layers = 0;
            p.gpu_mem_needed = gpu_needed;
            p.cpu_mem_needed = 0;
            p.ngl = 999;
            p.all_layers = true;
            p.batch = opt[i].batch;
            p.ubatch = opt[i].ubatch;
            p.budget_gb = gpu_free;
            p.available_gb = gpu_free - gpu_needed;
            p.efficient = true;
            return p;
        }
    }
    
    // Tier 2: Split between GPU and CPU/RAM
    if (gpu_free > 0 && has_gpu()) {
        // Calculate optimal split: put as many layers on GPU as possible
        float per_layer = model_gb / (float)n_layers;
        float gpu_for_layers = gpu_free - kv_gb - 0.3f; // Reserve for KV + scratch
        int layers_on_gpu = std::max(0, std::min(n_layers, (int)(gpu_for_layers / per_layer)));
        int layers_on_cpu = n_layers - layers_on_gpu;
        
        float gpu_mem = (per_layer * layers_on_gpu) + kv_gb + 0.3f;
        float cpu_mem = per_layer * layers_on_cpu;
        
        // Check if CPU has enough RAM
        if (cpu_mem <= ram_free) {
            p.gpu_layers = layers_on_gpu;
            p.cpu_layers = layers_on_cpu;
            p.gpu_mem_needed = gpu_mem;
            p.cpu_mem_needed = cpu_mem;
            p.ngl = layers_on_gpu;
            p.all_layers = false;
            p.batch = 512; // Smaller batch for split offload
            p.ubatch = 512;
            p.budget_gb = gpu_free;
            p.available_gb = gpu_free - gpu_mem;
            
            // Efficiency warning: split offload is slower
            if (layers_on_cpu > n_layers / 2) {
                p.efficient = false;
                p.efficiency_warning = "Most layers on CPU — generation will be slow. " +
                    std::to_string(layers_on_gpu) + " of " + std::to_string(n_layers) + 
                    " layers on GPU.";
                p.recommendation = "Use a smaller model or quantization (Q4/Q5) for better speed.";
            } else {
                p.efficient = true;
                p.efficiency_warning = "";
                p.recommendation = "";
            }
            return p;
        }
    }
    
    // Tier 3: CPU-only fallback
    if (model_gb + kv_gb <= ram_free) {
        p.gpu_layers = 0;
        p.cpu_layers = n_layers;
        p.gpu_mem_needed = 0;
        p.cpu_mem_needed = model_gb + kv_gb;
        p.ngl = 0;
        p.all_layers = false;
        p.cpu_only = true;
        p.batch = 512;
        p.ubatch = 512;
        p.efficient = false;
        p.efficiency_warning = "Running entirely on CPU — will be very slow.";
        p.recommendation = "Use a smaller quantized model (Q4_K_M or Q5_K_M) for acceptable CPU speed.";
        return p;
    }
    
    // Cannot fit anywhere
    p.gpu_layers = 0;
    p.cpu_layers = 0;
    p.ngl = 0;
    p.cpu_only = true;
    p.efficient = false;
    p.efficiency_warning = "Model too large for available memory. Needs " + 
        std::to_string((int)model_gb) + "GB but only " + 
        std::to_string((int)gpu_free) + "GB GPU + " + 
        std::to_string((int)ram_free) + "GB RAM available.";
    p.recommendation = "Use a smaller model or lower quantization. Try models under " + 
        std::to_string((int)(gpu_free + ram_free - 2.0f)) + "GB.";
    return p;
}

bool HardwareMonitor::can_run_efficiently(long long model_size_bytes, int n_layers, int ctx_size) const {
    OffloadPlan p = plan_tiered_offload(model_size_bytes, n_layers, ctx_size);
    return p.efficient && (p.gpu_layers > 0 || !p.cpu_only);
}

std::vector<std::string> HardwareMonitor::get_recommended_model_sizes() const {
    std::vector<std::string> recs;
    float gpu_free = gpu_available_gb();
    float ram_total = const_cast<HardwareMonitor*>(this)->collect_system_ram_total_gb();
    float ram_used = const_cast<HardwareMonitor*>(this)->collect_system_ram_used_gb();
    float ram_free = ram_total - ram_used - 2.0f;
    
    float total_budget = gpu_free + ram_free;
    
    if (gpu_free > 0) {
        if (gpu_free >= 8.0f) recs.push_back("7B models (Q4_K_M, Q5_K_M)");
        if (gpu_free >= 16.0f) recs.push_back("13B models (Q4_K_M)");
        if (gpu_free >= 24.0f) recs.push_back("30B models (Q4_K_M)");
        if (gpu_free >= 40.0f) recs.push_back("65B models (Q4_K_M)");
    }
    
    if (recs.empty() && total_budget > 0) {
        if (total_budget >= 8.0f) recs.push_back("3B models (Q4_K_M) for CPU");
        if (total_budget >= 12.0f) recs.push_back("7B models (Q4_K_M) for CPU");
    }
    
    if (recs.empty()) {
        recs.push_back("Use quantized models under " + std::to_string((int)total_budget) + "GB");
    }
    
    return recs;
}

} // namespace delta