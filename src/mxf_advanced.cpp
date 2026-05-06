#include "dcpdoctor/mxf_advanced.h"
#include <AS_DCP.h>
#include <KM_fileio.h>
#include <fstream>
#include <cstring>

namespace dcpdoctor {
namespace fs = std::filesystem;

// MXF Partition Pack key prefix (SMPTE 377-1)
static const uint8_t partition_pack_key[] = {
    0x06, 0x0e, 0x2b, 0x34, 0x02, 0x05, 0x01, 0x01,
    0x0d, 0x01, 0x02, 0x01, 0x01, 0x00, 0x00, 0x00
};

MxfPartitionInfo validate_mxf_partitions(const fs::path& mxf_path) {
    MxfPartitionInfo info;

    std::ifstream f(mxf_path, std::ios::binary);
    if (!f) {
        info.error = "Cannot open MXF file";
        return info;
    }

    // Read first 64 bytes to check header partition
    uint8_t header[16];
    f.read(reinterpret_cast<char*>(header), 16);
    if (!f || f.gcount() < 16) {
        info.error = "File too small for MXF";
        return info;
    }

    // Check for partition pack key (first 13 bytes match)
    bool is_partition = (std::memcmp(header, partition_pack_key, 13) == 0);
    if (is_partition) {
        info.has_header_partition = true;
        // Byte 13 indicates partition type:
        // 0x02 = Header Partition (Open Incomplete)
        // 0x03 = Header Partition (Closed Incomplete)
        // 0x04 = Header Partition (Open Complete)
        // 0x04 = Header Partition (Closed Complete)
        uint8_t partition_status = header[14];
        info.closed_complete = (partition_status >= 0x04);
    }

    // Seek to end to find footer
    f.seekg(0, std::ios::end);
    auto file_size = f.tellg();

    // Scan last portion for footer partition
    if (file_size > 1024) {
        f.seekg(-std::min(int64_t(65536), int64_t(file_size)), std::ios::end);
        std::vector<uint8_t> tail(std::min(size_t(65536), size_t(file_size)));
        f.read(reinterpret_cast<char*>(tail.data()), tail.size());

        for (size_t i = 0; i + 16 <= tail.size(); ++i) {
            if (std::memcmp(tail.data() + i, partition_pack_key, 13) == 0) {
                uint8_t type = tail[i + 13];
                if (type == 0x04) {  // Footer partition
                    info.has_footer_partition = true;
                    info.footer_offset = file_size - int64_t(tail.size()) + int64_t(i);
                } else if (type == 0x03) {  // Body partition
                    info.has_body_partition = true;
                    info.body_partition_count++;
                }
            }
        }
    }

    info.valid = true;
    info.header_size = file_size;
    return info;
}

std::vector<Note> check_mxf_partitions(const MxfPartitionInfo& info,
                                        const fs::path& mxf_path) {
    std::vector<Note> notes;
    if (!info.valid) return notes;

    if (!info.has_header_partition) {
        notes.push_back(Note{Severity::error, Code::mxf_invalid_structure,
                        "MXF missing header partition", mxf_path});
    }

    if (!info.has_footer_partition) {
        notes.push_back(Note{Severity::warning, Code::mxf_invalid_structure,
                        "MXF missing footer partition (may cause playback issues on some servers)",
                        mxf_path});
    }

    if (!info.closed_complete) {
        notes.push_back(Note{Severity::info, Code::mxf_invalid_structure,
                        "MXF header partition not Closed & Complete",
                        mxf_path});
    }

    return notes;
}

DolbyVisionInfo detect_dolby_vision(const fs::path& mxf_path) {
    DolbyVisionInfo info;

    // Dolby Vision uses specific ULs in MXF metadata
    // Look for DV RPU (Reference Processing Unit) essence
    Kumu::FileReaderFactory factory;
    ASDCP::JP2K::MXFReader reader(factory);

    auto result = reader.OpenRead(mxf_path.string());
    if (ASDCP_FAILURE(result)) return info;

    ASDCP::WriterInfo winfo;
    reader.FillWriterInfo(winfo);

    // Check for Dolby Vision essence container label
    // DV uses a specific essence coding UL: 06.0e.2b.34.04.01.01.0d.04.01.02.02.03.01.XX.XX
    // The presence of supplemental data with DV OID indicates Dolby Vision
    // For now, detect based on product name in writer info
    std::string product(reinterpret_cast<const char*>(winfo.ProductName.c_str()));
    if (product.find("Dolby") != std::string::npos &&
        product.find("Vision") != std::string::npos) {
        info.detected = true;
        info.version = product;
    }

    return info;
}

DtsxInfo detect_dtsx(const fs::path& mxf_path) {
    DtsxInfo info;

    // DTS:X uses MXF DC Data essence (like Atmos)
    // Distinguished by the DTS:X specific essence container UL
    // UL: 06.0e.2b.34.04.01.01.05.0e.09.06.XX.XX.XX.XX.XX (DTS namespace)
    Kumu::FileReaderFactory factory;
    ASDCP::PCM::MXFReader reader(factory);

    auto result = reader.OpenRead(mxf_path.string());
    if (ASDCP_FAILURE(result)) return info;

    ASDCP::WriterInfo winfo;
    reader.FillWriterInfo(winfo);

    // Check essence coding label for DTS:X indicators
    // DTS:X metadata is carried as supplemental data in audio MXF
    ASDCP::PCM::AudioDescriptor adesc;
    reader.FillAudioDescriptor(adesc);

    // DTS:X typically uses higher channel counts for object audio
    if (adesc.ChannelCount > 8) {
        // Check writer info for DTS product markers
        std::string product(reinterpret_cast<const char*>(winfo.ProductName.c_str()));
        if (product.find("DTS") != std::string::npos) {
            info.detected = true;
            info.immersive = true;
            info.channel_count = adesc.ChannelCount;
            info.version = product;
        }
    }

    return info;
}

std::vector<Note> check_dtsx_compliance(const DtsxInfo& info, const fs::path& mxf_path) {
    std::vector<Note> notes;
    if (!info.detected) return notes;

    notes.push_back(Note{Severity::info, Code::sound_invalid_channel_count,
                    "DTS:X Immersive Audio detected (" +
                    std::to_string(info.channel_count) + " channels)",
                    mxf_path});

    if (info.channel_count < 12) {
        notes.push_back(Note{Severity::warning, Code::sound_invalid_channel_count,
                        "DTS:X typically requires 12+ channels for full immersive experience",
                        mxf_path});
    }

    return notes;
}

} // namespace dcpdoctor
