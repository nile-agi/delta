// src-tauri/src/commands.rs
use serde::{Deserialize, Serialize};
use sysinfo::{System, RefreshKind, CpuRefreshKind, MemoryRefreshKind};
use std::time::Duration;

#[derive(Serialize, Deserialize, Clone)]
pub struct SystemStats {
    pub os: OsInfo,
    pub cpu: CpuInfo,
    pub memory: MemoryInfo,
    pub gpu: GpuInfo,
}

#[derive(Serialize, Deserialize, Clone)]
pub struct OsInfo {
    pub name: String,
    pub version: String,
}

#[derive(Serialize, Deserialize, Clone)]
pub struct CpuInfo {
    pub model: String,
    pub arch: String,
    pub cores: usize,
    pub usage: f32,
}

#[derive(Serialize, Deserialize, Clone)]
pub struct MemoryInfo {
    pub total: u64,
    pub available: u64,
    pub usage: f32,
}

#[derive(Serialize, Deserialize, Clone)]
pub struct GpuInfo {
    pub name: String,
    pub usage: f32,
    pub total_memory: u64,
    pub available_memory: u64,
}

#[tauri::command]
pub async fn get_system_stats() -> Result<SystemStats, String> {
    tokio::task::spawn_blocking(|| {
        let mut sys = System::new_with_specifics(
            RefreshKind::new()
                .with_cpu(CpuRefreshKind::new().with_cpu_usage())
                .with_memory(MemoryRefreshKind::everything())
        );

        // Wait 250ms so sysinfo can measure CPU usage over time
        std::thread::sleep(Duration::from_millis(250));

        sys.refresh_specifics(
            RefreshKind::new()
                .with_cpu(CpuRefreshKind::new().with_cpu_usage())
                .with_memory(MemoryRefreshKind::everything())
        );

        // Cosmetic: show "macOS" instead of the kernel name "Darwin"
        let mut os_name = System::name().unwrap_or_else(|| "Unknown OS".to_string());
        if os_name == "Darwin" {
            os_name = "macOS".to_string();
        }
        let os_version = System::os_version().unwrap_or_else(|| "Unknown".to_string());
        let arch = System::cpu_arch().unwrap_or_else(|| "Unknown".to_string());
        let cores = sys.physical_core_count().unwrap_or(0);

        // ✅ FIXED: use used_memory() — the metric sysinfo actually implements
        // on macOS (active + wired + compressed). available_memory() returns 0
        // on macOS in this sysinfo version, which caused the fake 100%.
        let total_ram = sys.total_memory();
        let used_ram = sys.used_memory();
        let available_ram = total_ram.saturating_sub(used_ram);
        let ram_usage = if total_ram > 0 {
            (used_ram as f32 / total_ram as f32) * 100.0
        } else {
            0.0
        };

        let cpu_model = sys.cpus().first()
            .map(|c| c.brand().to_string())
            .unwrap_or_else(|| "Unknown CPU".to_string());
        let cpu_usage = sys.global_cpu_usage();

        Ok(SystemStats {
            os: OsInfo { name: os_name, version: os_version },
            cpu: CpuInfo { model: cpu_model, arch, cores, usage: cpu_usage },
            memory: MemoryInfo { total: total_ram, available: available_ram, usage: ram_usage },
            gpu: GpuInfo {
                name: "Standard Output".to_string(),
                usage: 0.0,
                total_memory: 0,
                available_memory: 0,
            },
        })
    })
    .await
    .unwrap_or_else(|e| Err(format!("Task panicked: {}", e)))
}