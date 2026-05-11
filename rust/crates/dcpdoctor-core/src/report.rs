use std::io::Write;
use std::path::Path;

use crate::VerifyResult;

/// Report output format.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum ReportFormat {
    Text,
    Json,
    Html,
}

/// Write a verification report.
pub fn write_report<W: Write>(
    result: &VerifyResult,
    dcp_path: &Path,
    writer: &mut W,
    format: ReportFormat,
) -> std::io::Result<()> {
    match format {
        ReportFormat::Text => write_text_report(result, dcp_path, writer),
        ReportFormat::Json => write_json_report(result, writer),
        ReportFormat::Html => write_html_report(result, dcp_path, writer),
    }
}

fn write_text_report<W: Write>(
    result: &VerifyResult,
    dcp_path: &Path,
    writer: &mut W,
) -> std::io::Result<()> {
    writeln!(writer, "=== DcpDoctor Report ===")?;
    writeln!(writer, "DCP: {}", dcp_path.display())?;
    writeln!(writer, "Standard: {}", result.standard)?;
    writeln!(
        writer,
        "Result: {} ({} errors, {} warnings)",
        if result.ok() { "PASS" } else { "FAIL" },
        result.error_count,
        result.warning_count
    )?;
    writeln!(writer)?;

    for note in &result.notes {
        writeln!(writer, "{}", note)?;
    }

    Ok(())
}

fn write_json_report<W: Write>(result: &VerifyResult, writer: &mut W) -> std::io::Result<()> {
    let json = serde_json::to_string_pretty(result).map_err(std::io::Error::other)?;
    writer.write_all(json.as_bytes())
}

fn write_html_report<W: Write>(
    result: &VerifyResult,
    dcp_path: &Path,
    writer: &mut W,
) -> std::io::Result<()> {
    writeln!(writer, "<!DOCTYPE html><html><head><meta charset='utf-8'>")?;
    writeln!(writer, "<title>DcpDoctor Report</title>")?;
    writeln!(writer, "<style>")?;
    writeln!(writer, "body{{font-family:sans-serif;margin:2em}}")?;
    writeln!(writer, "table{{border-collapse:collapse;width:100%}}")?;
    writeln!(
        writer,
        "th,td{{border:1px solid #ccc;padding:8px;text-align:left}}"
    )?;
    writeln!(
        writer,
        ".error{{color:#c00}}.warning{{color:#c80}}.info{{color:#08c}}"
    )?;
    writeln!(writer, "</style></head><body>")?;
    writeln!(writer, "<h1>DcpDoctor Report</h1>")?;
    writeln!(writer, "<p>DCP: {}</p>", dcp_path.display())?;
    writeln!(writer, "<p>Standard: {}</p>", result.standard)?;
    writeln!(
        writer,
        "<p>Result: <strong>{}</strong> ({} errors, {} warnings)</p>",
        if result.ok() { "PASS" } else { "FAIL" },
        result.error_count,
        result.warning_count
    )?;
    writeln!(
        writer,
        "<table><tr><th>Severity</th><th>Code</th><th>Message</th><th>File</th></tr>"
    )?;
    for note in &result.notes {
        let class = match note.severity {
            crate::Severity::Error => "error",
            crate::Severity::Warning => "warning",
            crate::Severity::Info => "info",
        };
        let file = note
            .file
            .as_ref()
            .map(|f| f.display().to_string())
            .unwrap_or_default();
        writeln!(
            writer,
            "<tr><td class='{class}'>{}</td><td>{}</td><td>{}</td><td>{}</td></tr>",
            note.severity, note.code, note.message, file
        )?;
    }
    writeln!(writer, "</table></body></html>")?;
    Ok(())
}
