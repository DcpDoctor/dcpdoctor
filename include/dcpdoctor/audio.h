#pragma once

#include "dcpdoctor/dcpdoctor.h"
#include <filesystem>
#include <vector>
#include <cstdint>

namespace dcpdoctor
{

/// Audio level analysis results
struct AudioLevelStats
{
  bool valid = false;
  uint32_t channels = 0;
  uint32_t sample_rate = 0;
  uint32_t bit_depth = 0;
  uint64_t frame_count = 0;
  std::vector<double> peak_dbfs; // per-channel peak in dBFS
  std::vector<double> rms_dbfs; // per-channel RMS in dBFS
  double overall_peak_dbfs = -200.0;
  double overall_rms_dbfs = -200.0;
  std::string error;
};

/// Analyze audio levels from a PCM MXF file using asdcplib
AudioLevelStats analyze_audio_levels(const std::filesystem::path& mxf_path,
                                     uint32_t max_frames = 0);

/// Generate notes for audio level issues
std::vector<Note> check_audio_levels(const AudioLevelStats& stats,
                                     const std::filesystem::path& mxf_path);

} // namespace dcpdoctor
