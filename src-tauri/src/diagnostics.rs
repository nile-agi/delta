use regex::Regex;
use serde::{Deserialize, Serialize};
use std::collections::VecDeque;
use std::path::PathBuf;
use std::sync::{Arc, Mutex, OnceLock};
use std::time::{SystemTime, UNIX_EPOCH};

pub type DiagnosticState = Arc<Mutex<Diagnostics>>;

pub fn redact(input: &str) -> String {
    static RULES: OnceLock<Vec<Regex>> = OnceLock::new();
    let rules = RULES.get_or_init(|| {
        vec![
            Regex::new(r"(?i)https?://[^\s]+").unwrap(),
            Regex::new(r"\b(?:hf_[A-Za-z0-9]{8,}|sk-[A-Za-z0-9_-]{8,})\b").unwrap(),
            Regex::new(
                r"(?i)\b(?:authorization|bearer|api[_-]?key|token|password|secret)\b[^\r\n]*",
            )
            .unwrap(),
            Regex::new(r#"(?i)(?:[a-z]:[\\/]|/)[^\r\n\"'<>]+"#).unwrap(),
        ]
    });
    let mut clean = input.to_owned();
    for rule in rules {
        clean = rule.replace_all(&clean, "[redacted]").into_owned();
    }
    clean.chars().take(4096).collect()
}

#[derive(Clone, Debug, Serialize, Deserialize)]
#[serde(rename_all = "camelCase")]
pub struct Report {
    pub version: String,
    pub release_tag: String,
    pub commit: String,
    pub target: String,
    pub cpu_policy: String,
    pub os: String,
    pub cpu_features: Vec<String>,
    pub timestamp: u64,
    pub stage: String,
    pub message: String,
    pub model: Option<String>,
    pub exit_code: Option<i64>,
    pub signal: Option<i32>,
    pub logs: Vec<String>,
}

#[derive(Default)]
pub struct Diagnostics {
    logs: VecDeque<String>,
    path: Option<PathBuf>,
    previous: Option<Report>,
    pub exit_code: Option<i64>,
    pub signal: Option<i32>,
}

impl Diagnostics {
    pub fn set_path(&mut self, path: PathBuf) {
        if let Ok(metadata) = std::fs::metadata(&path) {
            if metadata.len() <= 131072 {
                if let Ok(bytes) = std::fs::read(&path) {
                    self.previous =
                        serde_json::from_slice::<Report>(&bytes)
                            .ok()
                            .map(|mut report| {
                                report.message = redact(&report.message);
                                report.logs =
                                    report.logs.iter().take(200).map(|s| redact(s)).collect();
                                report
                            });
                }
            }
        }
        self.path = Some(path);
    }

    pub fn append(&mut self, text: &str) {
        for line in text.lines() {
            let lower = line.to_ascii_lowercase();
            // Only model-runtime sources; never document indexing, tools, HTTP bodies or chat.
            if !["llama_", "ggml_", "dyld:"]
                .iter()
                .any(|prefix| lower.starts_with(prefix))
            {
                continue;
            }
            // Never collect request/chat payloads, even when verbose backend logging is enabled.
            if [
                "prompt",
                "completion",
                "messages",
                "content",
                "authorization",
                "token",
                "password",
                "secret",
                "api_key",
                "api-key",
            ]
            .iter()
            .any(|word| lower.contains(word))
            {
                continue;
            }
            if ![
                "error",
                "fail",
                "abort",
                "exit",
                "dyld",
                "illegal",
                "memory",
                "alloc",
                "backend",
                "version",
                "cpu",
                "signal",
                "load_model",
                "load_tensors",
            ]
            .iter()
            .any(|word| lower.contains(word))
            {
                continue;
            }
            self.logs.push_back(redact(line));
            while self.logs.len() > 200 || self.logs.iter().map(|s| s.len()).sum::<usize>() > 65536
            {
                self.logs.pop_front();
            }
        }
    }

    pub fn previous(&self) -> Option<Report> {
        self.previous.clone()
    }

    pub fn snapshot(
        &mut self,
        stage: &str,
        model: Option<String>,
        message: &str,
        model_exit: Option<i64>,
    ) -> Report {
        let report = Report {
            version: env!("CARGO_PKG_VERSION").into(),
            release_tag: option_env!("DELTA_BUILD_TAG")
                .unwrap_or("development")
                .into(),
            commit: option_env!("DELTA_BUILD_COMMIT")
                .unwrap_or("unknown")
                .into(),
            target: option_env!("DELTA_BUILD_TARGET")
                .unwrap_or(std::env::consts::ARCH)
                .into(),
            cpu_policy: option_env!("DELTA_CPU_POLICY")
                .unwrap_or("development")
                .into(),
            os: operating_system(),
            cpu_features: cpu_features(),
            timestamp: SystemTime::now()
                .duration_since(UNIX_EPOCH)
                .unwrap_or_default()
                .as_secs(),
            stage: stage.into(),
            message: redact(message),
            model: model.map(|s| {
                redact(s.rsplit(['/', '\\']).next().unwrap_or(""))
                    .chars()
                    .take(160)
                    .collect()
            }),
            exit_code: model_exit.or(self.exit_code),
            signal: self.signal,
            logs: self.logs.iter().cloned().collect(),
        };
        self.previous = Some(report.clone());
        if let Some(path) = &self.path {
            if let (Some(parent), Ok(bytes)) = (path.parent(), serde_json::to_vec_pretty(&report)) {
                let _ = std::fs::create_dir_all(parent);
                // Atomic replace where supported; keep only one sanitized failure report.
                let temp = path.with_extension("tmp");
                let mut options = std::fs::OpenOptions::new();
                options.write(true).create(true).truncate(true);
                #[cfg(unix)]
                {
                    use std::os::unix::fs::OpenOptionsExt;
                    options.mode(0o600);
                }
                use std::io::Write;
                if options
                    .open(&temp)
                    .and_then(|mut file| file.write_all(&bytes))
                    .is_ok()
                {
                    if std::fs::rename(&temp, path).is_err() {
                        let _ = std::fs::remove_file(&temp);
                    }
                }
            }
        }
        report
    }
}

fn operating_system() -> String {
    #[cfg(target_os = "linux")]
    if let Ok(text) = std::fs::read_to_string("/etc/os-release") {
        if let Some(line) = text.lines().find(|s| s.starts_with("PRETTY_NAME=")) {
            return redact(line.trim_start_matches("PRETTY_NAME=").trim_matches('"'));
        }
    }
    #[cfg(target_os = "macos")]
    if let Ok(output) = std::process::Command::new("/usr/bin/sw_vers")
        .arg("-productVersion")
        .output()
    {
        return format!("macOS {}", String::from_utf8_lossy(&output.stdout).trim());
    }
    #[cfg(target_os = "windows")]
    if let Ok(output) = std::process::Command::new("cmd")
        .args(["/C", "ver"])
        .output()
    {
        return redact(String::from_utf8_lossy(&output.stdout).trim());
    }
    std::env::consts::OS.into()
}

fn cpu_features() -> Vec<String> {
    let mut features = Vec::new();
    #[cfg(any(target_arch = "x86", target_arch = "x86_64"))]
    {
        if std::is_x86_feature_detected!("sse2") {
            features.push("sse2".into());
        }
        if std::is_x86_feature_detected!("avx") {
            features.push("avx".into());
        }
        if std::is_x86_feature_detected!("avx2") {
            features.push("avx2".into());
        }
        if std::is_x86_feature_detected!("avx512f") {
            features.push("avx512f".into());
        }
    }
    #[cfg(target_arch = "aarch64")]
    {
        features.push("arm64".into());
    }
    features
}

#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn reports_never_include_credentials_or_personal_paths() {
        let clean = redact(
            "Failed to load /Users/Asha/private/model.gguf with api_key=topsecret Bearer abc123",
        );
        assert!(!clean.contains("Asha"));
        assert!(!clean.contains("topsecret"));
        assert!(!clean.contains("abc123"));
    }
    #[test]
    fn windows_paths_and_url_credentials_are_removed() {
        let clean = redact("Load failed: C:\\Users\\Asha\\models\\private.gguf https://user:pass@example.com/a?token=secret");
        assert!(!clean.contains("Asha"));
        assert!(!clean.contains("pass"));
        assert!(!clean.contains("secret"));
    }
    #[test]
    fn quoted_paths_with_spaces_and_unrelated_document_logs_are_excluded() {
        for path in [
            r#""C:\Users\Asha Smith\models\x.gguf""#,
            r#""/Users/Asha Smith/models/x.gguf""#,
        ] {
            assert!(!redact(path).contains("Smith"));
        }
        let mut store = Diagnostics::default();
        store.append("[delta-documents] indexing My private medical report failed: unavailable");
        store.append("[harness] tool error: My personal plans");
        assert!(store.logs.is_empty());
    }

    #[test]
    fn last_failure_survives_restart_and_contains_no_credentials() {
        let directory = std::env::temp_dir().join(format!(
            "delta-report-test-{}-{}",
            std::process::id(),
            SystemTime::now()
                .duration_since(UNIX_EPOCH)
                .unwrap()
                .as_nanos()
        ));
        let path = directory.join("last-failure.json");
        let mut store = Diagnostics::default();
        store.set_path(path.clone());
        store.snapshot("backend", None, "Failed api_key=supersecret", Some(1));
        assert!(!std::fs::read_to_string(&path)
            .unwrap()
            .contains("supersecret"));
        let mut restored = Diagnostics::default();
        restored.set_path(path);
        assert_eq!(restored.previous().unwrap().exit_code, Some(1));
        std::fs::remove_dir_all(directory).unwrap();
    }
    #[test]
    fn logs_are_bounded_and_chat_payloads_are_excluded() {
        let mut store = Diagnostics::default();
        store.append("error prompt: private conversation\nerror Authorization: Bearer secret\n");
        for i in 0..1000 {
            store.append(&format!("ggml_backend: error {i}: {}", "x".repeat(2048)));
        }
        let report = store.snapshot("model-load", None, "Loading failed", None);
        assert!(report.logs.len() <= 200);
        assert!(report.logs.iter().map(|s| s.len()).sum::<usize>() <= 65536);
        assert!(!format!("{report:?}").contains("private conversation"));
    }
    #[test]
    fn snapshot_preserves_exit_details_and_only_the_model_basename() {
        let mut store = Diagnostics::default();
        store.signal = Some(4);
        let report = store.snapshot(
            "model-load",
            Some("/home/asha/models/gemma.gguf".into()),
            "Illegal instruction",
            Some(132),
        );
        assert_eq!(report.model.as_deref(), Some("gemma.gguf"));
        assert_eq!(report.exit_code, Some(132));
        assert_eq!(report.signal, Some(4));
    }
}
