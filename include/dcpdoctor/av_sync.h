#pragma once

#include <filesystem>
#include <string>
#include <cstdint>

namespace dcpdoctor
{

struct AvSyncOptions
{
  std::filesystem::path video_file; // video MXF or image sequence dir
  std::filesystem::path audio_file; // audio WAV/MXF
  uint32_t fps_num = 24;
  uint32_t fps_den = 1;
  uint32_t sample_rate = 48000;
};

struct AvSyncResult
{
  int32_t drift_samples = 0; // positive = audio ahead, negative = audio behind
  double drift_ms = 0; // drift in milliseconds
  double drift_frames = 0; // drift in video frames
  bool in_sync = false; // within ±1 frame tolerance
  std::string recommendation; // "trim audio head by X samples" etc.
  bool success = false;
  std::string error;
};

struct AvSyncFixOptions
{
  std::filesystem::path audio_file;
  std::filesystem::path output_file;
  int32_t trim_samples = 0; // positive = trim from start, negative = pad start
  uint32_t sample_rate = 48000;
  uint16_t bit_depth = 24;
};

struct AvSyncFixResult
{
  std::filesystem::path output_file;
  int32_t samples_adjusted = 0;
  bool success = false;
  std::string error;
};

// Detect A/V sync drift between video and audio
AvSyncResult detect_av_sync(const AvSyncOptions& opts);

// Fix A/V sync by trimming or padding audio
AvSyncFixResult fix_av_sync(const AvSyncFixOptions& opts);

} // namespace dcpdoctor
