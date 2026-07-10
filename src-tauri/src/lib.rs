use tauri::Manager;
use tauri::ipc::Channel;
use tauri_plugin_shell::ShellExt;
use tauri_plugin_shell::process::CommandChild;
use std::sync::Mutex;
use std::sync::atomic::{AtomicBool, Ordering};
use std::sync::Arc;
use std::collections::HashMap;

struct ServerState {
    child: Option<CommandChild>,
    port: u16,
}

#[tauri::command]
fn get_server_port(state: tauri::State<'_, Mutex<ServerState>>) -> u16 {
    state.lock().unwrap().port
}

struct StreamAbortFlags {
    flags: Mutex<HashMap<String, Arc<AtomicBool>>>,
}

#[tauri::command]
async fn stream_chat(
    state: tauri::State<'_, StreamAbortFlags>,
    stream_id: String,
    url: String,
    headers: HashMap<String, String>,
    body: String,
    on_chunk: Channel<String>,
) -> Result<u16, String> {
    let abort_flag = Arc::new(AtomicBool::new(false));
    {
        state.flags.lock().unwrap().insert(stream_id.clone(), abort_flag.clone());
    }

    let abort = abort_flag.clone();
    let result = tokio::task::spawn_blocking(move || -> Result<u16, String> {
        let agent = ureq::AgentBuilder::new()
            .timeout_read(std::time::Duration::from_secs(180))
            .build();
        let mut req = agent.post(&url);
        for (key, value) in &headers {
            req = req.set(key, value);
        }

        match req.send_string(&body) {
            Ok(response) => {
                let status = response.status();
                use std::io::BufRead;
                let reader = std::io::BufReader::new(response.into_reader());
                for line_result in reader.lines() {
                    if abort.load(Ordering::Relaxed) {
                        break;
                    }
                    match line_result {
                        Ok(line) => {
                            if on_chunk.send(line).is_err() {
                                break;
                            }
                        }
                        Err(ref e) if e.kind() == std::io::ErrorKind::TimedOut => {
                            if abort.load(Ordering::Relaxed) {
                                break;
                            }
                            continue;
                        }
                        Err(_) => break,
                    }
                }
                Ok(status)
            }
            Err(ureq::Error::Status(code, response)) => {
                let body = response.into_string().unwrap_or_default();
                Err(format!("HTTP {}: {}", code, body))
            }
            Err(e) => Err(format!("Connection error: {}", e)),
        }
    })
    .await;

    state.flags.lock().unwrap().remove(&stream_id);
    result.map_err(|_| "Stream task panicked".to_string())?
}

#[tauri::command]
fn abort_stream(state: tauri::State<'_, StreamAbortFlags>, stream_id: String) {
    if let Some(flag) = state.flags.lock().unwrap().get(&stream_id) {
        flag.store(true, Ordering::Relaxed);
    }
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

fn wait_for_server(port: u16) -> bool {
    for _ in 0..120 {
        if let Ok(mut stream) = std::net::TcpStream::connect(("127.0.0.1", port)) {
            use std::io::{Read, Write};
            let _ = stream.write_all(b"GET /props HTTP/1.0\r\nHost: localhost\r\n\r\n");
            let mut buf = [0u8; 64];
            let _ = stream.read(&mut buf);
            return true;
        }
        std::thread::sleep(std::time::Duration::from_millis(250));
    }
    false
}

#[cfg(target_os = "windows")]
fn kill_stale_server_processes() {
    use std::process::Command;
    let _ = Command::new("taskkill").args(["/F", "/FI", "IMAGENAME eq llama-server*"]).output();
    let _ = Command::new("taskkill").args(["/F", "/FI", "IMAGENAME eq delta-server*"]).output();
    std::thread::sleep(std::time::Duration::from_millis(500));
}

#[cfg(not(target_os = "windows"))]
fn kill_stale_server_processes() {
    use std::process::Command;
    let _ = Command::new("pkill").args(["-9", "llama-server"]).output();
    let _ = Command::new("pkill").args(["-9", "delta-server"]).output();
    std::thread::sleep(std::time::Duration::from_millis(500));
}

pub fn run() {
    kill_stale_server_processes();

    tauri::Builder::default()
        .plugin(tauri_plugin_shell::init())
        .plugin(tauri_plugin_process::init())
        .manage(Mutex::new(ServerState {
            child: None,
            port: 8080,
        }))
        .manage(StreamAbortFlags {
            flags: Mutex::new(HashMap::new()),
        })
        .invoke_handler(tauri::generate_handler![get_server_port, stream_chat, abort_stream])
        .setup(|app| {
            let app_handle = app.handle().clone();

            let server_port = find_available_port(8080);
            let model_api_port = find_available_port(server_port + 1);

            {
                let state = app.state::<Mutex<ServerState>>();
                let mut s = state.lock().unwrap();
                s.port = server_port;
            }

            let models_dir = get_models_dir();
            let _ = std::fs::create_dir_all(&models_dir);

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

            let port = server_port;
            let mapi_port = model_api_port;
            std::thread::spawn(move || {
                if wait_for_server(port) {
                    if let Some(window) = app_handle.get_webview_window("main") {
                        let js = format!(
                            "window.__DELTA_PORT__={};window.__DELTA_MODEL_API_PORT__={};window.dispatchEvent(new CustomEvent('delta-server-ready'))",
                            port, mapi_port
                        );
                        let _ = window.eval(&js);
                    }
                } else {
                    log::warn!("Server did not respond within timeout");
                    if let Some(window) = app_handle.get_webview_window("main") {
                        let js = format!(
                            "window.__DELTA_PORT__={};window.__DELTA_MODEL_API_PORT__={};window.dispatchEvent(new CustomEvent('delta-server-error'))",
                            port, mapi_port
                        );
                        let _ = window.eval(&js);
                    }
                }
            });

            Ok(())
        })
        .on_window_event(|window, event| {
            if let tauri::WindowEvent::CloseRequested { .. } = event {
                let app = window.app_handle();
                let state = app.state::<Mutex<ServerState>>();
                let child = {
                    let mut s = state.lock().unwrap();
                    s.child.take()
                };
                if let Some(child) = child {
                    log::info!("Shutting down delta-server...");
                    let _ = child.kill();
                }
                kill_stale_server_processes();
            }
        })
        .build(tauri::generate_context!())
        .expect("error while building Delta")
        .run(|app_handle, event| {
            if let tauri::RunEvent::Exit = event {
                let state = app_handle.state::<Mutex<ServerState>>();
                if let Ok(mut s) = state.lock() {
                    if let Some(child) = s.child.take() {
                        let _ = child.kill();
                    }
                }
                kill_stale_server_processes();
            }
        });
}
