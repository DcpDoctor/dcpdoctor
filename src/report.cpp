#include "dcpdoctor/report.h"
#include <format>
#include <ostream>

namespace dcpdoctor {

static std::string html_escape(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        switch (c) {
        case '&': out += "&amp;"; break;
        case '<': out += "&lt;"; break;
        case '>': out += "&gt;"; break;
        case '"': out += "&quot;"; break;
        default: out += c;
        }
    }
    return out;
}

static void write_text(const VerifyResult& result, const std::filesystem::path& dcp_dir,
                       std::ostream& out) {
    out << "DCP Verification Report\n";
    out << "=======================\n";
    out << "Path: " << dcp_dir.string() << "\n";
    out << "Standard: " << (result.standard == Standard::smpte ? "SMPTE" :
                            result.standard == Standard::interop ? "Interop" : "Unknown") << "\n";
    out << "Result: " << (result.ok() ? "PASS" : "FAIL") << "\n";
    out << "Errors: " << result.error_count << "  Warnings: " << result.warning_count << "\n\n";

    if (result.notes.empty()) {
        out << "No issues found.\n";
        return;
    }

    for (const auto& note : result.notes) {
        out << std::format("[{}] {} - {}", note.severity_str(), note.code_str(), note.message);
        if (!note.file.empty())
            out << " (" << note.file.filename().string() << ")";
        out << "\n";
    }
}

static void write_json(const VerifyResult& result, const std::filesystem::path& dcp_dir,
                       std::ostream& out) {
    out << "{\n";
    out << "  \"path\": \"" << dcp_dir.string() << "\",\n";
    out << "  \"standard\": \"" << (result.standard == Standard::smpte ? "smpte" :
                                    result.standard == Standard::interop ? "interop" : "unknown") << "\",\n";
    out << "  \"pass\": " << (result.ok() ? "true" : "false") << ",\n";
    out << "  \"errors\": " << result.error_count << ",\n";
    out << "  \"warnings\": " << result.warning_count << ",\n";
    out << "  \"notes\": [\n";

    for (size_t i = 0; i < result.notes.size(); ++i) {
        const auto& note = result.notes[i];
        out << "    {\n";
        out << "      \"severity\": \"" << note.severity_str() << "\",\n";
        out << "      \"code\": \"" << note.code_str() << "\",\n";
        out << "      \"message\": \"" << note.message << "\",\n";
        out << "      \"file\": \"" << note.file.string() << "\"\n";
        out << "    }" << (i + 1 < result.notes.size() ? "," : "") << "\n";
    }

    out << "  ]\n";
    out << "}\n";
}

static void write_html(const VerifyResult& result, const std::filesystem::path& dcp_dir,
                       std::ostream& out) {
    auto standard_str = result.standard == Standard::smpte ? "SMPTE" :
                        result.standard == Standard::interop ? "Interop" : "Unknown";
    auto pass_str = result.ok() ? "PASS" : "FAIL";
    auto badge_color = result.ok() ? "#28a745" : "#dc3545";

    out << R"(<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>DCP Verification Report</title>
<style>
:root { --bg: #1a1a2e; --card: #16213e; --text: #e0e0e0; --accent: #0f3460; }
* { margin: 0; padding: 0; box-sizing: border-box; }
body { font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;
       background: var(--bg); color: var(--text); padding: 2rem; min-height: 100vh; }
.container { max-width: 900px; margin: 0 auto; }
h1 { font-size: 1.8rem; margin-bottom: 1.5rem; color: #fff; }
.summary { background: var(--card); border-radius: 12px; padding: 1.5rem; margin-bottom: 1.5rem;
           display: grid; grid-template-columns: 1fr auto; gap: 1rem; align-items: center; }
.summary-info p { margin: 0.3rem 0; font-size: 0.95rem; }
.badge { display: inline-block; padding: 0.5rem 1.5rem; border-radius: 8px; font-weight: 700;
         font-size: 1.4rem; color: #fff; text-transform: uppercase; }
.stats { display: flex; gap: 1rem; margin-bottom: 1.5rem; }
.stat-card { background: var(--card); border-radius: 8px; padding: 1rem 1.5rem; flex: 1; text-align: center; }
.stat-card .number { font-size: 2rem; font-weight: 700; }
.stat-card .label { font-size: 0.8rem; text-transform: uppercase; letter-spacing: 0.05em; opacity: 0.7; }
.stat-card.errors .number { color: #dc3545; }
.stat-card.warnings .number { color: #ffc107; }
.notes { background: var(--card); border-radius: 12px; overflow: hidden; }
.notes-header { padding: 1rem 1.5rem; border-bottom: 1px solid rgba(255,255,255,0.1); font-weight: 600; }
.note { padding: 0.8rem 1.5rem; border-bottom: 1px solid rgba(255,255,255,0.05);
        display: grid; grid-template-columns: 80px 1fr auto; gap: 1rem; align-items: center; }
.note:last-child { border-bottom: none; }
.note-severity { font-weight: 600; font-size: 0.8rem; text-transform: uppercase; padding: 0.2rem 0.6rem;
                 border-radius: 4px; text-align: center; }
.note-severity.error { background: rgba(220,53,69,0.2); color: #dc3545; }
.note-severity.warning { background: rgba(255,193,7,0.2); color: #ffc107; }
.note-severity.info { background: rgba(23,162,184,0.2); color: #17a2b8; }
.note-msg { font-size: 0.9rem; }
.note-code { font-size: 0.75rem; opacity: 0.6; font-family: monospace; }
.note-file { font-size: 0.8rem; opacity: 0.6; text-align: right; }
.empty { padding: 2rem; text-align: center; opacity: 0.6; }
footer { margin-top: 2rem; text-align: center; font-size: 0.8rem; opacity: 0.5; }
</style>
</head>
<body>
<div class="container">
<h1>DCP Verification Report</h1>

<div class="summary">
<div class="summary-info">
)";

    out << "<p><strong>Path:</strong> " << html_escape(dcp_dir.string()) << "</p>\n";
    out << "<p><strong>Standard:</strong> " << standard_str << "</p>\n";
    out << "</div>\n";
    out << "<div class=\"badge\" style=\"background:" << badge_color << "\">" << pass_str << "</div>\n";
    out << "</div>\n\n";

    out << "<div class=\"stats\">\n";
    out << "<div class=\"stat-card errors\"><div class=\"number\">" << result.error_count << "</div><div class=\"label\">Errors</div></div>\n";
    out << "<div class=\"stat-card warnings\"><div class=\"number\">" << result.warning_count << "</div><div class=\"label\">Warnings</div></div>\n";
    out << "<div class=\"stat-card\"><div class=\"number\">" << result.notes.size() << "</div><div class=\"label\">Total Notes</div></div>\n";
    out << "</div>\n\n";

    out << "<div class=\"notes\">\n";
    out << "<div class=\"notes-header\">Issues</div>\n";

    if (result.notes.empty()) {
        out << "<div class=\"empty\">No issues found. DCP is valid.</div>\n";
    } else {
        for (const auto& note : result.notes) {
            std::string sev_class = note.severity == Severity::error ? "error" :
                                    note.severity == Severity::warning ? "warning" : "info";
            out << "<div class=\"note\">\n";
            out << "  <span class=\"note-severity " << sev_class << "\">" << note.severity_str() << "</span>\n";
            out << "  <div><div class=\"note-msg\">" << html_escape(note.message) << "</div>";
            out << "<div class=\"note-code\">" << note.code_str() << "</div></div>\n";
            out << "  <div class=\"note-file\">" << html_escape(note.file.filename().string()) << "</div>\n";
            out << "</div>\n";
        }
    }

    out << "</div>\n\n";
    out << "<footer>Generated by dcpdoctor v0.1.0</footer>\n";
    out << "</div>\n</body>\n</html>\n";
}

void write_report(const VerifyResult& result, const std::filesystem::path& dcp_dir,
                  std::ostream& out, ReportFormat format) {
    switch (format) {
    case ReportFormat::text:
        write_text(result, dcp_dir, out);
        break;
    case ReportFormat::json:
        write_json(result, dcp_dir, out);
        break;
    case ReportFormat::html:
        write_html(result, dcp_dir, out);
        break;
    }
}

} // namespace dcpdoctor
