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
