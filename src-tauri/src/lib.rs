mod diagnostics;
mod startup;
use diagnostics::DiagnosticState;

use std::collections::HashMap;
use std::sync::atomic::{AtomicBool, Ordering};
use std::sync::{Arc, Mutex};
use tauri::ipc::Channel;
use tauri::{Emitter, Manager};
use tauri_plugin_shell::process::{CommandChild, CommandEvent};
use tauri_plugin_shell::ShellExt;

struct ServerState {
    child: Option<CommandChild>,
    port: u16,
    model_api_port: u16,
    ready: bool,
    error: bool,
    stopping: bool,
}

type BackendDiagnostic = Arc<Mutex<Option<String>>>;

#[tauri::command]
async fn get_diagnostic_report(
    window: tauri::Window,
    state: tauri::State<'_, DiagnosticState>,
    stage: String,
    model: Option<String>,
    error: Option<String>,
    exit_code: Option<i64>,
) -> Result<Option<diagnostics::Report>, String> {
    if window.label() != "main" {
        return Err("Reports are available only in the main window".into());
    }
    if ![
        "startup",
        "startup-timeout",
        "backend",
        "frontend",
        "model-load",
        "previous",
    ]
    .contains(&stage.as_str())
    {
        return Err("Unknown failure stage".into());
    }
    let store = state.inner().clone();
    tauri::async_runtime::spawn_blocking(move || {
        let mut store = store.lock().unwrap_or_else(|e| e.into_inner());
        if stage == "previous" {
            store.previous()
        } else {
            Some(store.snapshot(
                &stage,
                model,
                &error.unwrap_or_else(|| "Backend unavailable".into()),
                exit_code,
            ))
        }
    })
    .await
    .map_err(|e| e.to_string())
}

#[tauri::command]
fn get_server_port(state: tauri::State<'_, Mutex<ServerState>>) -> u16 {
    state.lock().unwrap_or_else(|e| e.into_inner()).port
}

#[tauri::command]
fn get_server_status(
    state: tauri::State<'_, Mutex<ServerState>>,
    backend_diagnostic: tauri::State<'_, BackendDiagnostic>,
) -> (u16, u16, bool, bool, String) {
    let s = state.lock().unwrap_or_else(|e| e.into_inner());
    let diagnostic = backend_diagnostic
        .lock()
        .unwrap_or_else(|e| e.into_inner())
        .clone()
        .unwrap_or_default();
    (s.port, s.model_api_port, s.ready, s.error, diagnostic)
}

fn startup_info(message: &str) {
    log::info!("{}", message);
    eprintln!("[Delta] {}", message);
}

fn startup_warn(message: &str) {
    log::warn!("{}", message);
    eprintln!("[Delta] WARN: {}", message);
}

fn startup_error(message: &str) {
    log::error!("{}", message);
    eprintln!("[Delta] ERROR: {}", message);
}

fn classify_backend_diagnostic(line: &str) -> Option<String> {
    let normalized = line.to_ascii_lowercase();
    let is_fatal = normalized.contains("could not create default egl display")
        || normalized.contains("egl_bad_parameter")
        || normalized.contains("llama-server aborted")
        || normalized.contains("failed to start server")
        || normalized.contains("llama-server exited");

    if is_fatal {
        Some(line.trim().chars().take(240).collect())
    } else {
        None
    }
}

fn log_sidecar_events(
    mut rx: tauri::async_runtime::Receiver<CommandEvent>,
    backend_diagnostic: BackendDiagnostic,
    app_handle: tauri::AppHandle,
) {
    std::thread::spawn(move || {
        while let Some(event) = rx.blocking_recv() {
            match event {
                CommandEvent::Stdout(bytes) => {
                    let line = String::from_utf8_lossy(&bytes);
                    let trimmed = line.trim();
                    if !trimmed.is_empty() {
                        app_handle
                            .state::<DiagnosticState>()
                            .lock()
                            .unwrap_or_else(|e| e.into_inner())
                            .append(trimmed);
                        startup_info(&format!("delta-server stdout: {}", trimmed));
                    }
                }
                CommandEvent::Stderr(bytes) => {
                    let line = String::from_utf8_lossy(&bytes);
                    let trimmed = line.trim();
                    if !trimmed.is_empty() {
                        app_handle
                            .state::<DiagnosticState>()
                            .lock()
                            .unwrap_or_else(|e| e.into_inner())
                            .append(trimmed);
                        startup_warn(&format!("delta-server stderr: {}", trimmed));
                        if let Some(diagnostic) = classify_backend_diagnostic(trimmed) {
                            let mut current =
                                backend_diagnostic.lock().unwrap_or_else(|e| e.into_inner());
                            if current.is_none() {
                                *current = Some(diagnostic);
                            }
                        }
                    }
                }
                CommandEvent::Error(error) => {
                    report_backend_exit(
                        &app_handle,
                        &backend_diagnostic,
                        format!("Backend process error: {}", error),
                    );
                }
                CommandEvent::Terminated(payload) => {
                    {
                        let store = app_handle.state::<DiagnosticState>();
                        let mut store = store.lock().unwrap_or_else(|e| e.into_inner());
                        store.exit_code = payload.code.map(i64::from);
                        store.signal = payload.signal;
                    }
                    report_backend_exit(
                        &app_handle,
                        &backend_diagnostic,
                        format!(
                            "Backend exited (code: {:?}, signal: {:?})",
                            payload.code, payload.signal
                        ),
                    );
                }
                _ => {}
            }
        }
    });
}

fn report_backend_exit(app: &tauri::AppHandle, diagnostic: &BackendDiagnostic, reason: String) {
    {
        let state = app.state::<Mutex<ServerState>>();
        let mut state = state.lock().unwrap_or_else(|e| e.into_inner());
        if state.stopping {
            return;
        }
        state.ready = false;
        state.error = true;
    }
    {
        let store = app.state::<DiagnosticState>();
        let mut store = store.lock().unwrap_or_else(|e| e.into_inner());
        store.append(&reason);
        store.snapshot("backend", None, &reason, None);
    }
    startup_error(&reason);
    *diagnostic.lock().unwrap_or_else(|e| e.into_inner()) = Some(reason.clone());
    let _ = app.emit(
        "delta-server-error",
        serde_json::json!({ "reason": reason }),
    );
    if let Some(window) = app.get_webview_window("main") {
        let reason_json = serde_json::to_string(&reason).unwrap_or_default();
        let _ = window.eval(&format!("window.__DELTA_MODEL_BACKEND_ERROR__={};window.__DELTA_SERVER_ERROR__=true;window.dispatchEvent(new CustomEvent('delta-server-error'))", reason_json));
    }
}

#[cfg(test)]
mod tests {
    use super::classify_backend_diagnostic;

    #[test]
    fn captures_fatal_graphics_startup_errors() {
        assert_eq!(
            classify_backend_diagnostic(
                "Could not create default EGL display: EGL_BAD_PARAMETER. Aborting..."
            ),
            Some(
                "Could not create default EGL display: EGL_BAD_PARAMETER. Aborting...".to_string()
            )
        );
    }

    #[test]
    fn ignores_non_fatal_sidecar_warnings() {
        assert_eq!(classify_backend_diagnostic("schema version: 4"), None);
    }
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
        state
            .flags
            .lock()
            .unwrap()
            .insert(stream_id.clone(), abort_flag.clone());
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

    state
        .flags
        .lock()
        .unwrap_or_else(|e| e.into_inner())
        .remove(&stream_id);
    result.map_err(|_| "Stream task panicked".to_string())?
}

#[tauri::command]
fn abort_stream(state: tauri::State<'_, StreamAbortFlags>, stream_id: String) {
    if let Some(flag) = state
        .flags
        .lock()
        .unwrap_or_else(|e| e.into_inner())
        .get(&stream_id)
    {
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
    home.join(".delta-cli")
        .join("models")
        .to_string_lossy()
        .to_string()
}

fn dirs_next() -> Option<std::path::PathBuf> {
    #[cfg(target_os = "macos")]
    {
        std::env::var("HOME").ok().map(std::path::PathBuf::from)
    }
    #[cfg(target_os = "windows")]
    {
        std::env::var("USERPROFILE")
            .ok()
            .map(std::path::PathBuf::from)
    }
    #[cfg(target_os = "linux")]
    {
        std::env::var("HOME").ok().map(std::path::PathBuf::from)
    }
}

pub fn run() {
    let diagnostics = DiagnosticState::default();
    let panic_diagnostics = diagnostics.clone();
    let original_panic_hook = std::panic::take_hook();
    std::panic::set_hook(Box::new(move |info| {
        if let Ok(mut store) = panic_diagnostics.try_lock() {
            let location = info
                .location()
                .map(|l| format!("{}:{}", l.file(), l.line()))
                .unwrap_or_default();
            store.snapshot(
                "native",
                None,
                &format!("Native application panic at {}", location),
                None,
            );
        }
        original_panic_hook(info);
    }));

    tauri::Builder::default()
        .plugin(tauri_plugin_shell::init())
        .plugin(tauri_plugin_process::init())
        .plugin(tauri_plugin_notification::init())
        .manage(Mutex::new(ServerState {
            child: None,
            port: 8080,
            model_api_port: 8081,
            ready: false,
            error: false,
            stopping: false,
        }))
        .manage(BackendDiagnostic::default())
        .manage(diagnostics.clone())
        .manage(StreamAbortFlags {
            flags: Mutex::new(HashMap::new()),
        })
        .invoke_handler(tauri::generate_handler![get_server_port, get_server_status, stream_chat, abort_stream, get_diagnostic_report])
        .setup(|app| {
            let app_handle = app.handle().clone();
            if let Ok(directory) = app.path().app_log_dir() {
                app.state::<DiagnosticState>().lock().unwrap_or_else(|e| e.into_inner()).set_path(directory.join("last-failure.json"));
            }

            let server_port = find_available_port(8080);
            let model_api_port = find_available_port(server_port + 1);
            startup_info(&format!(
                "Selected server port {} and model API port {}",
                server_port, model_api_port
            ));

            {
                let state = app.state::<Mutex<ServerState>>();
                let mut s = state.lock().unwrap_or_else(|e| e.into_inner());
                s.port = server_port;
                s.model_api_port = model_api_port;
            }

            let models_dir = get_models_dir();
            match std::fs::create_dir_all(&models_dir) {
                Ok(()) => startup_info(&format!("Using models directory {}", models_dir)),
                Err(e) => startup_error(&format!("Failed to create models directory {}: {}", models_dir, e)),
            }

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

            let sidecar_spawned = match sidecar_cmd.spawn() {
                Ok((rx, child)) => {
                    startup_info(&format!(
                        "Spawned delta-server sidecar on port {} (model API on {})",
                        server_port, model_api_port
                    ));
                    let backend_diagnostic = app.state::<BackendDiagnostic>().inner().clone();
                    log_sidecar_events(rx, backend_diagnostic, app_handle.clone());
                    let state = app.state::<Mutex<ServerState>>();
                    let mut s = state.lock().unwrap_or_else(|e| e.into_inner());
                    s.child = Some(child);
                    true
                }
                Err(e) => {
                    report_backend_exit(&app_handle, &app.state::<BackendDiagnostic>().inner().clone(), format!("Failed to spawn backend: {}", e));
                    false
                }
            };

            let port = server_port;
            let mapi_port = model_api_port;
            std::thread::spawn(move || {
                if !sidecar_spawned {
                    startup_error("Sidecar failed to spawn — notifying frontend immediately");
                    if let Ok(mut s) = app_handle.state::<Mutex<ServerState>>().lock() {
                        s.error = true;
                    }
                    if let Some(window) = app_handle.get_webview_window("main") {
                        let js = format!(
                            "window.__DELTA_PORT__={};window.__DELTA_MODEL_API_PORT__={};window.__DELTA_SERVER_ERROR__=true;window.dispatchEvent(new CustomEvent('delta-server-error'))",
                            port, mapi_port
                        );
                        if let Err(e) = window.eval(&js) {
                            startup_error(&format!("Failed to inject error state: {}", e));
                        }
                    }
                    return;
                }

                // Wait for Model API server (starts quickly, always available)
                // rather than llama-server (may take 30s+ to load a model)
                startup_info(&format!("Waiting for Model API readiness on port {}", mapi_port));

                let outcome = startup::wait_for_server(mapi_port, std::time::Duration::from_secs(30), || {
                    app_handle.state::<Mutex<ServerState>>().lock().map(|s| s.error || s.stopping).unwrap_or(true)
                });
                if outcome == startup::StartupOutcome::Interrupted { return; }
                if outcome == startup::StartupOutcome::Ready {
                    startup_info(&format!("Model API is reachable on port {}", mapi_port));
                    if let Ok(mut s) = app_handle.state::<Mutex<ServerState>>().lock() {
                        if s.error || s.stopping { return; }
                        s.ready = true;
                    }

                    // ✅ Emit Tauri event for the frontend to listen to
                    let _ = app_handle.emit("delta-server-ready", serde_json::json!({
                        "port": port,
                        "modelApiPort": mapi_port
                    }));

                    // Fallback: inject into window if it's already open
                    // ✅ CORRECTED CODE:
                    if let Some(window) = app_handle.get_webview_window("main") {
                        let js = format!(
                            "window.__DELTA_PORT__={};window.__DELTA_MODEL_API_PORT__={};window.dispatchEvent(new CustomEvent('delta-server-ready'))",
                            port, mapi_port
                        );
                        if let Err(e) = window.eval(&js) {
                            startup_error(&format!("Failed to inject ports: {}", e));
                        }
                    }
                } else { // <--- ✅ Fixed: Only one closing brace before `else`
                    report_backend_exit(&app_handle, &app_handle.state::<BackendDiagnostic>().inner().clone(), "Backend did not become ready within 30 seconds".into());

                    if let Ok(mut s) = app_handle.state::<Mutex<ServerState>>().lock() {
                        s.error = true;
                    }
                    if let Some(window) = app_handle.get_webview_window("main") {
                        let js = format!(
                            "window.__DELTA_PORT__={};window.__DELTA_MODEL_API_PORT__={};window.__DELTA_SERVER_ERROR__=true;window.dispatchEvent(new CustomEvent('delta-server-error'))",
                            port, mapi_port
                        );
                        if let Err(e) = window.eval(&js) {
                            startup_error(&format!("Failed to inject error state: {}", e));
                        }
                    }
                }
            });

            Ok(())
        })
        .on_window_event(|window, event| {
            if let tauri::WindowEvent::CloseRequested { .. } = event {
                // Auxiliary windows (e.g. the "hardware-telemetry" OS window) must NOT
                // tear down the sidecar. Only the main window owns the server lifecycle.
                if window.label() != "main" {
                    return;
                }

                // Main window is closing: shut down auxiliary windows first so they
                // don't outlive the server with a dead SSE stream.
                let app = window.app_handle();
                for (label, w) in app.webview_windows() {
                    if label != "main" {
                        let _ = w.close();
                    }
                }

                let state = app.state::<Mutex<ServerState>>();
                let (child, model_api_port) = {
                    let mut s = state.lock().unwrap_or_else(|e| e.into_inner());
                    s.stopping = true;
                    (s.child.take(), s.model_api_port)
                };
                if let Some(child) = child {
                    log::info!("Shutting down delta-server...");
                    startup::unload_backend(model_api_port);
                    let _ = child.kill();
                }
            }
        })
        .build(tauri::generate_context!())
        .expect("error while building Delta")
        .run(|app_handle, event| {
            if let tauri::RunEvent::Exit = event {
                let state = app_handle.state::<Mutex<ServerState>>();
                let owned = state.lock().ok().and_then(|mut s| {
                    s.stopping = true;
                    let port = s.model_api_port;
                    s.child.take().map(|child| (child, port))
                });
                if let Some((child, port)) = owned {
                    startup::unload_backend(port);
                    let _ = child.kill();
                }

            }
        });
}
