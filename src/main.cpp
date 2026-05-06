#include "dcpdoctor/dcpdoctor.h"
#include "dcpdoctor/report.h"
#include <libxml/parser.h>
#include <libxml/xmlerror.h>
#include <filesystem>
#include <iostream>
#include <fstream>
#include <string>
#include <string_view>
#include <vector>

namespace fs = std::filesystem;

static void suppress_xml_error(void* /*ctx*/, const char* /*msg*/, ...) {}
static void suppress_xml_structured_error(void* /*ctx*/, const xmlError* /*err*/) {}

static void usage(const char* prog) {
    std::cerr << "Usage: " << prog << " [OPTIONS] <dcp-directory> [dcp-directory...]\n"
              << "\n"
              << "Options:\n"
              << "  -h, --help           Show this help\n"
              << "  -v, --verbose        Verbose output\n"
              << "  -q, --quiet          Only show errors\n"
              << "  --json               Output in JSON format\n"
              << "  --html               Output as HTML report\n"
              << "  --no-hashes          Skip hash verification\n"
              << "  --no-signatures      Skip signature verification\n"
              << "  --check-mxf          Inspect MXF essence metadata\n"
              << "  --strict             Enable strict SMPTE compliance checks\n"
              << "  -o, --output FILE    Write report to file\n"
              << "\n"
              << "Exit codes:\n"
              << "  0  All DCPs passed validation\n"
              << "  1  One or more DCPs failed validation\n"
              << "  2  Usage error\n";
}

int main(int argc, char* argv[]) {
    // Suppress libxml2 error messages to stderr
    xmlSetGenericErrorFunc(nullptr, suppress_xml_error);
    xmlSetStructuredErrorFunc(nullptr, suppress_xml_structured_error);

    dcpdoctor::VerifyOptions opts;
    dcpdoctor::ReportFormat format = dcpdoctor::ReportFormat::text;
    std::vector<fs::path> dcp_dirs;
    std::string output_file;
    bool quiet = false;

    for (int i = 1; i < argc; ++i) {
        std::string_view arg(argv[i]);

        if (arg == "-h" || arg == "--help") {
            usage(argv[0]);
            return 0;
        } else if (arg == "-v" || arg == "--verbose") {
            // verbose mode - show info notes too (default shows warnings+errors)
        } else if (arg == "-q" || arg == "--quiet") {
            quiet = true;
        } else if (arg == "--json") {
            format = dcpdoctor::ReportFormat::json;
        } else if (arg == "--html") {
            format = dcpdoctor::ReportFormat::html;
        } else if (arg == "--no-hashes") {
            opts.check_hashes = false;
        } else if (arg == "--no-signatures") {
            opts.check_signatures = false;
        } else if (arg == "--strict") {
            opts.strict_smpte = true;
            opts.check_picture_details = true;
        } else if (arg == "--check-mxf") {
            opts.check_picture_details = true;
        } else if (arg == "-o" || arg == "--output") {
            if (++i >= argc) {
                std::cerr << "Error: " << arg << " requires an argument\n";
                return 2;
            }
            output_file = argv[i];
        } else if (arg.starts_with("-")) {
            std::cerr << "Unknown option: " << arg << "\n";
            return 2;
        } else {
            dcp_dirs.emplace_back(argv[i]);
        }
    }

    if (dcp_dirs.empty()) {
        std::cerr << "Error: no DCP directory specified\n\n";
        usage(argv[0]);
        return 2;
    }

    bool all_passed = true;

    for (const auto& dir : dcp_dirs) {
        auto result = dcpdoctor::verify(dir, opts);

        if (!result.ok())
            all_passed = false;

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
