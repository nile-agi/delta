fn main() {
    let commit = std::process::Command::new("git")
        .args(["rev-parse", "HEAD"])
        .output()
        .ok()
        .filter(|output| output.status.success())
        .map(|o| String::from_utf8_lossy(&o.stdout).trim().to_owned())
        .unwrap_or_else(|| "unknown".into());
    println!("cargo:rerun-if-env-changed=RELEASE_TAG");
    println!("cargo:rerun-if-env-changed=DELTA_PORTABLE_RELEASE");
    println!("cargo:rerun-if-changed=../.git/HEAD");
    println!("cargo:rerun-if-changed=../.git/refs/heads/dev");
    println!("cargo:rustc-env=DELTA_BUILD_COMMIT={}", commit);
    println!(
        "cargo:rustc-env=DELTA_BUILD_TAG={}",
        std::env::var("RELEASE_TAG").unwrap_or_else(|_| "development".into())
    );
    println!(
        "cargo:rustc-env=DELTA_BUILD_TARGET={}",
        std::env::var("TARGET").unwrap_or_default()
    );
    let portable = std::env::var("DELTA_PORTABLE_RELEASE").as_deref() == Ok("1");
    let arch = std::env::var("CARGO_CFG_TARGET_ARCH").unwrap_or_default();
    println!(
        "cargo:rustc-env=DELTA_CPU_POLICY={}",
        if portable {
            format!("{}-baseline", arch)
        } else {
            "development".into()
        }
    );
    tauri_build::build()
}
