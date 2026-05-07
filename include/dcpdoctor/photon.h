#pragma once

#include "dcpdoctor/dcpdoctor.h"
#include <filesystem>
#include <string>
#include <vector>
#include <optional>

namespace dcpdoctor
{

/// Configuration for Photon integration
struct PhotonConfig
{
  std::filesystem::path photon_dir; // Path to Photon source/build dir
  std::filesystem::path java_executable; // Path to java binary (default: "java")
  bool build_if_needed = true; // Auto-build Photon if JARs missing
  int timeout_seconds = 300; // Max time for Photon analysis
};

/// Single error/warning from Photon output
struct PhotonError
{
  enum class Level
  {
    fatal,
    error,
    warning
  };
  Level level = Level::error;
  std::string code; // e.g. "IMF_AM_ERROR", "IMF_CPL_ERROR"
  std::string description;
  std::string file; // Which file the error relates to
};

/// Result of a Photon IMF validation run
struct PhotonResult
{
  bool success = false; // Process ran successfully
  bool photon_available = false; // Photon is installed/built
  int exit_code = -1;
  int error_count = 0;
  int warning_count = 0;
  std::vector<PhotonError> errors;
  std::string raw_output; // Full stdout+stderr
  std::string error_message; // If process failed to run
};

/// Detect if Photon is available (JARs built)
bool photon_available(const PhotonConfig& config);

/// Build Photon from source (runs gradle)
bool build_photon(const PhotonConfig& config);

/// Run Photon IMPAnalyzer on an IMF package directory
PhotonResult run_photon(const std::filesystem::path& imf_dir, const PhotonConfig& config);

/// Convert Photon results to dcpdoctor Notes
std::vector<Note> photon_to_notes(const PhotonResult& result, const std::filesystem::path& imf_dir);

/// Get default PhotonConfig (looks for extern/photon, PHOTON_DIR env var)
PhotonConfig default_photon_config();

} // namespace dcpdoctor
