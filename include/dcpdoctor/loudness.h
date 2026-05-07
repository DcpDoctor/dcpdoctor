#pragma once

#include <filesystem>
#include <string>
#include <cstdint>

namespace dcpdoctor
{

// EBU R128 loudness measurement results
struct ImfLoudnessResult
{
  double integrated_lufs = 0; // Integrated loudness (LUFS)
  double loudness_range_lu = 0; // Loudness range (LU)
  double true_peak_dbtp = 0; // True peak (dBTP)
  double momentary_max_lufs = 0; // Max momentary loudness
  bool compliant_r128 = false; // -23 LUFS ±1
  bool compliant_atsc = false; // -24 LKFS ±2
  bool success = false;
  std::string error;
};

// Measure loudness of a WAV file using ffmpeg loudnorm filter
ImfLoudnessResult measure_imf_loudness(const std::filesystem::path& audio_file);

// Normalize audio to target loudness
struct NormalizeOptions
{
  std::filesystem::path input_file;
  std::filesystem::path output_file;
  double target_lufs = -23.0; // EBU R128 default
  double true_peak_limit = -1.0; // dBTP limit
};

struct NormalizeResult
{
  std::filesystem::path output_file;
  ImfLoudnessResult measured;
  bool success = false;
  std::string error;
};

NormalizeResult normalize_loudness(const NormalizeOptions& opts);

} // namespace dcpdoctor
