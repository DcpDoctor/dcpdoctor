#pragma once

#include "dcpdoctor/dcpdoctor.h"
#include <filesystem>
#include <string>
#include <vector>

namespace dcpdoctor {

/// Theater server compatibility profile
struct TheaterProfile {
    std::string name;           // e.g. "Dolby IMS3000"
    std::string vendor;         // e.g. "Dolby", "Barco", "Christie", "GDC"
    bool requires_bv21 = false;
    bool supports_interop = true;
    bool supports_hfr = false;  // > 24fps
    bool supports_4k = false;
    bool supports_atmos = false;
    int max_channels = 16;
    int max_bitrate_mbps = 250;
    std::vector<std::string> known_issues;  // Known quirks
};

/// Get built-in theater profiles
std::vector<TheaterProfile> get_theater_profiles();

/// Find profile by name (case-insensitive partial match)
const TheaterProfile* find_profile(const std::string& query);

/// Validate DCP against a specific theater profile
std::vector<Note> check_theater_compatibility(
    const std::filesystem::path& dcp_dir,
    const VerifyResult& result,
    const TheaterProfile& profile);

} // namespace dcpdoctor
