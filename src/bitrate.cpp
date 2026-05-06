#include "dcpdoctor/bitrate.h"
#include "dcpdoctor/j2k.h"
#include <AS_DCP.h>
#include <KM_fileio.h>
#include <algorithm>

namespace dcpdoctor {

FrameBitrateStats analyze_picture_bitrate(const std::filesystem::path& mxf_path) {
    FrameBitrateStats stats;

    Kumu::FileReaderFactory defaultFactory;
    ASDCP::JP2K::MXFReader reader(defaultFactory);

    auto result = reader.OpenRead(mxf_path.string());
    if (ASDCP_FAILURE(result)) {
        stats.error = "Failed to open MXF: " + std::string(ASDCP::Result_t(result).Label());
        return stats;
    }

    ASDCP::JP2K::PictureDescriptor pdesc;
    result = reader.FillPictureDescriptor(pdesc);
    if (ASDCP_FAILURE(result)) {
        stats.error = "Failed to read picture descriptor";
        return stats;
    }

    stats.frame_count = pdesc.ContainerDuration;
    stats.width = pdesc.StoredWidth;
    stats.height = pdesc.StoredHeight;
    stats.frame_rate = double(pdesc.EditRate.Numerator) / double(pdesc.EditRate.Denominator);

    if (stats.frame_count == 0 || stats.frame_rate <= 0) {
        stats.error = "Invalid frame count or rate";
        return stats;
    }

    // Read each frame to get exact sizes
    ASDCP::JP2K::FrameBuffer frame_buf;
    frame_buf.Capacity(16 * 1024 * 1024);  // 16MB max frame

    stats.min_frame_bytes = UINT64_MAX;
    stats.max_frame_bytes = 0;
    stats.total_bytes = 0;

    for (uint32_t i = 0; i < stats.frame_count; ++i) {
        result = reader.ReadFrame(i, frame_buf);
        if (ASDCP_FAILURE(result)) break;

        uint64_t frame_size = frame_buf.Size();
        stats.total_bytes += frame_size;

        if (frame_size > stats.max_frame_bytes) {
            stats.max_frame_bytes = frame_size;
            stats.max_frame_index = i;
        }
        if (frame_size < stats.min_frame_bytes) {
            stats.min_frame_bytes = frame_size;
        }
    }

    if (stats.min_frame_bytes == UINT64_MAX)
        stats.min_frame_bytes = 0;

    // Calculate bitrates
    double frame_duration_sec = 1.0 / stats.frame_rate;
    stats.avg_bitrate_mbps = (double(stats.total_bytes) * 8.0) /
                             (double(stats.frame_count) * frame_duration_sec * 1000000.0);
    stats.max_bitrate_mbps = (double(stats.max_frame_bytes) * 8.0) /
                             (frame_duration_sec * 1000000.0);
    stats.min_bitrate_mbps = (double(stats.min_frame_bytes) * 8.0) /
                             (frame_duration_sec * 1000000.0);

    stats.valid = true;
    return stats;
}

std::vector<Note> check_bitrate_compliance(const FrameBitrateStats& stats,
                                            const std::filesystem::path& mxf_path) {
    std::vector<Note> notes;

    if (!stats.valid) return notes;

    // DCI limits: 250 Mbps for 2K, 500 Mbps for 4K
    double max_allowed = (stats.width > 2048) ? 500.0 : 250.0;

    // Check max frame bitrate (instantaneous)
    if (stats.max_bitrate_mbps > max_allowed) {
        notes.push_back({Severity::error, Code::j2k_bitrate_exceeded,
                        "Peak frame bitrate " + std::to_string(int(stats.max_bitrate_mbps)) +
                        " Mbps exceeds DCI limit of " + std::to_string(int(max_allowed)) +
                        " Mbps (frame #" + std::to_string(stats.max_frame_index) + ")",
                        mxf_path});
    } else if (stats.max_bitrate_mbps > max_allowed * 0.95) {
        notes.push_back({Severity::warning, Code::j2k_bitrate_exceeded,
                        "Peak frame bitrate " + std::to_string(int(stats.max_bitrate_mbps)) +
                        " Mbps is near DCI limit of " + std::to_string(int(max_allowed)) +
                        " Mbps", mxf_path});
    }

    // Check average
    if (stats.avg_bitrate_mbps > max_allowed) {
        notes.push_back({Severity::error, Code::j2k_bitrate_exceeded,
                        "Average bitrate " + std::to_string(int(stats.avg_bitrate_mbps)) +
                        " Mbps exceeds DCI limit", mxf_path});
    }

    return notes;
}

J2kDeepInfo deep_validate_j2k(const std::filesystem::path& mxf_path) {
    J2kDeepInfo info;

    Kumu::FileReaderFactory defaultFactory;
    ASDCP::JP2K::MXFReader reader(defaultFactory);

    auto result = reader.OpenRead(mxf_path.string());
    if (ASDCP_FAILURE(result)) {
        info.error = "Failed to open MXF";
        return info;
    }

    ASDCP::JP2K::PictureDescriptor pdesc;
    result = reader.FillPictureDescriptor(pdesc);
    if (ASDCP_FAILURE(result)) {
        info.error = "Failed to read picture descriptor";
        return info;
    }

    info.rsiz = pdesc.Rsize;
    info.tile_width = pdesc.XTsize;
    info.tile_height = pdesc.YTsize;
    info.num_components = pdesc.Csize;
    info.num_decomp_levels = pdesc.CodingStyleDefault.SPcod.DecompositionLevels;
    info.code_block_width = pdesc.CodingStyleDefault.SPcod.CodeblockWidth;
    info.code_block_height = pdesc.CodingStyleDefault.SPcod.CodeblockHeight;
    info.num_quality_layers = (pdesc.CodingStyleDefault.SGcod.NumberOfLayers[0] << 8)
                            | pdesc.CodingStyleDefault.SGcod.NumberOfLayers[1];

    // Bit depth from first component
    if (pdesc.Csize > 0) {
        info.bit_depth = pdesc.ImageComponents[0].Ssize + 1;
    }

    // Check irreversibility from transformation type
    // SPcod.Transformation: 0 = 9-7 irreversible, 1 = 5-3 reversible
    info.irreversible = (pdesc.CodingStyleDefault.SPcod.Transformation == 0);

    info.valid = true;
    return info;
}

std::vector<Note> check_j2k_deep_compliance(const J2kDeepInfo& info,
                                             const std::filesystem::path& mxf_path) {
    std::vector<Note> notes;

    if (!info.valid) return notes;

    // DCI requires 3 components (XYZ)
    if (info.num_components != 3) {
        notes.push_back({Severity::error, Code::j2k_invalid_component_count,
                        "J2K has " + std::to_string(info.num_components) +
                        " components (DCI requires 3 for XYZ)", mxf_path});
    }

    // DCI requires 12-bit components
    if (info.bit_depth != 12 && info.bit_depth != 0) {
        notes.push_back({Severity::warning, Code::j2k_invalid_profile,
                        "J2K bit depth is " + std::to_string(info.bit_depth) +
                        " (DCI standard is 12)", mxf_path});
    }

    // DCI requires irreversible (9-7) wavelet
    if (!info.irreversible) {
        notes.push_back({Severity::warning, Code::j2k_invalid_profile,
                        "J2K uses reversible (5/3) wavelet; DCI requires irreversible (9/7)",
                        mxf_path});
    }

    // Check profile marker (RSIZ)
    if (info.rsiz != RSIZ_CINEMA_2K && info.rsiz != RSIZ_CINEMA_4K &&
        info.rsiz != RSIZ_CINEMA_2K_S3D && info.rsiz != RSIZ_CINEMA_4K_S3D &&
        info.rsiz != 0) {
        notes.push_back({Severity::info, Code::j2k_invalid_profile,
                        "J2K profile RSIZ=" + std::to_string(info.rsiz) +
                        " is not a Cinema profile (3/4/5/6)", mxf_path});
    }

    // DCI code-block size should be 32x32 (exponent 4) for Cinema 2K or 64x64 for HFR
    // Code-block width/height are stored as exponents minus 2
    uint8_t cb_w = (info.code_block_width & 0x0F) + 2;
    uint8_t cb_h = (info.code_block_height & 0x0F) + 2;
    if (cb_w > 0 && cb_h > 0) {
        uint32_t actual_w = 1u << cb_w;
        uint32_t actual_h = 1u << cb_h;
        if (actual_w != 32 && actual_w != 64) {
            notes.push_back({Severity::info, Code::j2k_invalid_profile,
                            "Code-block width " + std::to_string(actual_w) +
                            " (DCI standard: 32 or 64)", mxf_path});
        }
    }

    // Number of decomposition levels: DCI typically requires 5 for 2K, 6 for 4K
    if (info.num_decomp_levels > 0 && info.num_decomp_levels < 5) {
        notes.push_back({Severity::warning, Code::j2k_invalid_profile,
                        "Only " + std::to_string(info.num_decomp_levels) +
                        " decomposition levels (DCI recommends >= 5)", mxf_path});
    }

    return notes;
}

} // namespace dcpdoctor
