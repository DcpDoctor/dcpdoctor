#pragma once

#include "dcpdoctor/dcpdoctor.h"
#include <filesystem>
#include <string>
#include <vector>
#include <cstdint>

namespace dcpdoctor {

/// HFR (High Frame Rate) validation info
struct HfrInfo {
    double frame_rate = 0.0;
    bool is_hfr = false;        // > 30fps
    bool is_ultra_hfr = false;  // > 60fps
    bool bv21_compliant = false;
    std::string edit_rate_str;
};

/// Validate HFR-specific DCI constraints
std::vector<Note> check_hfr_compliance(const std::filesystem::path& cpl_path);

/// Multi-CPL analysis result
struct MultiCplInfo {
    struct CplEntry {
        std::string id;
        std::string content_title;
        std::string edit_rate;
        uint32_t reel_count = 0;
        uint64_t total_duration = 0;
        std::string type;  // "main", "trailer", "advertisement", etc.
    };

    std::vector<CplEntry> cpls;
    bool consistent_frame_rate = true;
    bool consistent_resolution = true;
    std::vector<std::string> shared_assets;  // Assets used by multiple CPLs
};

/// Analyze multiple CPLs in a DCP
MultiCplInfo analyze_multi_cpl(const std::filesystem::path& dcp_dir);

/// 3D Stereoscopic alignment info
struct Stereo3dInfo {
    bool is_stereo = false;
    bool eyes_aligned = true;
    uint64_t left_duration = 0;
    uint64_t right_duration = 0;
    int64_t eye_offset = 0;  // Frame offset between eyes
    bool has_stereo_metadata = false;
    std::string stereo_type;  // "side-by-side", "top-bottom", "frame-sequential"
};

/// Deep 3D stereoscopic validation
Stereo3dInfo analyze_stereo3d(const std::filesystem::path& cpl_path);

/// Generate notes for 3D alignment issues
std::vector<Note> check_stereo3d_compliance(const Stereo3dInfo& info,
                                             const std::filesystem::path& cpl_path);

/// CPL version/supplemental chain info
struct CplChainEntry {
    std::string cpl_id;
    std::string content_title;
    std::string content_version_id;
    std::string content_version_label;
    bool is_supplemental = false;
    std::string original_cpl_id;  // For supplementals, the original CPL
};

/// Trace supplemental DCP chain
std::vector<CplChainEntry> trace_cpl_chain(const std::filesystem::path& dcp_dir);

/// Validate supplemental chain integrity
std::vector<Note> check_cpl_chain(const std::vector<CplChainEntry>& chain,
                                   const std::filesystem::path& dcp_dir);

} // namespace dcpdoctor
