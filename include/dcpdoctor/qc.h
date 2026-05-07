#pragma once

#include <filesystem>
#include <string>
#include <vector>
#include <cstdint>

namespace dcpdoctor
{

// Frame-level QC analysis
struct FrameQcEntry
{
  uint32_t frame_number;
  uint64_t size_bytes; // J2K codestream size
  double bitrate_mbps; // Effective bitrate for this frame
  bool over_budget = false; // Exceeds target bitrate
  bool under_budget = false; // Below minimum quality threshold
};

struct FrameQcResult
{
  std::vector<FrameQcEntry> frames;
  double average_bitrate_mbps = 0;
  double peak_bitrate_mbps = 0;
  double min_bitrate_mbps = 0;
  uint32_t over_budget_count = 0;
  uint32_t under_budget_count = 0;
  uint32_t total_frames = 0;
  uint64_t total_bytes = 0;
  bool success = false;
  std::string error;
};

struct FrameQcOptions
{
  std::filesystem::path j2k_dir; // J2K codestream directory
  double target_bitrate_mbps = 250.0; // Target
  double max_bitrate_mbps = 300.0; // Over-budget threshold
  double min_bitrate_mbps = 50.0; // Under-budget threshold
  uint32_t fps_num = 24;
  uint32_t fps_den = 1;
};

// Analyze J2K frame sizes for bitrate compliance
FrameQcResult analyze_frame_qc(const FrameQcOptions& opts);

// VMAF/PSNR quality metrics
struct QualityMetrics
{
  double vmaf_score = 0; // 0-100
  double psnr_y = 0; // Luma PSNR (dB)
  double psnr_avg = 0; // Average PSNR
  double ssim = 0; // 0-1
  bool success = false;
  std::string error;
};

struct QualityOptions
{
  std::filesystem::path reference; // Reference video/frames
  std::filesystem::path distorted; // Encoded/processed video
  bool compute_vmaf = true;
  bool compute_psnr = true;
  bool compute_ssim = true;
};

// Compute quality metrics using ffmpeg libvmaf
QualityMetrics compute_quality(const QualityOptions& opts);

} // namespace dcpdoctor
