#pragma once

#include "dcpdoctor/dcpdoctor.h"
#include <filesystem>
#include <string>
#include <vector>
#include <chrono>

namespace dcpdoctor
{

/// DKDM (Distribution KDM) info
struct DkdmInfo
{
  bool valid = false;
  std::string cpl_id;
  std::string content_title;
  std::string issuer;
  std::string recipient;
  std::chrono::system_clock::time_point not_valid_before;
  std::chrono::system_clock::time_point not_valid_after;
  bool is_dkdm = false; // true if this is a DKDM (vs regular KDM)
  std::string error;
};

/// Parse DKDM file
DkdmInfo parse_dkdm(const std::filesystem::path& dkdm_path);

/// Validate DKDM
std::vector<Note> validate_dkdm(const std::filesystem::path& dkdm_path);

/// Trusted Device List (TDL) entry
struct TdlEntry
{
  std::string thumbprint; // Certificate thumbprint/fingerprint
  std::string common_name; // CN from certificate
  std::string organization; // O from certificate
  bool active = true;
};

/// Load Trusted Device List from XML file
std::vector<TdlEntry> load_trusted_device_list(const std::filesystem::path& tdl_path);

/// Validate KDM recipient against Trusted Device List
std::vector<Note> validate_kdm_against_tdl(const std::filesystem::path& kdm_path,
                                           const std::vector<TdlEntry>& tdl);

/// KDM Annotation scheme validation
struct KdmAnnotation
{
  std::string facility_name;
  std::string screen_name;
  std::string content_title;
  std::string valid_from;
  std::string valid_to;
  bool valid_format = false;
};

/// Parse and validate KDM annotation text
KdmAnnotation parse_kdm_annotation(const std::string& annotation_text);

/// Validate KDM with timezone awareness
struct TimezoneKdmResult
{
  bool valid_now = false;
  bool valid_now_utc = false;
  std::string local_timezone;
  int utc_offset_hours = 0;
  std::chrono::system_clock::time_point local_start;
  std::chrono::system_clock::time_point local_end;
};

/// Check KDM validity in a specific timezone (offset from UTC in hours)
TimezoneKdmResult check_kdm_timezone(const std::filesystem::path& kdm_path,
                                     int utc_offset_hours = 0);

} // namespace dcpdoctor
