#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>

namespace dcpdoctor {

enum class EssenceType { unknown, jpeg2000, mpeg2, pcm_audio, timed_text };

struct PictureDescriptor {
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t frame_rate_num = 0;
    uint32_t frame_rate_den = 0;
    uint32_t bit_depth = 0;
    uint64_t frame_count = 0;
    EssenceType type = EssenceType::unknown;
};

struct SoundDescriptor {
    uint32_t sample_rate = 0;
    uint32_t channels = 0;
    uint32_t bit_depth = 0;
    uint64_t duration = 0;
    EssenceType type = EssenceType::unknown;
};

struct MxfInfo {
    bool valid = false;
    EssenceType essence_type = EssenceType::unknown;
    std::optional<PictureDescriptor> picture;
    std::optional<SoundDescriptor> sound;
    std::string error;
};

/// Read MXF file metadata without decoding essence
MxfInfo read_mxf_info(const std::filesystem::path& path);

/// Return human-readable string for essence type
std::string_view essence_type_str(EssenceType t);

} // namespace dcpdoctor
