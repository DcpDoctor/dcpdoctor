#include "dcpdoctor/dcpdoctor.h"
#include "dcpdoctor/advanced.h"
#include "dcpdoctor/report.h"
#include "dcpdoctor/server.h"
#include "dcpdoctor/timeline.h"
#include <CLI/CLI.hpp>
#include <spdlog/spdlog.h>
#include <libxml/parser.h>
#include <libxml/xmlerror.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

static void suppress_xml_error(void* /*ctx*/, const char* /*msg*/, ...) {}
static void suppress_xml_structured_error(void* /*ctx*/, const xmlError* /*err*/) {}

int main(int argc, char* argv[]) {
    xmlSetGenericErrorFunc(nullptr, suppress_xml_error);
    xmlSetStructuredErrorFunc(nullptr, suppress_xml_structured_error);

    CLI::App app{"DcpDoctor - DCP validator and verifier"};
    app.set_version_flag("--version", "0.1.0");
    app.require_subcommand(0, 1);

    // === VALIDATE subcommand (default) ===
    auto* validate_cmd = app.add_subcommand("validate", "Validate DCP directories");

    bool verbose = false, quiet = false, json = false, html = false;
    bool no_hashes = false, no_signatures = false, check_mxf = false, strict = false;
    bool bv21 = false, deep_j2k = false;
    std::string output_file, manifest_file, timeline_file;
    std::vector<std::string> dcp_dirs;

    validate_cmd->add_flag("-v,--verbose", verbose, "Show info-level notes");
    validate_cmd->add_flag("-q,--quiet", quiet, "Only show errors");
    validate_cmd->add_flag("--json", json, "JSON output");
    validate_cmd->add_flag("--html", html, "HTML report output");
    validate_cmd->add_flag("--no-hashes", no_hashes, "Skip hash verification");
    validate_cmd->add_flag("--no-signatures", no_signatures, "Skip signature verification");
    validate_cmd->add_flag("--check-mxf", check_mxf, "Inspect MXF essence metadata");
    validate_cmd->add_flag("--strict", strict, "Strict SMPTE compliance");
    validate_cmd->add_flag("--bv21", bv21, "Check BV2.1 application profile");
    validate_cmd->add_flag("--deep-j2k", deep_j2k, "Deep J2K codestream validation");
    validate_cmd->add_option("-o,--output", output_file, "Write report to file");
    validate_cmd->add_option("--manifest", manifest_file, "Compare against manifest JSON");
    validate_cmd->add_option("--timeline", timeline_file, "Write SVG timeline to file");
    validate_cmd->add_option("dcp_dirs", dcp_dirs, "DCP directories to validate")
        ->required()
        ->check(CLI::ExistingDirectory);

    // === WATCH subcommand ===
    auto* watch_cmd = app.add_subcommand("watch", "Watch directory for new DCPs");
    std::string watch_dir;
    int poll_interval = 5000;
    watch_cmd->add_option("directory", watch_dir, "Directory to watch")
        ->required()->check(CLI::ExistingDirectory);
    watch_cmd->add_option("--interval", poll_interval, "Poll interval in ms (default: 5000)");

    // === SERVE subcommand ===
    auto* serve_cmd = app.add_subcommand("serve", "Start REST API server");
    std::string bind_addr = "0.0.0.0";
    int port = 8080;
    serve_cmd->add_option("--bind", bind_addr, "Bind address (default: 0.0.0.0)");
    serve_cmd->add_option("--port,-p", port, "Port (default: 8080)");

    // Also allow validate args on the main app for backward compat
    app.add_flag("-v,--verbose", verbose, "Show info-level notes");
    app.add_flag("-q,--quiet", quiet, "Only show errors");
    app.add_flag("--json", json, "JSON output");
    app.add_flag("--html", html, "HTML report output");
    app.add_flag("--no-hashes", no_hashes, "Skip hash verification");
    app.add_flag("--no-signatures", no_signatures, "Skip signature verification");
    app.add_flag("--check-mxf", check_mxf, "Inspect MXF essence metadata");
    app.add_flag("--strict", strict, "Strict SMPTE compliance");
    app.add_flag("--bv21", bv21, "Check BV2.1 application profile");
    app.add_flag("--deep-j2k", deep_j2k, "Deep J2K codestream validation");
    app.add_option("-o,--output", output_file, "Write report to file");
    app.add_option("--manifest", manifest_file, "Compare against manifest JSON");
    app.add_option("--timeline", timeline_file, "Write SVG timeline to file");
    app.add_option("dcp_dirs", dcp_dirs, "DCP directories to validate")
        ->check(CLI::ExistingDirectory);

    CLI11_PARSE(app, argc, argv);

    // Configure logging
    if (verbose) spdlog::set_level(spdlog::level::debug);
    else if (quiet) spdlog::set_level(spdlog::level::err);
    else spdlog::set_level(spdlog::level::warn);

    // === WATCH mode ===
    if (watch_cmd->parsed()) {
        dcpdoctor::VerifyOptions opts;
        opts.check_hashes = true;
        opts.check_signatures = true;
        opts.check_picture_details = true;

        dcpdoctor::watch_directory(fs::path(watch_dir), opts,
            [](const fs::path& path, const dcpdoctor::VerifyResult& result) {
                std::string status = result.ok() ? "PASS" : "FAIL";
                spdlog::info("{}: {} ({} errors, {} warnings)",
                            path.string(), status, result.error_count, result.warning_count);
                dcpdoctor::write_report(result, path, std::cout, dcpdoctor::ReportFormat::text);
            }, poll_interval);
        return 0;
    }

    // === SERVE mode ===
    if (serve_cmd->parsed()) {
        dcpdoctor::VerifyOptions opts;
        opts.check_hashes = true;
        opts.check_signatures = true;
        opts.check_picture_details = true;
        dcpdoctor::serve_api(bind_addr, port, opts);
        return 0;
    }

    // === VALIDATE mode (default) ===
    if (dcp_dirs.empty()) {
        std::cerr << app.help() << "\n";
        return 2;
    }

    dcpdoctor::VerifyOptions opts;
    opts.check_hashes = !no_hashes;
    opts.check_signatures = !no_signatures;
    opts.check_picture_details = check_mxf || strict || deep_j2k;
    opts.strict_smpte = strict;

    dcpdoctor::ReportFormat format = dcpdoctor::ReportFormat::text;
    if (json) format = dcpdoctor::ReportFormat::json;
    else if (html) format = dcpdoctor::ReportFormat::html;

    spdlog::debug("Validating {} DCP(s)", dcp_dirs.size());

    bool all_passed = true;
    std::vector<dcpdoctor::BatchResult> batch_results;
    dcpdoctor::ProgressBar progress(dcp_dirs.size(),
                                     dcp_dirs.size() > 1 ? "Validating" : "");

    for (size_t idx = 0; idx < dcp_dirs.size(); ++idx) {
        fs::path dir(dcp_dirs[idx]);
        if (dcp_dirs.size() > 1)
            progress.update(idx);
        spdlog::debug("Processing: {}", dir.string());

        auto result = dcpdoctor::verify(dir, opts);

        // BV2.1 compliance
        if (bv21) {
            auto bv21_notes = dcpdoctor::check_bv21_compliance(dir, result.standard);
            for (auto& note : bv21_notes)
                result.add(std::move(note));
        }

        // Manifest comparison
        if (!manifest_file.empty()) {
            auto manifest_notes = dcpdoctor::compare_manifest(dir, fs::path(manifest_file));
            for (auto& note : manifest_notes)
                result.add(std::move(note));
        }

        if (!result.ok())
            all_passed = false;

        // Batch tracking
        batch_results.push_back({dir, result.ok(), result.error_count,
                                result.warning_count, result.standard});

        spdlog::debug("Result: {} errors, {} warnings",
                     result.error_count, result.warning_count);

        if (!quiet) {
            if (!output_file.empty()) {
                std::ofstream out(output_file);
                dcpdoctor::write_report(result, dir, out, format);
            } else {
                dcpdoctor::write_report(result, dir, std::cout, format);
            }
        }

        // Timeline SVG generation
        if (!timeline_file.empty()) {
            // Find CPL in DCP
            std::error_code ec;
            for (auto& entry : fs::directory_iterator(dir, ec)) {
                if (!entry.is_regular_file()) continue;
                if (entry.path().extension() != ".xml") continue;
                auto reels = dcpdoctor::extract_timeline(entry.path());
                if (!reels.empty()) {
                    std::ofstream svg_out(timeline_file);
                    dcpdoctor::write_timeline_svg(svg_out, reels,
                        dir.filename().string(), 24.0);
                    spdlog::info("Timeline written to: {}", timeline_file);
                    break;
                }
            }
        }
    }

    if (dcp_dirs.size() > 1)
        progress.finish();

    // Batch summary for multiple DCPs
    if (batch_results.size() > 1) {
        std::cout << "\n";
        dcpdoctor::write_batch_summary(std::cout, batch_results);
    }

    return all_passed ? 0 : 1;
}
