#include <spdlog/spdlog.h>
#include <algorithm>
#include <array>
#include <cstdio>
#include <regex>

#include "dcpdoctor/qc.h"
#include "dcpdoctor/platform.h"

namespace dcpdoctor
{

FrameQcResult analyze_frame_qc(const FrameQcOptions& opts)
{
  FrameQcResult result;
  namespace fs = std::filesystem;

  if(!fs::exists(opts.j2k_dir))
  {
    result.error = "J2K directory not found";
    return result;
  }

  double fps = static_cast<double>(opts.fps_num) / opts.fps_den;
  std::vector<fs::path> frames;

  for(const auto& entry : fs::directory_iterator(opts.j2k_dir))
  {
    if(!entry.is_regular_file())
      continue;
    auto ext = entry.path().extension().string();
    if(ext == ".j2c" || ext == ".j2k")
    {
      frames.push_back(entry.path());
    }
  }

  std::sort(frames.begin(), frames.end());

  for(uint32_t i = 0; i < frames.size(); ++i)
  {
    auto size = fs::file_size(frames[i]);
    double bitrate = (size * 8.0 * fps) / 1e6; // Mbps

    FrameQcEntry entry;
    entry.frame_number = i;
    entry.size_bytes = size;
    entry.bitrate_mbps = bitrate;
    entry.over_budget = (bitrate > opts.max_bitrate_mbps);
    entry.under_budget = (bitrate < opts.min_bitrate_mbps);

    if(entry.over_budget)
      ++result.over_budget_count;
    if(entry.under_budget)
      ++result.under_budget_count;

    result.total_bytes += size;
    result.frames.push_back(entry);
  }

  result.total_frames = static_cast<uint32_t>(frames.size());
  if(result.total_frames > 0)
  {
    result.average_bitrate_mbps = (result.total_bytes * 8.0 * fps) / (result.total_frames * 1e6);
    result.peak_bitrate_mbps = std::max_element(result.frames.begin(), result.frames.end(),
                                                [](const auto& a, const auto& b) {
                                                  return a.bitrate_mbps < b.bitrate_mbps;
                                                })
                                   ->bitrate_mbps;
    result.min_bitrate_mbps = std::min_element(result.frames.begin(), result.frames.end(),
                                               [](const auto& a, const auto& b) {
                                                 return a.bitrate_mbps < b.bitrate_mbps;
                                               })
                                  ->bitrate_mbps;
  }

  result.success = true;
  spdlog::info("Frame QC: {} frames, avg {:.1f} Mbps, peak {:.1f} Mbps, {} over-budget",
               result.total_frames, result.average_bitrate_mbps, result.peak_bitrate_mbps,
               result.over_budget_count);
  return result;
}

static std::string exec_cmd(const std::string& cmd)
{
  std::array<char, 4096> buffer;
  std::string result;
  FILE* pipe = DCPDOCTOR_POPEN(cmd.c_str(), "r");
  if(!pipe)
    return {};
  while(fgets(buffer.data(), buffer.size(), pipe))
    result += buffer.data();
  DCPDOCTOR_PCLOSE(pipe);
  return result;
}

QualityMetrics compute_quality(const QualityOptions& opts)
{
  QualityMetrics result;

  std::string filter = "";
  if(opts.compute_vmaf)
  {
    filter = "libvmaf=log_fmt=json:log_path=/dev/stderr";
  }
  if(opts.compute_psnr)
  {
    if(!filter.empty())
      filter += ";";
    filter += "psnr";
  }
  if(opts.compute_ssim)
  {
    if(!filter.empty())
      filter += ";";
    filter += "ssim";
  }

  std::string cmd = "ffmpeg -i " + opts.distorted.string() + " -i " + opts.reference.string() +
                    " -lavfi \"[0:v][1:v]" + filter + "\" -f null - 2>&1";

  auto output = exec_cmd(cmd);
  if(output.empty())
  {
    result.error = "ffmpeg quality analysis failed";
    return result;
  }

  // Parse VMAF score
  std::regex vmaf_re(R"(VMAF score:\s*([\d.]+))");
  std::regex psnr_re(R"(average:([\d.]+))");
  std::regex ssim_re(R"(All:([\d.]+))");

  std::smatch match;
  if(std::regex_search(output, match, vmaf_re))
    result.vmaf_score = std::stod(match[1].str());
  if(std::regex_search(output, match, psnr_re))
    result.psnr_avg = std::stod(match[1].str());
  if(std::regex_search(output, match, ssim_re))
    result.ssim = std::stod(match[1].str());

  result.success = true;
  spdlog::info("Quality: VMAF={:.2f}, PSNR={:.2f}, SSIM={:.4f}", result.vmaf_score, result.psnr_avg,
               result.ssim);
  return result;
}

} // namespace dcpdoctor
