#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace dcpdoctor
{

struct ValidationNote
{
  enum class Severity
  {
    error,
    warning,
    info
  };
  Severity severity;
  std::string message;
  std::string context;
};

struct ValidationResult
{
  bool valid = false;
  std::vector<ValidationNote> notes;
  std::string validator_version;
};

// Validate an IMP using Netflix Photon (requires java)
ValidationResult validate_with_photon(const std::filesystem::path& imp_dir);

// Check if Photon is available on the system
bool photon_available();

} // namespace dcpdoctor
