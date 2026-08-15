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
    // We use spawn_blocking so the 250ms sleep doesn't freeze the Tauri UI thread pool
    tokio::task::spawn_blocking(|| {
        // 1. FIXED: Changed RefreshKind::nothing() to RefreshKind::new() as your compiler suggested
        let mut sys = System::new_with_specifics(
            RefreshKind::new()
                .with_cpu(CpuRefreshKind::new().with_cpu_usage())
                .with_memory(MemoryRefreshKind::new())
        );
        
        // Wait 250ms to measure CPU usage over time (required by sysinfo for accuracy)
        std::thread::sleep(Duration::from_millis(250));
        
        sys.refresh_specifics(
            RefreshKind::new()
                .with_cpu(CpuRefreshKind::new().with_cpu_usage())
                .with_memory(MemoryRefreshKind::new())
        );

        let os_name = System::long_os_version().unwrap_or_else(|| "Unknown OS".to_string());
        let os_version = System::os_version().unwrap_or_else(|| "Unknown Version".to_string());
        let arch = System::cpu_arch().unwrap_or_else(|| "Unknown Arch".to_string());
        
        // 2. FIXED: Changed System::physical_core_count() to sys.physical_core_count() 
        // because your version of sysinfo defines it as an instance method (&self)
        let cores = sys.physical_core_count().unwrap_or(0);

        let total_ram = sys.total_memory();
        let available_ram = sys.available_memory();
        let ram_usage = if total_ram > 0 {
            ((total_ram - available_ram) as f32 / total_ram as f32) * 100.0
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