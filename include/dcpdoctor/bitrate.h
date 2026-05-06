#pragma once

#include "dcpdoctor/dcpdoctor.h"
#include <filesystem>
#include <vector>
#include <cstdint>

namespace dcpdoctor {

/// Per-frame bitrate statistics
struct FrameBitrateStats {
    bool valid = false;
    uint32_t frame_count = 0;
    uint32_t width = 0;
    uint32_t height = 0;
    double frame_rate = 0.0;
    double avg_bitrate_mbps = 0.0;
    double max_bitrate_mbps = 0.0;
    double min_bitrate_mbps = 0.0;
    uint32_t max_frame_index = 0;
    uint64_t max_frame_bytes = 0;
    uint64_t min_frame_bytes = 0;
    uint64_t total_bytes = 0;
    std::string error;
};

/// Analyze per-frame bitrate using asdcplib
/// Returns detailed stats and validation notes
FrameBitrateStats analyze_picture_bitrate(const std::filesystem::path& mxf_path);

/// Generate bitrate violation notes from stats
std::vector<Note> check_bitrate_compliance(const FrameBitrateStats& stats,
                                            const std::filesystem::path& mxf_path);

/// Deep J2K codestream validation using asdcplib
/// Checks: profile (RSIZ), tile structure, precinct sizes, code-block sizes,
/// number of quality layers, component bit depth, XYZ color space
struct J2kDeepInfo {
    bool valid = false;
    uint16_t rsiz = 0;
    uint32_t tile_width = 0;
    uint32_t tile_height = 0;
    uint8_t num_components = 0;
    uint8_t bit_depth = 0;
    uint8_t num_decomp_levels = 0;
    uint8_t num_quality_layers = 0;
    uint8_t code_block_width = 0;   // exponent (e.g. 5 means 32)
    uint8_t code_block_height = 0;
    bool irreversible = false;
    std::string error;
};

/// Deep-validate the first J2K frame from an MXF file
J2kDeepInfo deep_validate_j2k(const std::filesystem::path& mxf_path);

/// Generate validation notes from deep J2K analysis
std::vector<Note> check_j2k_deep_compliance(const J2kDeepInfo& info,
                                             const std::filesystem::path& mxf_path);

} // namespace dcpdoctor
