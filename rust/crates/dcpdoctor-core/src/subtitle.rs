/// Subtitle/timed text validation.
use crate::{Code, Note, Severity, Standard};
use std::path::Path;

/// Validate a subtitle/timed text XML file.
pub fn validate_subtitle(file: &Path, standard: Standard) -> Vec<Note> {
    let xml = match std::fs::read_to_string(file) {
        Ok(s) => s,
        Err(e) => {
            return vec![Note {
                severity: Severity::Error,
                code: Code::SubtitleParseError,
                message: format!("Failed to read subtitle file: {e}"),
                file: Some(file.to_path_buf()),
                line: 0,
            }];
        }
    };

    let mut notes = Vec::new();

    // Check if the namespace matches the standard
    match standard {
        Standard::Smpte => {
            if !xml.contains("http://www.smpte-ra.org/schemas/428-7/2010/DCST") {
                notes.push(Note {
                    severity: Severity::Warning,
                    code: Code::SmpteNamespaceWrong,
                    message: "Subtitle file does not use SMPTE namespace".to_string(),
                    file: Some(file.to_path_buf()),
                    line: 0,
                });
            }
        }
        Standard::Interop => {
            if !xml.contains("http://www.digicine.com/PROTO-ASDCP-TT-DEF") {
                notes.push(Note {
                    severity: Severity::Warning,
                    code: Code::InteropNamespaceWrong,
                    message: "Subtitle file does not use Interop namespace".to_string(),
                    file: Some(file.to_path_buf()),
                    line: 0,
                });
            }
        }
        Standard::Unknown => {}
    }

    notes
}
