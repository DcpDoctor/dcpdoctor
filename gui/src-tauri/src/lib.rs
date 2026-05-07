use serde::{Deserialize, Serialize};
use std::process::Command;

#[derive(Debug, Serialize, Deserialize, Clone)]
pub struct ValidationResult {
    pub severity: String,
    pub code: String,
    pub message: String,
    pub file: String,
}

#[derive(Debug, Serialize, Deserialize)]
pub struct ValidationResponse {
    pub results: Vec<ValidationResult>,
    pub summary: String,
    pub exit_code: i32,
}

fn find_dcpdoctor_binary() -> String {
    // Look for the binary in common locations
    let candidates = [
        // Sidecar (bundled with Tauri)
        std::env::current_exe()
            .ok()
            .and_then(|p| p.parent().map(|d| d.join("dcpdoctor")))
            .unwrap_or_default(),
        // Build directory (development)
        std::path::PathBuf::from("../build/dcpdoctor"),
        std::path::PathBuf::from("dcpdoctor"),
    ];

    for candidate in &candidates {
        if candidate.exists() {
            return candidate.to_string_lossy().to_string();
        }
    }

    // Fallback to PATH
    "dcpdoctor".to_string()
}

fn parse_output(output: &str) -> Vec<ValidationResult> {
    let mut results = Vec::new();
    for line in output.lines() {
        let line = line.trim();
        if line.is_empty() {
            continue;
        }

        // Parse lines like: [ERROR] (code) message | file
        // or:                [WARN]  (code) message | file
        let severity = if line.starts_with("[ERROR]") {
            "error"
        } else if line.starts_with("[WARN]") || line.starts_with("[WARNING]") {
            "warning"
        } else if line.starts_with("[INFO]") {
            "info"
        } else {
            continue;
        };

        let rest = line
            .trim_start_matches("[ERROR]")
            .trim_start_matches("[WARNING]")
            .trim_start_matches("[WARN]")
            .trim_start_matches("[INFO]")
            .trim();

        let (code, rest) = if rest.starts_with('(') {
            if let Some(end) = rest.find(')') {
                (&rest[1..end], rest[end + 1..].trim())
            } else {
                ("unknown", rest)
            }
        } else {
            ("unknown", rest)
        };

        let (message, file) = if let Some(pipe_pos) = rest.rfind('|') {
            (rest[..pipe_pos].trim(), rest[pipe_pos + 1..].trim())
        } else {
            (rest, "")
        };

        results.push(ValidationResult {
            severity: severity.to_string(),
            code: code.to_string(),
            message: message.to_string(),
            file: file.to_string(),
        });
    }
    results
}

#[tauri::command]
fn validate_dcp(path: String, flags: Vec<String>) -> Result<ValidationResponse, String> {
    let binary = find_dcpdoctor_binary();

    let mut cmd = Command::new(&binary);
    cmd.arg(&path);
    for flag in &flags {
        cmd.arg(flag);
    }

    let output = cmd.output().map_err(|e| {
        format!(
            "Failed to run dcpdoctor binary at '{}': {}",
            binary, e
        )
    })?;

    let stdout = String::from_utf8_lossy(&output.stdout).to_string();
    let stderr = String::from_utf8_lossy(&output.stderr).to_string();
    let combined = format!("{}\n{}", stdout, stderr);

    let results = parse_output(&combined);
    let exit_code = output.status.code().unwrap_or(-1);

    let errors = results.iter().filter(|r| r.severity == "error").count();
    let warnings = results.iter().filter(|r| r.severity == "warning").count();

    let summary = if results.is_empty() && exit_code == 0 {
        "DCP is valid — no issues found.".to_string()
    } else {
        format!(
            "{} error(s), {} warning(s) found.",
            errors, warnings
        )
    };

    Ok(ValidationResponse {
        results,
        summary,
        exit_code,
    })
}

#[tauri::command]
fn get_version() -> Result<String, String> {
    let binary = find_dcpdoctor_binary();
    let output = Command::new(&binary)
        .arg("--version")
        .output()
        .map_err(|e| format!("Failed to run dcpdoctor: {}", e))?;

    Ok(String::from_utf8_lossy(&output.stdout).trim().to_string())
}

#[cfg_attr(mobile, tauri::mobile_entry_point)]
pub fn run() {
    tauri::Builder::default()
        .plugin(tauri_plugin_shell::init())
        .invoke_handler(tauri::generate_handler![validate_dcp, get_version])
        .run(tauri::generate_context!())
        .expect("error while running tauri application");
}
