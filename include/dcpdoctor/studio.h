#pragma once

#include "dcpdoctor/dcpdoctor.h"
#include <filesystem>
#include <string>
#include <vector>
#include <cstdint>

namespace dcpdoctor
{

// ════════════════════════════════════════════════════════════════════════════════
// 1. Audio Loudness (EBU R128 / SMPTE RP 2071)
// ════════════════════════════════════════════════════════════════════════════════

struct LoudnessResult
{
  bool valid = false;
  double integrated_lufs = -70.0; // EBU R128 integrated loudness
  double loudness_range_lu = 0.0; // LRA (Loudness Range)
  double true_peak_dbtp = -70.0; // True peak in dBTP
  double momentary_max_lufs = -70.0; // Max momentary loudness
  uint32_t channels = 0;
  uint32_t sample_rate = 0;
  std::string error;
};

/// Measure EBU R128 loudness from a PCM MXF file
LoudnessResult measure_loudness(const std::filesystem::path& mxf_path, uint32_t max_frames = 0);

/// Check loudness against cinema standards (Leq(m) -85 dBFS / -31 LUFS dialogue)
std::vector<Note> check_loudness_compliance(const LoudnessResult& result,
                                            const std::filesystem::path& mxf_path);

// ════════════════════════════════════════════════════════════════════════════════
// 2. Audio Channel Configuration
// ════════════════════════════════════════════════════════════════════════════════

enum class ChannelLayout
{
  mono, // 1.0
  stereo, // 2.0
  surround_51, // 5.1
  surround_71, // 7.1
  atmos_iab, // Atmos / IAB
  dtsx, // DTS:X
  unknown
};

struct ChannelConfig
{
  bool valid = false;
  uint32_t channel_count = 0;
  ChannelLayout layout = ChannelLayout::unknown;
  std::string mca_tag_symbol; // MCA TagSymbol (SMPTE ST 377-4)
  std::vector<std::string> labels; // Per-channel labels (L, R, C, LFE, Ls, Rs, etc.)
  bool has_mca_labels = false;
  std::string error;
};

/// Detect audio channel configuration from MXF sound essence
ChannelConfig detect_channel_config(const std::filesystem::path& mxf_path);

/// Check channel configuration against DCI/SMPTE requirements
std::vector<Note> check_channel_compliance(const ChannelConfig& config,
                                           const std::filesystem::path& mxf_path);

// ════════════════════════════════════════════════════════════════════════════════
// 3. Color Space / Gamut Validation
// ════════════════════════════════════════════════════════════════════════════════

enum class ColorSpace
{
  xyz, // CIE XYZ (DCI standard)
  dci_p3, // DCI-P3
  rec709, // ITU-R BT.709
  rec2020, // ITU-R BT.2020
  unknown
};

struct ColorInfo
{
  bool valid = false;
  ColorSpace detected_space = ColorSpace::unknown;
  uint8_t bit_depth = 0;
  bool xyz_to_p3_checked = false;
  double max_code_value = 0.0;
  double min_code_value = 0.0;
  bool out_of_gamut_detected = false;
  uint32_t oog_pixel_count = 0; // out-of-gamut pixel count in sampled frames
  std::string error;
};

/// Detect color space from J2K codestream parameters
ColorInfo detect_color_space(const std::filesystem::path& mxf_path);

/// Check color gamut compliance (DCI-P3 within XYZ container)
std::vector<Note> check_color_compliance(const ColorInfo& info,
                                         const std::filesystem::path& mxf_path);

// ════════════════════════════════════════════════════════════════════════════════
// 4. Stereoscopic 3D Validation
// ════════════════════════════════════════════════════════════════════════════════

struct StereoInfo
{
  bool valid = false;
  bool is_stereoscopic = false;
  bool left_eye_detected = false;
  bool right_eye_detected = false;
  uint32_t left_frame_count = 0;
  uint32_t right_frame_count = 0;
  bool frame_count_match = false;
  double left_bitrate_mbps = 0.0;
  double right_bitrate_mbps = 0.0;
  std::string error;
};

/// Detect and validate stereoscopic 3D content
StereoInfo detect_stereoscopic(const std::filesystem::path& dcp_dir);

/// Check stereoscopic compliance
std::vector<Note> check_stereo_compliance(const StereoInfo& info,
                                          const std::filesystem::path& dcp_dir);

// ════════════════════════════════════════════════════════════════════════════════
// 5. Cross-Reel Continuity
// ════════════════════════════════════════════════════════════════════════════════

struct ReelContinuity
{
  bool valid = false;
  uint32_t reel_count = 0;
  bool timing_continuous = false;
  std::vector<int64_t> gap_frames; // Gap between reels (negative = overlap)
  std::vector<uint64_t> reel_durations; // Duration per reel in frames
  bool audio_video_sync = true;
  std::string error;
};

/// Check cross-reel timing continuity in a multi-reel DCP
ReelContinuity analyze_reel_continuity(const std::filesystem::path& dcp_dir);

/// Generate notes for reel continuity issues
std::vector<Note> check_continuity_compliance(const ReelContinuity& info,
                                              const std::filesystem::path& dcp_dir);

// ════════════════════════════════════════════════════════════════════════════════
// 6. Supplemental Package (VF) Validation
// ════════════════════════════════════════════════════════════════════════════════

struct SupplementalInfo
{
  bool valid = false;
  bool is_supplemental = false; // Version File (VF)?
  std::string original_version_id; // UUID of referenced OV
  uint32_t referenced_assets = 0;
  uint32_t missing_references = 0;
  std::vector<std::string> missing_asset_ids;
  bool ov_path_found = false;
  std::filesystem::path ov_path;
  std::string error;
};

/// Detect and validate supplemental (VF) package references
SupplementalInfo validate_supplemental(const std::filesystem::path& dcp_dir,
                                       const std::filesystem::path& ov_dir = "");

/// Generate notes for supplemental package issues
std::vector<Note> check_supplemental_compliance(const SupplementalInfo& info,
                                                const std::filesystem::path& dcp_dir);

// ════════════════════════════════════════════════════════════════════════════════
// 7. Encryption Consistency
// ════════════════════════════════════════════════════════════════════════════════

struct EncryptionInfo
{
  bool valid = false;
  bool has_encrypted_assets = false;
  bool has_unencrypted_assets = false;
  bool mixed_encryption = false; // Some encrypted, some not
  uint32_t encrypted_count = 0;
  uint32_t unencrypted_count = 0;
  std::vector<std::string> encrypted_asset_ids;
  std::vector<std::string> unencrypted_asset_ids;
  bool kdm_required = false;
  std::string error;
};

/// Check encryption consistency across all MXF assets
EncryptionInfo check_encryption(const std::filesystem::path& dcp_dir);

/// Generate notes for encryption issues
std::vector<Note> check_encryption_compliance(const EncryptionInfo& info,
                                              const std::filesystem::path& dcp_dir);

// ════════════════════════════════════════════════════════════════════════════════
// 8. Reel Duration Compliance
// ════════════════════════════════════════════════════════════════════════════════

struct ReelDurationInfo
{
  bool valid = false;
  uint32_t reel_count = 0;
  uint64_t total_duration_frames = 0;
  double total_duration_seconds = 0.0;
  double frame_rate = 0.0;
  uint64_t longest_reel_frames = 0;
  double longest_reel_seconds = 0.0;
  uint32_t longest_reel_index = 0;
  bool exceeds_max_reel_length = false; // >40 min SMPTE recommendation
  std::string error;
};

/// Analyze reel durations
ReelDurationInfo analyze_reel_durations(const std::filesystem::path& dcp_dir);

/// Check reel duration against SMPTE recommendations
std::vector<Note> check_duration_compliance(const ReelDurationInfo& info,
                                            const std::filesystem::path& dcp_dir);

// ════════════════════════════════════════════════════════════════════════════════
// 9. DCI Content Type Detection
// ════════════════════════════════════════════════════════════════════════════════

enum class ContentType
{
  feature, // Full-length feature film
  trailer, // Trailer / advertisement
  advertisement, // Pre-show advertisement
  test, // Test content
  short_film, // Short film
  transition, // Transition content
  unknown
};

struct ContentTypeInfo
{
  bool valid = false;
  ContentType detected_type = ContentType::unknown;
  double duration_minutes = 0.0;
  std::string content_kind; // From CPL ContentKind element
  std::string rating; // MPAA / BBFC rating if present
  bool has_content_kind = false;
  std::string error;
};

/// Detect content type from CPL metadata and duration
ContentTypeInfo detect_content_type(const std::filesystem::path& dcp_dir);

/// Check content type metadata compliance
std::vector<Note> check_content_type_compliance(const ContentTypeInfo& info,
                                                const std::filesystem::path& dcp_dir);

// ════════════════════════════════════════════════════════════════════════════════
// 10. Multi-CPL Validation
// ════════════════════════════════════════════════════════════════════════════════

struct MultiCplInfo
{
  bool valid = false;
  uint32_t cpl_count = 0;
  std::vector<std::string> cpl_ids;
  std::vector<std::string> cpl_titles;
  bool all_assets_referenced = false; // All PKL assets used by at least one CPL
  std::vector<std::string> orphan_assets; // Assets in PKL but not in any CPL
  bool consistent_frame_rate = true;
  bool consistent_resolution = true;
  std::string error;
};

/// Validate multi-CPL DCP package
MultiCplInfo validate_multi_cpl(const std::filesystem::path& dcp_dir);

/// Check multi-CPL compliance
std::vector<Note> check_multi_cpl_compliance(const MultiCplInfo& info,
                                             const std::filesystem::path& dcp_dir);

// ════════════════════════════════════════════════════════════════════════════════
// 11. Subtitle Font Validation
// ════════════════════════════════════════════════════════════════════════════════

struct SubtitleFontInfo
{
  bool valid = false;
  uint32_t font_count = 0;
  std::vector<std::string> font_ids;
  std::vector<std::string> missing_fonts; // Referenced but not embedded
  bool all_fonts_embedded = false;
  uint32_t total_subtitle_count = 0;
  double min_display_seconds = 999.0; // Shortest subtitle duration
  double max_display_seconds = 0.0;
  bool timing_valid = true; // No overlaps, min duration met
  std::string error;
};

/// Validate subtitle fonts and timing
SubtitleFontInfo validate_subtitle_fonts(const std::filesystem::path& dcp_dir);

/// Check subtitle font/timing compliance
std::vector<Note> check_subtitle_font_compliance(const SubtitleFontInfo& info,
                                                 const std::filesystem::path& dcp_dir);

// ════════════════════════════════════════════════════════════════════════════════
// 12. Resolution & Aspect Ratio Validation
// ════════════════════════════════════════════════════════════════════════════════

enum class DciContainer
{
  flat_2k, // 1998x1080
  scope_2k, // 2048x858
  full_2k, // 2048x1080
  flat_4k, // 3996x2160
  scope_4k, // 4096x1716
  full_4k, // 4096x2160
  non_standard
};

struct ResolutionInfo
{
  bool valid = false;
  uint32_t width = 0;
  uint32_t height = 0;
  DciContainer container = DciContainer::non_standard;
  double aspect_ratio = 0.0;
  bool is_2k = false;
  bool is_4k = false;
  bool matches_dci_container = false;
  std::string error;
};

/// Detect and validate picture resolution
ResolutionInfo detect_resolution(const std::filesystem::path& mxf_path);

/// Check resolution against DCI container requirements
std::vector<Note> check_resolution_compliance(const ResolutionInfo& info,
                                              const std::filesystem::path& mxf_path);

// ════════════════════════════════════════════════════════════════════════════════
// Convenience: Run all studio checks at once
// ════════════════════════════════════════════════════════════════════════════════

/// Run all studio-level validation checks on a DCP directory
std::vector<Note> run_studio_checks(const std::filesystem::path& dcp_dir, bool deep = false);

} // namespace dcpdoctor
