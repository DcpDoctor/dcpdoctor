use crate::{Code, Note, Severity};

impl Note {
    pub fn error(code: Code, message: impl Into<String>) -> Self {
        Self {
            severity: Severity::Error,
            code,
            message: message.into(),
            file: None,
            line: 0,
        }
    }

    pub fn warning(code: Code, message: impl Into<String>) -> Self {
        Self {
            severity: Severity::Warning,
            code,
            message: message.into(),
            file: None,
            line: 0,
        }
    }

    pub fn info(code: Code, message: impl Into<String>) -> Self {
        Self {
            severity: Severity::Info,
            code,
            message: message.into(),
            file: None,
            line: 0,
        }
    }

    pub fn with_file(mut self, file: impl Into<std::path::PathBuf>) -> Self {
        self.file = Some(file.into());
        self
    }
}

#[cfg(test)]
mod tests {
    use crate::{Code, Note, Severity};

    #[test]
    fn test_note_error() {
        let n = Note::error(Code::MissingAssetmap, "no ASSETMAP");
        assert_eq!(n.severity, Severity::Error);
        assert_eq!(n.code, Code::MissingAssetmap);
        assert_eq!(n.message, "no ASSETMAP");
        assert!(n.file.is_none());
    }

    #[test]
    fn test_note_warning_with_file() {
        let n = Note::warning(Code::PklHashMismatch, "hash mismatch").with_file("/path/to/pkl.xml");
        assert_eq!(n.severity, Severity::Warning);
        assert_eq!(n.file.unwrap().to_str().unwrap(), "/path/to/pkl.xml");
    }

    #[test]
    fn test_note_display() {
        let n = Note::info(Code::EncryptionDetected, "content is encrypted");
        let display = format!("{n}");
        assert!(display.contains("INFO"));
        assert!(display.contains("encryption_detected"));
    }

    #[test]
    fn test_severity_ordering() {
        assert!(Severity::Info < Severity::Warning);
        assert!(Severity::Warning < Severity::Error);
    }

    #[test]
    fn test_code_as_str() {
        assert_eq!(Code::MissingAssetmap.as_str(), "missing_assetmap");
        assert_eq!(Code::J2kBitrateExceeded.as_str(), "j2k_bitrate_exceeded");
    }
}
