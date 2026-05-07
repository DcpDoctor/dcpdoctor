#include "dcpdoctor/j2k.h"
#include <cstring>
#include <fstream>

namespace dcpdoctor
{
namespace
{

  // JPEG 2000 markers
  constexpr uint16_t SOC = 0xFF4F; // Start of codestream
  constexpr uint16_t SIZ = 0xFF51; // Image and tile size
  constexpr uint16_t COD = 0xFF52; // Coding style default
  constexpr uint16_t COC = 0xFF53; // Coding style component
  constexpr uint16_t SOT = 0xFF90; // Start of tile-part

  uint16_t read_u16(const uint8_t* p)
  {
    return (uint16_t(p[0]) << 8) | p[1];
  }

  uint32_t read_u32(const uint8_t* p)
  {
    return (uint32_t(p[0]) << 24) | (uint32_t(p[1]) << 16) | (uint32_t(p[2]) << 8) | p[3];
  }

} // namespace

J2kInfo parse_j2k_header(const uint8_t* data, size_t len)
{
  J2kInfo info;

  if(len < 4)
  {
    info.error = "Data too small for J2K codestream";
    return info;
  }

  // Check SOC marker
  if(read_u16(data) != SOC)
  {
    info.error = "Missing SOC marker";
    return info;
  }

  size_t pos = 2;

  // Parse markers until SOT or end of header data
  while(pos + 4 <= len)
  {
    uint16_t marker = read_u16(data + pos);
    pos += 2;

    if(marker == SOT)
      break; // Reached tile data

    // Read marker segment length
    if(pos + 2 > len)
      break;
    uint16_t seg_len = read_u16(data + pos);
    if(seg_len < 2 || pos + seg_len > len)
      break;

    const uint8_t* seg = data + pos + 2; // segment data (after length field)
    size_t seg_data_len = seg_len - 2;

    if(marker == SIZ && seg_data_len >= 36)
    {
      // SIZ marker segment (ISO 15444-1 Table A.9)
      info.rsiz = read_u16(seg);
      info.width = read_u32(seg + 2); // Xsiz
      info.height = read_u32(seg + 6); // Ysiz
      // XOsiz at +10, YOsiz at +14
      // XTsiz at +18, YTsiz at +22
      // XTOsiz at +26, YTOsiz at +30
      info.num_components = read_u16(seg + 34);

      // Component bit depth (first component)
      if(seg_data_len >= 38)
      {
        uint8_t ssiz = seg[36];
        info.bit_depth = (ssiz & 0x7F) + 1;
      }

      info.valid = true;
    }
    else if(marker == COD && seg_data_len >= 9)
    {
      // COD marker segment (ISO 15444-1 Table A.12)
      // Scod at +0, SGcod at +1..+4, SPcod at +5+
      // SPcod: NumDecompLevels at +0
      info.num_resolutions = seg[5] + 1;
      // Transformation: 0 = 9-7 irreversible, 1 = 5-3 reversible
      if(seg_data_len >= 12)
      {
        info.irreversible = (seg[11] == 0);
      }
    }

    pos += seg_len;
  }

  if(!info.valid && info.error.empty())
  {
    info.error = "SIZ marker not found";
  }

  return info;
}

std::vector<Note> check_j2k_bitrate(const std::filesystem::path& mxf_path, uint64_t frame_count,
                                    uint32_t frame_rate_num, uint32_t frame_rate_den,
                                    uint32_t width, uint32_t height)
{
  std::vector<Note> notes;
  namespace fs = std::filesystem;

  if(frame_count == 0 || frame_rate_num == 0 || frame_rate_den == 0)
    return notes;

  // Get file size
  std::error_code ec;
  auto file_size = fs::file_size(mxf_path, ec);
  if(ec)
    return notes;

  // Estimate average bitrate:
  // Total essence bytes ≈ file_size (overestimate since includes MXF overhead)
  // Duration in seconds = frame_count * frame_rate_den / frame_rate_num
  // Bitrate = file_size * 8 / duration_seconds

  double duration_sec = double(frame_count) * double(frame_rate_den) / double(frame_rate_num);
  if(duration_sec <= 0)
    return notes;

  double avg_bitrate_mbps = (double(file_size) * 8.0) / (duration_sec * 1000000.0);

  // DCI bitrate limits:
  // 2K (width <= 2048): max 250 Mbps
  // 4K (width > 2048): max 500 Mbps
  double max_bitrate = (width > 2048) ? 500.0 : 250.0;

  if(avg_bitrate_mbps > max_bitrate)
  {
    notes.push_back({Severity::error, Code::picture_invalid_frame_rate,
                     "Average bitrate " + std::to_string(int(avg_bitrate_mbps)) +
                         " Mbps exceeds DCI limit of " + std::to_string(int(max_bitrate)) + " Mbps",
                     mxf_path});
  }
  else if(avg_bitrate_mbps > max_bitrate * 0.9)
  {
    notes.push_back({Severity::warning, Code::picture_invalid_frame_rate,
                     "Average bitrate " + std::to_string(int(avg_bitrate_mbps)) +
                         " Mbps is near DCI limit of " + std::to_string(int(max_bitrate)) + " Mbps",
                     mxf_path});
  }

  return notes;
}

} // namespace dcpdoctor
