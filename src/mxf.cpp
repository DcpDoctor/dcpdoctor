#include "dcpdoctor/mxf.h"
#include <array>
#include <cstring>
#include <fstream>
#include <vector>
#include <algorithm>

namespace dcpdoctor {
namespace {

// SMPTE 336M Universal Label prefix (first 4 bytes of all MXF ULs)
constexpr std::array<uint8_t, 4> UL_PREFIX = {0x06, 0x0e, 0x2b, 0x34};

// Partition Pack UL prefix (bytes 5-8 identify partition pack)
constexpr std::array<uint8_t, 4> PARTITION_PACK_KEY = {0x02, 0x05, 0x01, 0x01};

// Header Metadata key (Primer Pack)
constexpr std::array<uint8_t, 4> PRIMER_PACK_KEY = {0x01, 0x01, 0x05, 0x01};

// Set/Pack keys for descriptors (bytes 5-8 after UL prefix)
constexpr std::array<uint8_t, 4> PREFACE_KEY = {0x02, 0x53, 0x01, 0x01};

// CDCI Picture Essence Descriptor (SMPTE 377M)
constexpr std::array<uint8_t, 4> CDCI_DESCRIPTOR_KEY = {0x02, 0x53, 0x01, 0x01};

// RGBA Picture Essence Descriptor
constexpr std::array<uint8_t, 8> RGBA_DESCRIPTOR_UL = {
    0x02, 0x53, 0x00, 0x00, 0x0d, 0x01, 0x01, 0x01};

// Generic Picture Essence Descriptor item key prefix
constexpr uint16_t TAG_STORED_WIDTH = 0x3203;
constexpr uint16_t TAG_STORED_HEIGHT = 0x3202;
constexpr uint16_t TAG_SAMPLE_RATE = 0x3001;
constexpr uint16_t TAG_CONTAINER_DURATION = 0x3002;
constexpr uint16_t TAG_FRAME_LAYOUT = 0x320C;
constexpr uint16_t TAG_COMPONENT_DEPTH = 0x3301;

// Sound descriptor tags
constexpr uint16_t TAG_AUDIO_SAMPLING_RATE = 0x3D01;
constexpr uint16_t TAG_CHANNEL_COUNT = 0x3D07;
constexpr uint16_t TAG_QUANTIZATION_BITS = 0x3D01;  // context-dependent

// Essence container labels (partial, bytes 13-16)
constexpr std::array<uint8_t, 4> JPEG2000_EC = {0x01, 0x02, 0x01, 0x01};  // after prefix
constexpr std::array<uint8_t, 4> PCM_EC = {0x01, 0x06, 0x01, 0x00};

using UL = std::array<uint8_t, 16>;

// Known essence container ULs (partial match on bytes 1-14)
bool is_jpeg2000_container(const UL& ul) {
    // MXF-GC JPEG 2000: 06.0e.2b.34.04.01.01.07.0d.01.03.01.02.0c.01.00
    return ul[12] == 0x02 && ul[13] == 0x0c;
}

bool is_pcm_container(const UL& ul) {
    // MXF-GC AES3/BWF: 06.0e.2b.34.04.01.01.01.0d.01.03.01.02.06.xx.xx
    return ul[12] == 0x02 && ul[13] == 0x06;
}

bool is_mpeg2_container(const UL& ul) {
    // MXF-GC MPEG2: 06.0e.2b.34.04.01.01.01.0d.01.03.01.02.04.xx.xx
    return ul[12] == 0x02 && ul[13] == 0x04;
}

bool is_timed_text_container(const UL& ul) {
    // MXF-GC TimedText: 06.0e.2b.34.04.01.01.xx.0d.01.03.01.02.0d.xx.xx
    return ul[12] == 0x02 && ul[13] == 0x0d;
}

// Read BER-encoded length from stream
uint64_t read_ber_length(std::ifstream& f) {
    uint8_t first = 0;
    f.read(reinterpret_cast<char*>(&first), 1);
    if (!f) return 0;

    if (first < 0x80) {
        return first;
    }

    int num_bytes = first & 0x7f;
    if (num_bytes > 8) return 0;

    uint64_t len = 0;
    for (int i = 0; i < num_bytes; ++i) {
        uint8_t b = 0;
        f.read(reinterpret_cast<char*>(&b), 1);
        if (!f) return 0;
        len = (len << 8) | b;
    }
    return len;
}

uint16_t read_u16_be(const uint8_t* p) {
    return (uint16_t(p[0]) << 8) | p[1];
}

uint32_t read_u32_be(const uint8_t* p) {
    return (uint32_t(p[0]) << 24) | (uint32_t(p[1]) << 16) |
           (uint32_t(p[2]) << 8) | p[3];
}

uint64_t read_u64_be(const uint8_t* p) {
    return (uint64_t(read_u32_be(p)) << 32) | read_u32_be(p + 4);
}

// Parse a local set (SMPTE 377M section 7.4)
// Format: tag(2 bytes) + length(2 bytes) + value
struct LocalTag {
    uint16_t tag;
    std::vector<uint8_t> value;
};

std::vector<LocalTag> parse_local_set(const uint8_t* data, size_t len) {
    std::vector<LocalTag> tags;
    size_t pos = 0;
    while (pos + 4 <= len) {
        uint16_t tag = read_u16_be(data + pos);
        uint16_t vlen = read_u16_be(data + pos + 2);
        pos += 4;
        if (pos + vlen > len) break;
        tags.push_back({tag, {data + pos, data + pos + vlen}});
        pos += vlen;
    }
    return tags;
}

// Check if this UL is a picture essence descriptor
// CDCI: 06.0e.2b.34.02.53.01.01.0d.01.01.01.01.01.28.00
// RGBA: 06.0e.2b.34.02.53.01.01.0d.01.01.01.01.01.29.00
bool is_picture_descriptor_ul(const UL& ul) {
    if (std::memcmp(ul.data(), UL_PREFIX.data(), 4) != 0) return false;
    if (ul[4] != 0x02 || ul[5] != 0x53) return false;
    // Check for CDCI (0x28) or RGBA (0x29) descriptor
    return (ul[13] == 0x28 || ul[13] == 0x29);
}

// Check if this UL is a sound essence descriptor
// Generic Sound: 06.0e.2b.34.02.53.01.01.0d.01.01.01.01.01.42.00
// Wave PCM: 06.0e.2b.34.02.53.01.01.0d.01.01.01.01.01.48.00
bool is_sound_descriptor_ul(const UL& ul) {
    if (std::memcmp(ul.data(), UL_PREFIX.data(), 4) != 0) return false;
    if (ul[4] != 0x02 || ul[5] != 0x53) return false;
    return (ul[13] == 0x42 || ul[13] == 0x48);
}

// Detect essence type from Essence Container label in partition pack
EssenceType detect_essence_type(const UL& ec_label) {
    if (is_jpeg2000_container(ec_label)) return EssenceType::jpeg2000;
    if (is_pcm_container(ec_label)) return EssenceType::pcm_audio;
    if (is_mpeg2_container(ec_label)) return EssenceType::mpeg2;
    if (is_timed_text_container(ec_label)) return EssenceType::timed_text;
    return EssenceType::unknown;
}

} // namespace

MxfInfo read_mxf_info(const std::filesystem::path& path) {
    MxfInfo info;

    std::ifstream f(path, std::ios::binary);
    if (!f) {
        info.error = "Cannot open file";
        return info;
    }

    // Read and validate partition pack key (first 16 bytes)
    UL key{};
    f.read(reinterpret_cast<char*>(key.data()), 16);
    if (!f) {
        info.error = "File too small for MXF";
        return info;
    }

    // Verify UL prefix
    if (std::memcmp(key.data(), UL_PREFIX.data(), 4) != 0) {
        info.error = "Not an MXF file (bad UL prefix)";
        return info;
    }

    // Verify partition pack key (bytes 5-8)
    if (key[4] != 0x02 || key[5] != 0x05 || key[6] != 0x01) {
        info.error = "Not an MXF file (not a partition pack)";
        return info;
    }

    info.valid = true;

    // Read partition pack BER length
    uint64_t pack_len = read_ber_length(f);
    if (pack_len == 0 || pack_len > 65536) {
        info.error = "Invalid partition pack length";
        return info;
    }

    // Read partition pack value
    std::vector<uint8_t> pack_data(pack_len);
    f.read(reinterpret_cast<char*>(pack_data.data()), pack_len);
    if (!f) {
        info.error = "Truncated partition pack";
        return info;
    }

    // Parse partition pack fields (SMPTE 377M Table 6)
    // After the initial fixed fields, there's an array of essence container ULs
    if (pack_len >= 88) {
        // Offset 80: Essence Container count (uint32 at offset 80 from pack start)
        // The partition pack has fixed fields totaling 88 bytes minimum
        // Major/Minor version (4), KAG size (4), This partition (8), Previous partition (8),
        // Footer partition (8), Header byte count (8), Index byte count (8), Index SID (4),
        // Body offset (8), Body SID (4), Operational pattern UL (16), then EC labels batch
        size_t ec_offset = 84;  // Approximate offset to EC batch header
        if (ec_offset + 8 <= pack_len) {
            uint32_t ec_count = read_u32_be(pack_data.data() + ec_offset);
            uint32_t ec_item_len = read_u32_be(pack_data.data() + ec_offset + 4);
            ec_offset += 8;
            if (ec_item_len == 16 && ec_count > 0 && ec_offset + 16 <= pack_len) {
                UL ec_label;
                std::memcpy(ec_label.data(), pack_data.data() + ec_offset, 16);
                info.essence_type = detect_essence_type(ec_label);
            }
        }
    }

    // Now scan KLV triplets in the header metadata
    // Limit scan to first 1MB to avoid reading entire file
    constexpr size_t MAX_SCAN = 1024 * 1024;
    size_t scanned = 0;

    while (f && scanned < MAX_SCAN) {
        auto pos = f.tellg();
        UL klv_key{};
        f.read(reinterpret_cast<char*>(klv_key.data()), 16);
        if (!f) break;

        uint64_t klv_len = read_ber_length(f);
        if (klv_len == 0) break;

        auto value_pos = f.tellg();
        scanned = value_pos - std::streamoff(0);

        // Check for picture descriptor
        if (is_picture_descriptor_ul(klv_key) && klv_len <= 65536) {
            std::vector<uint8_t> set_data(klv_len);
            f.read(reinterpret_cast<char*>(set_data.data()), klv_len);
            if (f) {
                PictureDescriptor pic;
                pic.type = (info.essence_type == EssenceType::unknown)
                    ? EssenceType::jpeg2000 : info.essence_type;

                auto tags = parse_local_set(set_data.data(), set_data.size());
                for (const auto& t : tags) {
                    if (t.tag == TAG_STORED_WIDTH && t.value.size() >= 4)
                        pic.width = read_u32_be(t.value.data());
                    else if (t.tag == TAG_STORED_HEIGHT && t.value.size() >= 4)
                        pic.height = read_u32_be(t.value.data());
                    else if (t.tag == TAG_SAMPLE_RATE && t.value.size() >= 8) {
                        pic.frame_rate_num = read_u32_be(t.value.data());
                        pic.frame_rate_den = read_u32_be(t.value.data() + 4);
                    }
                    else if (t.tag == TAG_CONTAINER_DURATION && t.value.size() >= 8)
                        pic.frame_count = read_u64_be(t.value.data());
                    else if (t.tag == TAG_COMPONENT_DEPTH && t.value.size() >= 4)
                        pic.bit_depth = read_u32_be(t.value.data());
                }
                info.picture = pic;
            }
            continue;
        }

        // Check for sound descriptor
        if (is_sound_descriptor_ul(klv_key) && klv_len <= 65536) {
            std::vector<uint8_t> set_data(klv_len);
            f.read(reinterpret_cast<char*>(set_data.data()), klv_len);
            if (f) {
                SoundDescriptor snd;
                snd.type = EssenceType::pcm_audio;

                auto tags = parse_local_set(set_data.data(), set_data.size());
                for (const auto& t : tags) {
                    if (t.tag == TAG_AUDIO_SAMPLING_RATE && t.value.size() >= 8)
                        snd.sample_rate = read_u32_be(t.value.data());
                    else if (t.tag == TAG_CHANNEL_COUNT && t.value.size() >= 4)
                        snd.channels = read_u32_be(t.value.data());
                    else if (t.tag == TAG_QUANTIZATION_BITS && t.value.size() >= 4)
                        snd.bit_depth = read_u32_be(t.value.data());
                    else if (t.tag == TAG_CONTAINER_DURATION && t.value.size() >= 8)
                        snd.duration = read_u64_be(t.value.data());
                }
                info.sound = snd;
            }
            continue;
        }

        // Skip this KLV value
        if (klv_len > MAX_SCAN) break;
        f.seekg(value_pos + std::streamoff(klv_len));
    }

    return info;
}

std::string_view essence_type_str(EssenceType t) {
    switch (t) {
        case EssenceType::jpeg2000: return "JPEG 2000";
        case EssenceType::mpeg2: return "MPEG-2";
        case EssenceType::pcm_audio: return "PCM Audio";
        case EssenceType::timed_text: return "Timed Text";
        default: return "Unknown";
    }
}

} // namespace dcpdoctor
