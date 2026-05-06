#include "dcpdoctor/dcpdoctor.h"
#include "dcpdoctor/report.h"
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
    // Suppress libxml2 error messages to stderr
    xmlSetGenericErrorFunc(nullptr, suppress_xml_error);
    xmlSetStructuredErrorFunc(nullptr, suppress_xml_structured_error);

    CLI::App app{"DcpDoctor - DCP validator and verifier"};
    app.set_version_flag("--version", "0.1.0");

    // Options
    bool verbose = false;
    bool quiet = false;
    bool json = false;
    bool html = false;
    bool no_hashes = false;
    bool no_signatures = false;
    bool check_mxf = false;
    bool strict = false;
    std::string output_file;
    std::vector<std::string> dcp_dirs;

    app.add_flag("-v,--verbose", verbose, "Show info-level notes");
    app.add_flag("-q,--quiet", quiet, "Only show errors (no warnings/info)");
    app.add_flag("--json", json, "Output in JSON format");
    app.add_flag("--html", html, "Output as HTML report");
    app.add_flag("--no-hashes", no_hashes, "Skip hash verification");
    app.add_flag("--no-signatures", no_signatures, "Skip signature verification");
    app.add_flag("--check-mxf", check_mxf, "Inspect MXF essence metadata");
    app.add_flag("--strict", strict, "Enable strict SMPTE compliance checks");
    app.add_option("-o,--output", output_file, "Write report to file");
    app.add_option("dcp_dirs", dcp_dirs, "DCP directories to validate")
        ->required()
        ->check(CLI::ExistingDirectory);

    CLI11_PARSE(app, argc, argv);

    // Configure logging
    if (verbose) {
        spdlog::set_level(spdlog::level::debug);
    } else if (quiet) {
        spdlog::set_level(spdlog::level::err);
    } else {
        spdlog::set_level(spdlog::level::warn);
    }

    // Build options
    dcpdoctor::VerifyOptions opts;
    opts.check_hashes = !no_hashes;
    opts.check_signatures = !no_signatures;
    opts.check_picture_details = check_mxf || strict;
    opts.strict_smpte = strict;

    // Determine report format
    dcpdoctor::ReportFormat format = dcpdoctor::ReportFormat::text;
    if (json) format = dcpdoctor::ReportFormat::json;
    else if (html) format = dcpdoctor::ReportFormat::html;

    spdlog::debug("Validating {} DCP(s)", dcp_dirs.size());

    bool all_passed = true;

    for (const auto& dir_str : dcp_dirs) {
        fs::path dir(dir_str);
        spdlog::debug("Processing: {}", dir.string());

        auto result = dcpdoctor::verify(dir, opts);

        if (!result.ok())
            all_passed = false;

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
    }

    return all_passed ? 0 : 1;
}
