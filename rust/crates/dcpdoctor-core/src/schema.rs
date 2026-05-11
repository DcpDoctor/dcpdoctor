/// XML schema validation.
/// Requires xerces-c equivalent — placeholder for now.
use serde::{Deserialize, Serialize};
use std::path::Path;

#[derive(Debug, Clone, Default, Serialize, Deserialize)]
pub struct SchemaError {
    pub line: u32,
    pub column: u32,
    pub message: String,
}

#[derive(Debug, Clone, Default, Serialize, Deserialize)]
pub struct SchemaValidationResult {
    pub valid: bool,
    pub errors: Vec<SchemaError>,
}

/// Validate XML against SMPTE XSD schemas using xmllint.
///
/// Delegates to the system `xmllint` tool for full XSD validation.
/// Falls back to basic well-formedness checking if xmllint is unavailable.
pub fn validate_schema(xml_file: &Path, schema_dir: &Path) -> SchemaValidationResult {
    // Determine which XSD to use based on the XML content
    let content = match std::fs::read_to_string(xml_file) {
        Ok(c) => c,
        Err(e) => {
            return SchemaValidationResult {
                valid: false,
                errors: vec![SchemaError {
                    line: 0,
                    column: 0,
                    message: format!("Failed to read XML file: {e}"),
                }],
            };
        }
    };

    // Detect schema type from namespace
    let schema_file = if content.contains("PackingList") {
        "SMPTE-429-8-2006-PKL.xsd"
    } else if content.contains("CompositionPlaylist") {
        "SMPTE-429-7-2006-CPL.xsd"
    } else if content.contains("AssetMap") {
        "SMPTE-429-9-2007-AM.xsd"
    } else {
        // Can't determine schema — do well-formedness check only
        return validate_wellformed(&content);
    };

    let schema_path = schema_dir.join(schema_file);
    if !schema_path.exists() {
        // Schema file not found — fall back to well-formedness
        tracing::warn!(
            "Schema file {} not found, falling back to well-formedness check",
            schema_path.display()
        );
        return validate_wellformed(&content);
    }

    // Use xmllint for full XSD validation
    let output = std::process::Command::new("xmllint")
        .arg("--schema")
        .arg(&schema_path)
        .arg("--noout")
        .arg(xml_file)
        .output();

    match output {
        Ok(o) => {
            if o.status.success() {
                SchemaValidationResult {
                    valid: true,
                    errors: Vec::new(),
                }
            } else {
                let stderr = String::from_utf8_lossy(&o.stderr);
                let errors: Vec<SchemaError> = stderr
                    .lines()
                    .filter(|l| l.contains("error") || l.contains("Error"))
                    .map(|line| {
                        // xmllint format: "file:line: element error : message"
                        let parts: Vec<&str> = line.splitn(3, ':').collect();
                        let line_num = parts
                            .get(1)
                            .and_then(|s| s.trim().parse().ok())
                            .unwrap_or(0);
                        SchemaError {
                            line: line_num,
                            column: 0,
                            message: parts.last().unwrap_or(&line).trim().to_string(),
                        }
                    })
                    .collect();

                SchemaValidationResult {
                    valid: false,
                    errors,
                }
            }
        }
        Err(_) => {
            // xmllint not available — fall back to well-formedness
            tracing::warn!("xmllint not found, falling back to well-formedness check");
            validate_wellformed(&content)
        }
    }
}

/// Basic XML well-formedness check using quick-xml.
fn validate_wellformed(content: &str) -> SchemaValidationResult {
    use quick_xml::Reader;
    use quick_xml::events::Event;

    let mut reader = Reader::from_str(content);
    let mut errors = Vec::new();

    loop {
        match reader.read_event() {
            Ok(Event::Eof) => break,
            Ok(_) => {}
            Err(e) => {
                errors.push(SchemaError {
                    line: 0,
                    column: reader.error_position() as u32,
                    message: format!("XML parse error: {e}"),
                });
                break;
            }
        }
    }

    SchemaValidationResult {
        valid: errors.is_empty(),
        errors,
    }
}
