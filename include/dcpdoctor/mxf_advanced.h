#pragma once

#include "dcpdoctor/dcpdoctor.h"
#include <filesystem>
#include <string>
#include <vector>
#include <cstdint>

namespace dcpdoctor {

/// MXF partition structure info
struct MxfPartitionInfo {
    bool valid = false;
    bool has_header_partition = false;
    bool has_body_partition = false;
    bool has_footer_partition = false;
    bool closed_complete = false;       // Closed & Complete partition status
    uint32_t body_partition_count = 0;
    uint64_t header_size = 0;
    uint64_t footer_offset = 0;
    std::string error;
};

/// Validate MXF internal partition structure
MxfPartitionInfo validate_mxf_partitions(const std::filesystem::path& mxf_path);

/// Generate notes from partition validation
std::vector<Note> check_mxf_partitions(const MxfPartitionInfo& info,
                                        const std::filesystem::path& mxf_path);

/// Dolby Vision metadata detection
struct DolbyVisionInfo {
    bool detected = false;
    uint8_t profile = 0;         // DV profile (5, 8, etc.)
    uint8_t bl_signal = 0;       // Base layer signal
    bool rpu_present = false;    // Reference Processing Unit
    std::string version;
};

/// Detect Dolby Vision metadata in MXF
DolbyVisionInfo detect_dolby_vision(const std::filesystem::path& mxf_path);

/// DTS:X Immersive Audio detection
struct DtsxInfo {
    bool detected = false;
    uint8_t channel_count = 0;
    bool immersive = false;
    std::string version;
};

/// Detect DTS:X tracks in MXF
DtsxInfo detect_dtsx(const std::filesystem::path& mxf_path);

/// Generate notes for DTS:X validation
std::vector<Note> check_dtsx_compliance(const DtsxInfo& info,
                                         const std::filesystem::path& mxf_path);

} // namespace dcpdoctor
