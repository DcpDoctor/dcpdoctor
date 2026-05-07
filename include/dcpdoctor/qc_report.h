#pragma once

#include <filesystem>
#include <string>
#include <vector>
#include <cstdint>

namespace dcpdoctor
{

struct DetailedQcOptions
{
  std::filesystem::path imp_dir;
  std::filesystem::path output_file; // .html or .pdf
  std::string title;
  std::string client;
  bool include_thumbnails = true;
  bool include_waveform = true;
  bool include_loudness = true;
  bool include_bitrate_chart = true;
  uint32_t thumbnail_count = 12; // number of thumbnail frames
};

struct DetailedQcResult
{
  std::filesystem::path output_file;
  uint32_t pages = 0;
  bool success = false;
  std::string error;
};

// Generate a QC report (HTML or PDF) for an IMP
DetailedQcResult generate_detailed_qc(const DetailedQcOptions& opts);

} // namespace dcpdoctor
