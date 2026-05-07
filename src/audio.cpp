#include "dcpdoctor/audio.h"
#include <AS_DCP.h>
#include <KM_fileio.h>
#include <cmath>
#include <algorithm>

namespace dcpdoctor
{

AudioLevelStats analyze_audio_levels(const std::filesystem::path& mxf_path, uint32_t max_frames)
{
  AudioLevelStats stats;

  Kumu::FileReaderFactory defaultFactory;
  ASDCP::PCM::MXFReader reader(defaultFactory);

  auto result = reader.OpenRead(mxf_path.string());
  if(ASDCP_FAILURE(result))
  {
    stats.error = "Failed to open PCM MXF";
    return stats;
  }

  ASDCP::PCM::AudioDescriptor adesc;
  result = reader.FillAudioDescriptor(adesc);
  if(ASDCP_FAILURE(result))
  {
    stats.error = "Failed to read audio descriptor";
    return stats;
  }

  stats.channels = adesc.ChannelCount;
  stats.sample_rate = adesc.AudioSamplingRate.Numerator;
  stats.bit_depth = adesc.QuantizationBits;
  stats.frame_count = adesc.ContainerDuration;

  if(stats.channels == 0 || stats.frame_count == 0)
  {
    stats.error = "Invalid audio parameters";
    return stats;
  }

  uint32_t frames_to_read = stats.frame_count;
  if(max_frames > 0 && max_frames < frames_to_read)
    frames_to_read = max_frames;

  // Initialize per-channel accumulators
  stats.peak_dbfs.resize(stats.channels, -200.0);
  stats.rms_dbfs.resize(stats.channels, -200.0);
  std::vector<double> sum_sq(stats.channels, 0.0);
  std::vector<double> peak(stats.channels, 0.0);
  uint64_t total_samples = 0;

  uint32_t buf_size = ASDCP::PCM::CalcFrameBufferSize(adesc);
  ASDCP::PCM::FrameBuffer frame_buf(buf_size);

  uint32_t bytes_per_sample = stats.bit_depth / 8;
  uint32_t samples_per_frame = ASDCP::PCM::CalcSamplesPerFrame(adesc);
  double max_value = double((1ULL << (stats.bit_depth - 1)) - 1);

  for(uint32_t f = 0; f < frames_to_read; ++f)
  {
    result = reader.ReadFrame(f, frame_buf);
    if(ASDCP_FAILURE(result))
      break;

    const uint8_t* data = frame_buf.RoData();
    uint32_t data_size = frame_buf.Size();
    uint32_t sample_size = bytes_per_sample * stats.channels;

    for(uint32_t s = 0; s < samples_per_frame && (s * sample_size + sample_size) <= data_size; ++s)
    {
      for(uint32_t ch = 0; ch < stats.channels; ++ch)
      {
        // Read sample (little-endian signed integer, 24-bit typical)
        const uint8_t* sp = data + s * sample_size + ch * bytes_per_sample;
        int32_t sample = 0;

        if(bytes_per_sample == 3)
        {
          sample = int32_t(sp[0]) | (int32_t(sp[1]) << 8) | (int32_t(sp[2]) << 16);
          if(sample & 0x800000)
            sample |= 0xFF000000; // sign extend
        }
        else if(bytes_per_sample == 2)
        {
          sample = int16_t(sp[0] | (sp[1] << 8));
        }
        else if(bytes_per_sample == 4)
        {
          sample = int32_t(sp[0]) | (int32_t(sp[1]) << 8) | (int32_t(sp[2]) << 16) |
                   (int32_t(sp[3]) << 24);
        }

        double normalized = double(sample) / max_value;
        double abs_val = std::fabs(normalized);

        if(abs_val > peak[ch])
          peak[ch] = abs_val;
        sum_sq[ch] += normalized * normalized;
      }
      ++total_samples;
    }
  }

  if(total_samples == 0)
  {
    stats.error = "No audio samples read";
    return stats;
  }

  // Convert to dBFS
  for(uint32_t ch = 0; ch < stats.channels; ++ch)
  {
    stats.peak_dbfs[ch] = (peak[ch] > 0) ? 20.0 * std::log10(peak[ch]) : -200.0;
    double rms = std::sqrt(sum_sq[ch] / double(total_samples));
    stats.rms_dbfs[ch] = (rms > 0) ? 20.0 * std::log10(rms) : -200.0;

    if(stats.peak_dbfs[ch] > stats.overall_peak_dbfs)
      stats.overall_peak_dbfs = stats.peak_dbfs[ch];
    if(stats.rms_dbfs[ch] > stats.overall_rms_dbfs)
      stats.overall_rms_dbfs = stats.rms_dbfs[ch];
  }

  stats.valid = true;
  return stats;
}

std::vector<Note> check_audio_levels(const AudioLevelStats& stats,
                                     const std::filesystem::path& mxf_path)
{
  std::vector<Note> notes;

  if(!stats.valid)
    return notes;

  // Check for digital clipping (peak at 0 dBFS)
  if(stats.overall_peak_dbfs >= -0.1)
  {
    notes.push_back({Severity::warning, Code::sound_invalid_sample_rate,
                     "Audio may be clipping: peak level is " +
                         std::to_string(stats.overall_peak_dbfs).substr(0, 5) + " dBFS",
                     mxf_path});
  }

  // Check for very quiet audio (might indicate wrong gain)
  if(stats.overall_rms_dbfs < -40.0 && stats.overall_peak_dbfs < -20.0)
  {
    notes.push_back({Severity::info, Code::sound_invalid_sample_rate,
                     "Audio levels are very low: RMS " +
                         std::to_string(int(stats.overall_rms_dbfs)) + " dBFS, peak " +
                         std::to_string(int(stats.overall_peak_dbfs)) + " dBFS",
                     mxf_path});
  }

  // Check for silence (all channels below -80 dBFS)
  bool all_silent = true;
  for(double p : stats.peak_dbfs)
  {
    if(p > -80.0)
    {
      all_silent = false;
      break;
    }
  }
  if(all_silent)
  {
    notes.push_back({Severity::warning, Code::sound_invalid_sample_rate,
                     "Audio appears to be silent (all channels below -80 dBFS)", mxf_path});
  }

  return notes;
}

} // namespace dcpdoctor
