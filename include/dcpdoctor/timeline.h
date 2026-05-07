#pragma once

#include "dcpdoctor/dcpdoctor.h"
#include <filesystem>
#include <ostream>
#include <string>
#include <vector>

namespace dcpdoctor
{

/// Timeline information for one reel
struct TimelineReel
{
  std::string id;
  uint64_t picture_entry = 0;
  uint64_t picture_duration = 0;
  uint64_t sound_entry = 0;
  uint64_t sound_duration = 0;
  uint64_t subtitle_entry = 0;
  uint64_t subtitle_duration = 0;
  bool has_picture = false;
  bool has_sound = false;
  bool has_subtitle = false;
};

/// Generate SVG timeline visualization of CPL structure
void write_timeline_svg(std::ostream& out, const std::vector<TimelineReel>& reels,
                        const std::string& title, double frame_rate);

/// Extract timeline reels from a CPL file
std::vector<TimelineReel> extract_timeline(const std::filesystem::path& cpl_path);

/// Detect audio sync drift (picture vs sound duration mismatches)
std::vector<Note> check_audio_sync(const std::vector<TimelineReel>& reels,
                                   const std::filesystem::path& cpl_path);

} // namespace dcpdoctor
