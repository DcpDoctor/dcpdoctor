#include "dcpdoctor/photon.h"
#include "dcpdoctor/platform.h"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <regex>
#include <sstream>

namespace dcpdoctor {
namespace fs = std::filesystem;

PhotonConfig default_photon_config() {
    PhotonConfig config;

    // Check PHOTON_DIR environment variable first
    if (auto env = std::getenv("PHOTON_DIR")) {
        config.photon_dir = env;
    } else {
        // Default: look relative to executable for extern/photon
        // Try common locations
        auto candidates = {
            fs::path("extern/photon"),
            fs::path("../extern/photon"),
            fs::path("/usr/local/share/photon"),
        };
        for (const auto& p : candidates) {
            std::error_code ec;
            if (fs::exists(p / "build.gradle", ec)) {
                config.photon_dir = fs::canonical(p, ec);
                break;
            }
        }
    }

    // Check JAVA_HOME or just use "java" from PATH
    if (auto env = std::getenv("JAVA_HOME")) {
        config.java_executable = fs::path(env) / "bin" / "java";
    } else {
        config.java_executable = "java";
    }

    return config;
}

bool photon_available(const PhotonConfig& config) {
    if (config.photon_dir.empty()) return false;

    std::error_code ec;
    auto libs_dir = config.photon_dir / "build" / "libs";
    if (!fs::exists(libs_dir, ec)) return false;

    // Check if there are JAR files in build/libs
    for (auto& entry : fs::directory_iterator(libs_dir, ec)) {
        if (entry.path().extension() == ".jar") return true;
    }
    return false;
}

bool build_photon(const PhotonConfig& config) {
    if (config.photon_dir.empty()) return false;

    std::error_code ec;
    if (!fs::exists(config.photon_dir / "gradlew", ec)) return false;

    // Run gradle build
    std::string cmd = "cd " + config.photon_dir.string() +
                      " && ./gradlew build getDependencies -x test 2>&1";
    auto* pipe = DCPDOCTOR_POPEN(cmd.c_str(), "r");
    if (!pipe) return false;

    // Read output (discard for now)
    std::array<char, 4096> buf;
    while (fgets(buf.data(), buf.size(), pipe)) {}

    int status = DCPDOCTOR_PCLOSE(pipe);
    return (status == 0);
}

namespace {

PhotonError::Level parse_level(const std::string& level_str) {
    if (level_str == "FATAL") return PhotonError::Level::fatal;
    if (level_str == "WARNING") return PhotonError::Level::warning;
    return PhotonError::Level::error;
}

std::vector<PhotonError> parse_photon_output(const std::string& output,
                                              const std::string& current_file) {
    std::vector<PhotonError> errors;

    // Photon outputs lines like:
    //   "ERROR-<description> - Photon vX.X.X"
    //   "WARNING-<description> - Photon vX.X.X"
    // Preceded by:
    //   "<filename> has N errors and M warnings"

    std::istringstream stream(output);
    std::string line;
    std::string active_file = current_file;

    // Pattern: "filename has N errors and M warnings"
    std::regex file_pattern(R"((.+)\s+has\s+(\d+)\s+errors?\s+and\s+(\d+)\s+warnings?)");
    // Pattern: error/warning lines (indented with tabs)
    std::regex error_pattern(R"(\s*(ERROR|WARNING|FATAL)\s*-\s*(.+))");

    while (std::getline(stream, line)) {
        std::smatch match;

        if (std::regex_search(line, match, file_pattern)) {
            active_file = match[1].str();
        } else if (std::regex_search(line, match, error_pattern)) {
            PhotonError err;
            err.level = parse_level(match[1].str());
            err.description = match[2].str();
            err.file = active_file;

            // Strip " - Photon vX.X.X" suffix if present
            auto photon_pos = err.description.find(" - Photon v");
            if (photon_pos != std::string::npos) {
                err.description = err.description.substr(0, photon_pos);
            }

            // Try to extract error code from description
            // Photon includes code info in some messages
            if (err.description.find("AssetMap") != std::string::npos)
                err.code = "IMF_AM_ERROR";
            else if (err.description.find("PackingList") != std::string::npos ||
                     err.description.find("PKL") != std::string::npos)
                err.code = "IMF_PKL_ERROR";
            else if (err.description.find("CPL") != std::string::npos ||
                     err.description.find("Composition") != std::string::npos)
                err.code = "IMF_CPL_ERROR";
            else if (err.description.find("MXF") != std::string::npos ||
                     err.description.find("Track") != std::string::npos)
                err.code = "IMF_ESSENCE_ERROR";
            else
                err.code = "IMF_ERROR";

            errors.push_back(std::move(err));
        }
    }

    return errors;
}

} // namespace

PhotonResult run_photon(const fs::path& imf_dir, const PhotonConfig& config) {
    PhotonResult result;

    if (config.photon_dir.empty()) {
        result.error_message = "Photon directory not configured";
        return result;
    }

    if (!photon_available(config)) {
        if (config.build_if_needed) {
            if (!build_photon(config)) {
                result.error_message = "Failed to build Photon (run 'gradlew build' manually in " +
                                       config.photon_dir.string() + ")";
                return result;
            }
        } else {
            result.error_message = "Photon not built (run 'gradlew build getDependencies' in " +
                                   config.photon_dir.string() + ")";
            return result;
        }
    }

    result.photon_available = true;

    // Build classpath
    auto libs_dir = config.photon_dir / "build" / "libs";
    std::string classpath = libs_dir.string() + "/*";

    // Construct command
    std::string cmd = config.java_executable.string() +
                      " -cp \"" + classpath + ":\"" +
                      " com.netflix.imflibrary.app.IMPAnalyzer" +
                      " \"" + imf_dir.string() + "\"" +
                      " 2>&1";

    // Execute with timeout
    auto* pipe = DCPDOCTOR_POPEN(cmd.c_str(), "r");
    if (!pipe) {
        result.error_message = "Failed to execute Photon (java not found?)";
        return result;
    }

    // Read all output
    std::ostringstream output;
    std::array<char, 4096> buf;
    while (fgets(buf.data(), buf.size(), pipe)) {
        output << buf.data();
    }

    result.exit_code = DCPDOCTOR_PCLOSE(pipe);
    result.raw_output = output.str();
    result.success = true;

    // Parse output into structured errors
    result.errors = parse_photon_output(result.raw_output, imf_dir.string());

    for (const auto& err : result.errors) {
        if (err.level == PhotonError::Level::warning)
            result.warning_count++;
        else
            result.error_count++;
    }

    return result;
}

std::vector<Note> photon_to_notes(const PhotonResult& result,
                                   const fs::path& imf_dir) {
    std::vector<Note> notes;

    if (!result.success) {
        notes.push_back(Note{Severity::warning, Code::missing_assetmap,
                        "Photon IMF validation unavailable: " + result.error_message,
                        imf_dir});
        return notes;
    }

    if (result.errors.empty()) {
        notes.push_back(Note{Severity::info, Code::missing_assetmap,
                        "Photon: IMF package validates cleanly (no errors)",
                        imf_dir});
        return notes;
    }

    // Summary note
    notes.push_back(Note{Severity::info, Code::missing_assetmap,
                    "Photon: " + std::to_string(result.error_count) + " errors, " +
                    std::to_string(result.warning_count) + " warnings",
                    imf_dir});

    // Convert each Photon error to a dcpdoctor Note
    for (const auto& err : result.errors) {
        Severity sev;
        switch (err.level) {
        case PhotonError::Level::fatal: sev = Severity::error; break;
        case PhotonError::Level::error: sev = Severity::error; break;
        case PhotonError::Level::warning: sev = Severity::warning; break;
        }

        // Map Photon error codes to dcpdoctor codes
        Code code = Code::missing_assetmap; // default
        if (err.code == "IMF_AM_ERROR") code = Code::missing_assetmap;
        else if (err.code == "IMF_PKL_ERROR") code = Code::missing_pkl;
        else if (err.code == "IMF_CPL_ERROR") code = Code::missing_cpl;
        else if (err.code == "IMF_ESSENCE_ERROR") code = Code::mxf_invalid_structure;

        fs::path file_path = err.file.empty() ? imf_dir : fs::path(err.file);
        notes.push_back(Note{sev, code,
                        "[Photon] " + err.description, file_path});
    }

    return notes;
}

} // namespace dcpdoctor
