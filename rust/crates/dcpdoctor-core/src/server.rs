/// REST API server for remote validation.
/// Placeholder — will use axum or similar.
use std::path::Path;

/// Start the REST API server.
pub fn start_server(_bind: &str, _port: u16) {
    tracing::warn!("REST API server not yet implemented");
}

/// Watch a directory for new DCPs and auto-validate.
pub fn watch_directory(
    _dir: &Path,
    _opts: &crate::VerifyOptions,
    _on_result: impl Fn(&Path, &crate::VerifyResult),
    _poll_interval_ms: u32,
) {
    tracing::warn!("Directory watching not yet implemented");
}
