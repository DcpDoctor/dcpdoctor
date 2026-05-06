#include "dcpdoctor/dcpdoctor.h"
#include "dcpdoctor/advanced.h"
#include "dcpdoctor/diff.h"
#include "dcpdoctor/fixes.h"
#include "dcpdoctor/kdm.h"
#include "dcpdoctor/photon.h"
#include "dcpdoctor/premium.h"
#include "dcpdoctor/report.h"
#include "dcpdoctor/server.h"
#include "dcpdoctor/theater.h"
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

int main(int argc, char* argv[]) {
    xmlSetGenericErrorFunc(nullptr, suppress_xml_error);
    xmlSetStructuredErrorFunc(nullptr, nullptr);

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

    // === DIFF subcommand ===
    auto* diff_cmd = app.add_subcommand("diff", "Compare two DCPs");
    std::string diff_a, diff_b;
    bool diff_hashes = false;
    diff_cmd->add_option("dcp_a", diff_a, "First DCP directory")->required()->check(CLI::ExistingDirectory);
    diff_cmd->add_option("dcp_b", diff_b, "Second DCP directory")->required()->check(CLI::ExistingDirectory);
    diff_cmd->add_flag("--hashes", diff_hashes, "Compare content hashes (slow)");

    // === PROFILES subcommand ===
    auto* profiles_cmd = app.add_subcommand("profiles", "List or check theater compatibility profiles");
    std::string profile_name;
    std::string profile_dcp;
    profiles_cmd->add_option("--check", profile_name, "Check DCP against named profile");
    profiles_cmd->add_option("--dcp", profile_dcp, "DCP directory to check")->check(CLI::ExistingDirectory);

    // === KDM subcommand ===
    auto* kdm_cmd = app.add_subcommand("kdm", "Validate a KDM file");
    std::string kdm_file, kdm_dcp;
    kdm_cmd->add_option("kdm_file", kdm_file, "KDM XML file")->required()->check(CLI::ExistingFile);
    kdm_cmd->add_option("--dcp", kdm_dcp, "DCP directory to validate against")->check(CLI::ExistingDirectory);

    // Additional validate flags
    bool suggest_fixes_flag = false;
    bool imf_mode = false;
    bool netflix_mode = false;
    bool accessibility_check = false;
    bool hdr_check = false;
    bool atmos_deep = false;
    std::string photon_dir_opt;
    validate_cmd->add_flag("--fix", suggest_fixes_flag, "Show fix suggestions for detected issues");
    validate_cmd->add_flag("--imf", imf_mode, "Validate as IMF package (uses Netflix Photon)");
    validate_cmd->add_flag("--netflix", netflix_mode, "Check Netflix IMF delivery specs");
    validate_cmd->add_flag("--accessibility", accessibility_check, "Check accessibility tracks (AD/HI/CC)");
    validate_cmd->add_flag("--hdr", hdr_check, "Detect and validate HDR metadata");
    validate_cmd->add_flag("--atmos", atmos_deep, "Deep Dolby Atmos IAB inspection");
    validate_cmd->add_option("--photon-dir", photon_dir_opt, "Path to Photon source directory");
    app.add_flag("--fix", suggest_fixes_flag, "Show fix suggestions for detected issues");
    app.add_flag("--imf", imf_mode, "Validate as IMF package (uses Netflix Photon)");
    app.add_flag("--netflix", netflix_mode, "Check Netflix IMF delivery specs");
    app.add_flag("--accessibility", accessibility_check, "Check accessibility tracks (AD/HI/CC)");
    app.add_flag("--hdr", hdr_check, "Detect and validate HDR metadata");
    app.add_flag("--atmos", atmos_deep, "Deep Dolby Atmos IAB inspection");
    app.add_option("--photon-dir", photon_dir_opt, "Path to Photon source directory");

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

    // === DIFF mode ===
    if (diff_cmd->parsed()) {
        auto diff = dcpdoctor::compare_dcps(fs::path(diff_a), fs::path(diff_b), diff_hashes);
        dcpdoctor::write_diff_report(std::cout, diff);
        return diff.content_identical ? 0 : 1;
    }

    // === PROFILES mode ===
    if (profiles_cmd->parsed()) {
        if (profile_name.empty()) {
            // List all profiles
            auto profiles = dcpdoctor::get_theater_profiles();
            std::cout << "Theater Compatibility Profiles:\n\n";
            for (const auto& p : profiles) {
                std::cout << "  " << p.name << " (" << p.vendor << ")\n";
                std::cout << "    4K: " << (p.supports_4k ? "Yes" : "No")
                         << "  HFR: " << (p.supports_hfr ? "Yes" : "No")
                         << "  Atmos: " << (p.supports_atmos ? "Yes" : "No")
                         << "  Interop: " << (p.supports_interop ? "Yes" : "No")
                         << "\n";
            }
        } else if (!profile_dcp.empty()) {
            auto* profile = dcpdoctor::find_profile(profile_name);
            if (!profile) {
                std::cerr << "Unknown profile: " << profile_name << "\n";
                return 2;
            }
            auto result = dcpdoctor::verify(fs::path(profile_dcp));
            auto notes = dcpdoctor::check_theater_compatibility(
                fs::path(profile_dcp), result, *profile);
            std::cout << "Theater compatibility: " << profile->name << "\n\n";
            for (const auto& n : notes)
                std::cout << "[" << n.severity_str() << "] " << n.message << "\n";
            if (notes.empty())
                std::cout << "No compatibility issues detected.\n";
        } else {
            std::cerr << "Use --check PROFILE --dcp DIR to check compatibility\n";
            return 2;
        }
        return 0;
    }

    // === KDM mode ===
    if (kdm_cmd->parsed()) {
        auto info = dcpdoctor::parse_kdm(fs::path(kdm_file));
        if (!info.valid) {
            std::cerr << "Invalid KDM: " << info.error << "\n";
            return 1;
        }
        std::cout << "KDM Information:\n";
        std::cout << "  Content: " << info.content_title << "\n";
        std::cout << "  CPL ID:  " << info.cpl_id << "\n";
        std::cout << "  Status:  " << (info.is_expired ? "EXPIRED" :
                     info.is_not_yet_valid ? "NOT YET VALID" : "VALID") << "\n";

        if (!kdm_dcp.empty()) {
            auto notes = dcpdoctor::validate_kdm(fs::path(kdm_file), fs::path(kdm_dcp));
            for (const auto& n : notes)
                std::cout << "[" << n.severity_str() << "] " << n.message << "\n";
            if (notes.empty())
                std::cout << "\nKDM validates against DCP.\n";
        }
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

        // IMF validation via Netflix Photon
        if (imf_mode) {
            auto photon_config = dcpdoctor::default_photon_config();
            if (!photon_dir_opt.empty())
                photon_config.photon_dir = photon_dir_opt;
            auto photon_result = dcpdoctor::run_photon(dir, photon_config);
            auto photon_notes = dcpdoctor::photon_to_notes(photon_result, dir);
            for (auto& note : photon_notes)
                result.add(std::move(note));
        }

        // Netflix delivery spec
        if (netflix_mode) {
            auto netflix_result = dcpdoctor::check_netflix_delivery(dir);
            auto netflix_notes = dcpdoctor::netflix_to_notes(netflix_result, dir);
            for (auto& note : netflix_notes)
                result.add(std::move(note));
        }

        // Accessibility tracks
        if (accessibility_check) {
            auto acc_notes = dcpdoctor::check_accessibility(dir);
            for (auto& note : acc_notes)
                result.add(std::move(note));
        }

        // HDR metadata detection
        if (hdr_check) {
            std::error_code ec2;
            for (auto& mxf_entry : fs::directory_iterator(dir, ec2)) {
                if (!mxf_entry.is_regular_file()) continue;
                if (mxf_entry.path().extension() != ".mxf") continue;
                auto hdr_info = dcpdoctor::detect_hdr_metadata(mxf_entry.path());
                if (hdr_info.detected) {
                    auto hdr_notes = dcpdoctor::check_hdr_compliance(hdr_info, mxf_entry.path());
                    for (auto& note : hdr_notes)
                        result.add(std::move(note));
                    break;  // Only check first picture MXF
                }
            }
        }

        // Deep Atmos IAB inspection
        if (atmos_deep) {
            std::error_code ec2;
            for (auto& mxf_entry : fs::directory_iterator(dir, ec2)) {
                if (!mxf_entry.is_regular_file()) continue;
                if (mxf_entry.path().extension() != ".mxf") continue;
                auto atmos_info = dcpdoctor::parse_atmos_iab(mxf_entry.path());
                if (atmos_info.detected) {
                    auto atmos_notes = dcpdoctor::check_atmos_compliance(atmos_info, mxf_entry.path());
                    for (auto& note : atmos_notes)
                        result.add(std::move(note));
                }
            }
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

            // Fix suggestions
            if (suggest_fixes_flag && !result.notes.empty()) {
                auto fixes = dcpdoctor::suggest_fixes(result.notes);
                if (!fixes.empty()) {
                    std::cout << "\nSuggested Fixes:\n";
                    for (size_t fi = 0; fi < fixes.size(); ++fi) {
                        std::cout << "  " << (fi + 1) << ". " << fixes[fi].description;
                        if (!fixes[fi].command.empty())
                            std::cout << "\n     Command: " << fixes[fi].command;
                        std::cout << "\n";
                    }
                }
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
