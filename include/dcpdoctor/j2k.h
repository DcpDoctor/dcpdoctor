#pragma once

#include "dcpdoctor/dcpdoctor.h"
#include <filesystem>
#include <vector>

namespace dcpdoctor {

/// Check J2K bitrate compliance using file size and frame metadata
/// Returns notes for any bitrate violations
std::vector<Note> check_j2k_bitrate(const std::filesystem::path& mxf_path,
                                     uint64_t frame_count,
                                     uint32_t frame_rate_num,
                                     uint32_t frame_rate_den,
                                     uint32_t width, uint32_t height);

/// Validate J2K codestream header markers (SOC, SIZ, COD)
/// Uses Grok library if available, otherwise parses raw markers
struct J2kInfo {
    bool valid = false;
    uint32_t width = 0;
    uint32_t height = 0;
    uint8_t num_components = 0;
    uint8_t bit_depth = 0;
    uint8_t num_resolutions = 0;
    bool irreversible = false;  // true = lossy (9/7), false = lossless (5/3)
    uint16_t rsiz = 0;         // Profile marker (Cinema2K, Cinema4K, etc.)
    std::string error;
};

/// Parse a raw J2K codestream header (from first frame in MXF)
J2kInfo parse_j2k_header(const uint8_t* data, size_t len);

/// RSIZ profile constants (SMPTE 377-1, ISO 15444-1)
constexpr uint16_t RSIZ_CINEMA_2K = 3;
constexpr uint16_t RSIZ_CINEMA_4K = 4;
constexpr uint16_t RSIZ_CINEMA_2K_S3D = 5;
constexpr uint16_t RSIZ_CINEMA_4K_S3D = 6;

} // namespace dcpdoctor
