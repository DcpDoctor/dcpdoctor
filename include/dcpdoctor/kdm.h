#pragma once

#include "dcpdoctor/dcpdoctor.h"
#include <filesystem>
#include <string>
#include <vector>
#include <chrono>

namespace dcpdoctor
{

struct KdmInfo
{
  bool valid = false;
  std::string cpl_id;
  std::string content_title;
  std::string recipient_cert_thumbprint;
  std::chrono::system_clock::time_point not_valid_before;
  std::chrono::system_clock::time_point not_valid_after;
  bool is_expired = false;
  bool is_not_yet_valid = false;
  std::string error;
};

/// Parse and validate a KDM XML file
KdmInfo parse_kdm(const std::filesystem::path& kdm_path);

/// Validate KDM against a DCP (check CPL reference, validity period, certs)
std::vector<Note> validate_kdm(const std::filesystem::path& kdm_path,
                               const std::filesystem::path& dcp_dir);

} // namespace dcpdoctor
