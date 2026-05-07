#include <spdlog/spdlog.h>
#include <array>
#include <cstdio>
#include <regex>

#include "dcpdoctor/loudness.h"
#include "dcpdoctor/platform.h"

namespace dcpdoctor
{

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

ImfLoudnessResult measure_imf_loudness(const std::filesystem::path& audio_file)
{
  ImfLoudnessResult result;

  if(!std::filesystem::exists(audio_file))
  {
    result.error = "Audio file not found";
    return result;
  }

  // Use ffmpeg ebur128 filter for measurement
  std::string cmd = "ffmpeg -i " + audio_file.string() + " -af ebur128=peak=true -f null - 2>&1";
  auto output = exec_cmd(cmd);

  if(output.empty())
  {
    result.error = "ffmpeg loudness measurement failed";
    return result;
  }

  // Parse ffmpeg ebur128 output
  std::regex integrated_re(R"(I:\s*([-\d.]+)\s*LUFS)");
  std::regex range_re(R"(LRA:\s*([-\d.]+)\s*LU)");
  std::regex peak_re(R"(Peak:\s*([-\d.]+)\s*dBFS)");

  std::smatch match;
  if(std::regex_search(output, match, integrated_re))
    result.integrated_lufs = std::stod(match[1].str());
  if(std::regex_search(output, match, range_re))
    result.loudness_range_lu = std::stod(match[1].str());
  if(std::regex_search(output, match, peak_re))
    result.true_peak_dbtp = std::stod(match[1].str());

  // Check compliance
  result.compliant_r128 = (result.integrated_lufs >= -24.0 && result.integrated_lufs <= -22.0);
  result.compliant_atsc = (result.integrated_lufs >= -26.0 && result.integrated_lufs <= -22.0);

  result.success = true;
  spdlog::info("Loudness: {:.1f} LUFS, LRA: {:.1f} LU, Peak: {:.1f} dBTP", result.integrated_lufs,
               result.loudness_range_lu, result.true_peak_dbtp);
  return result;
}

NormalizeResult normalize_loudness(const NormalizeOptions& opts)
{
  NormalizeResult result;

  // Two-pass loudnorm
  std::string cmd = "ffmpeg -y -i " + opts.input_file.string() +
                    " -af loudnorm=I=" + std::to_string(opts.target_lufs) +
                    ":TP=" + std::to_string(opts.true_peak_limit) + ":LRA=11 " +
                    opts.output_file.string() + " 2>&1";

  int ret = system(cmd.c_str());
  if(ret != 0)
  {
    result.error = "ffmpeg loudnorm failed with code " + std::to_string(ret);
    return result;
  }

  result.output_file = opts.output_file;
  result.measured = measure_imf_loudness(opts.output_file);
  result.success = true;
  return result;
}

} // namespace dcpdoctor
