use tauri::Manager;
use tauri_plugin_shell::ShellExt;
use tauri_plugin_shell::process::CommandChild;
use std::sync::Mutex;

struct ServerState {
    child: Option<CommandChild>,
    port: u16,
}

#[tauri::command]
fn get_server_port(state: tauri::State<'_, Mutex<ServerState>>) -> u16 {
    state.lock().unwrap().port
}

fn find_available_port(start: u16) -> u16 {
    for port in start..start + 100 {
        if std::net::TcpListener::bind(("127.0.0.1", port)).is_ok() {
            return port;
        }
    }
    start
}

fn get_models_dir() -> String {
    let home = dirs_next().unwrap_or_else(|| std::path::PathBuf::from("."));
    home.join(".delta-cli").join("models").to_string_lossy().to_string()
}

fn dirs_next() -> Option<std::path::PathBuf> {
    #[cfg(target_os = "macos")]
    {
        std::env::var("HOME").ok().map(std::path::PathBuf::from)
    }
    #[cfg(target_os = "windows")]
    {
        std::env::var("USERPROFILE").ok().map(std::path::PathBuf::from)
    }
    #[cfg(target_os = "linux")]
    {
        std::env::var("HOME").ok().map(std::path::PathBuf::from)
    }
}

pub fn run() {
    tauri::Builder::default()
        .plugin(tauri_plugin_shell::init())
        .plugin(tauri_plugin_process::init())
        .manage(Mutex::new(ServerState {
            child: None,
            port: 8080,
        }))
        .invoke_handler(tauri::generate_handler![get_server_port])
        .setup(|app| {
            let app_handle = app.handle().clone();

            // Find available ports for the servers
            let server_port = find_available_port(8080);
            let model_api_port = find_available_port(8081);

            // Update stored port
            {
                let state = app.state::<Mutex<ServerState>>();
                let mut s = state.lock().unwrap();
                s.port = server_port;
            }

            let models_dir = get_models_dir();

            // Ensure models directory exists
            let _ = std::fs::create_dir_all(&models_dir);

            // Spawn delta-server sidecar
            let shell = app_handle.shell();
            let sidecar_cmd = shell
                .sidecar("delta-server")
                .expect("failed to create delta-server sidecar command")
                .args([
                    "--port",
                    &server_port.to_string(),
                    "--models-dir",
                    &models_dir,
                    "--model-api-port",
                    &model_api_port.to_string(),
                ]);

            match sidecar_cmd.spawn() {
                Ok((mut _rx, child)) => {
                    log::info!(
                        "delta-server started on port {} (model API on {})",
                        server_port,
                        model_api_port
                    );
                    let state = app.state::<Mutex<ServerState>>();
                    let mut s = state.lock().unwrap();
                    s.child = Some(child);
                }
                Err(e) => {
                    log::error!("Failed to start delta-server: {}", e);
                }
            }

            Ok(())
        })
        .on_window_event(|window, event| {
            if let tauri::WindowEvent::CloseRequested { .. } = event {
                let app = window.app_handle();
                let state = app.state::<Mutex<ServerState>>();
                let mut s = state.lock().unwrap();

                // Kill sidecar process (delta-server handles killing llama-server internally)
                if let Some(child) = s.child.take() {
                    log::info!("Shutting down delta-server...");
                    let _ = child.kill();
                }
            }
        })
        .run(tauri::generate_context!())
        .expect("error while running Delta");
}
