#pragma once

#include "dcpdoctor/dcpdoctor.h"
#include <filesystem>
#include <string>
#include <vector>

namespace dcpdoctor {

/// Detect encrypted content and KDM requirements
std::vector<Note> check_encryption(const std::filesystem::path& dcp_dir,
                                    const std::vector<std::filesystem::path>& cpl_paths);

/// Check reel continuity (entry points, durations match across reels)
std::vector<Note> check_reel_continuity(const std::filesystem::path& cpl_path);

/// Check 3D stereoscopic structure (matching left/right eyes)
std::vector<Note> check_stereo(const std::filesystem::path& cpl_path);

/// Check for required markers (FFMC, LFMC, FFTC, LFTC, FFOI, LFOI, FFEC, LFEC)
std::vector<Note> check_markers(const std::filesystem::path& cpl_path, bool strict);

/// Cross-reference integrity: all UUIDs in CPL reference assets in PKL/ASSETMAP
std::vector<Note> check_cross_references(const std::filesystem::path& dcp_dir,
                                          const std::vector<std::string>& known_asset_ids,
                                          const std::vector<std::filesystem::path>& cpl_paths);

/// Supplemental DCP: check OPL references
std::vector<Note> check_supplemental(const std::filesystem::path& dcp_dir,
                                      const std::vector<std::filesystem::path>& cpl_paths);

/// Audio channel labeling (SMPTE 429-2 channel assignments)
std::vector<Note> check_audio_channels(const std::filesystem::path& cpl_path);

/// Color space hints (XYZ detection from J2K profile)
std::vector<Note> check_color_space(const std::filesystem::path& cpl_path);

} // namespace dcpdoctor
