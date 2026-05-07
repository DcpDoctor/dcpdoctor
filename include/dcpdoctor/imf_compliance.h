#pragma once

#include <filesystem>
#include <string>
#include <vector>
#include <cstdint>

namespace dcpdoctor
{

enum class ImfComplianceTarget
{
  Netflix,
  Disney,
  Amazon,
  Apple,
  Cinema2K,
  Cinema4K,
  BroadcastHD,
  BroadcastUHD,
};

struct ImfComplianceCheck
{
  std::string rule;
  std::string description;
  bool passed = false;
  std::string actual_value;
  std::string expected_value;
};

struct ImfComplianceOptions
{
  std::filesystem::path imp_dir;
  ImfComplianceTarget target;
  bool strict = true; // fail on warnings too
};

struct ImfComplianceResult
{
  ImfComplianceTarget target;
  std::vector<ImfComplianceCheck> checks;
  uint32_t passed = 0;
  uint32_t failed = 0;
  uint32_t warnings = 0;
  bool compliant = false;
  bool success = false;
  std::string error;
};

// Run platform-specific compliance checks on an IMP
ImfComplianceResult check_imf_compliance(const ImfComplianceOptions& opts);

// Get human-readable target name
std::string imf_compliance_target_name(ImfComplianceTarget target);

} // namespace dcpdoctor
