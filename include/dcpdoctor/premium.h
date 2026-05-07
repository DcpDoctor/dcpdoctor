#pragma once

#include "dcpdoctor/dcpdoctor.h"
#include <filesystem>
#include <string>
#include <vector>

namespace dcpdoctor
{

/// TTML/IMSC subtitle timing entry
struct TtmlTimingEntry
{
  std::string begin;
  std::string end;
  std::string region;
  std::string text_content;
  int line_number = 0;
};

/// TTML document validation info
struct TtmlInfo
{
  bool valid = false;
  std::string profile; // e.g. "imsc1", "imsc1.1", "smpte-tt"
  std::string language;
  int subtitle_count = 0;
  int region_count = 0;
  bool has_timing_errors = false;
  bool has_style_refs = false;
  bool has_font_refs = false;
  std::vector<TtmlTimingEntry> entries;
  std::string error;
};

/// Parse and validate a TTML/IMSC subtitle file
TtmlInfo validate_ttml(const std::filesystem::path& ttml_path);

/// Check IMSC profile compliance
std::vector<Note> check_imsc_compliance(const TtmlInfo& info,
                                        const std::filesystem::path& ttml_path);

/// Dolby Vision metadata detection and validation
struct DolbyVisionMetadata
{
  bool detected = false;
  uint8_t profile = 0; // Profile 5, 8, etc.
  uint8_t level = 0;
  uint8_t bl_present_flag = 0;
  uint8_t el_present_flag = 0;
  uint8_t rpu_present_flag = 0;
  std::string compatibility_id;
  bool is_tunnel = false; // Dual-layer tunneled
  bool is_mef = false; // Multi-resolution Enhancement Framework
  uint32_t rpu_count = 0;
  std::string error;
};

/// Deep Dolby Vision metadata parsing from MXF
DolbyVisionMetadata parse_dolby_vision(const std::filesystem::path& mxf_path);

/// Validate DV metadata against DCI/SMPTE requirements
std::vector<Note> check_dolby_vision_compliance(const DolbyVisionMetadata& dv,
                                                const std::filesystem::path& source);

/// Dolby Atmos IAB (Immersive Audio Bitstream) info
struct AtmosIabInfo
{
  bool detected = false;
  uint32_t object_count = 0;
  uint32_t bed_count = 0;
  uint32_t channel_count = 0;
  uint32_t frame_count = 0;
  double sample_rate = 0.0;
  uint8_t bit_depth = 0;
  bool has_render_config = false;
  std::string version;
  std::string error;
};

/// Deep Dolby Atmos IAB inspection
AtmosIabInfo parse_atmos_iab(const std::filesystem::path& mxf_path);

/// Validate Atmos IAB against ST 2098-2
std::vector<Note> check_atmos_compliance(const AtmosIabInfo& info,
                                         const std::filesystem::path& source);

/// HDR metadata types
enum class HdrType
{
  none,
  pq,
  hlg,
  hdr10,
  hdr10plus,
  dolby_vision
};

/// HDR metadata info
struct HdrMetadata
{
  bool detected = false;
  HdrType type = HdrType::none;
  uint16_t max_cll = 0; // Max Content Light Level
  uint16_t max_fall = 0; // Max Frame Average Light Level
  double master_display_max = 0; // Mastering display max luminance (nits)
  double master_display_min = 0; // Mastering display min luminance (nits)
  std::string color_primaries; // e.g. "BT.2020"
  std::string transfer_function; // e.g. "PQ", "HLG"
  std::string error;
};

/// Detect HDR metadata (SMPTE ST 2098)
HdrMetadata detect_hdr_metadata(const std::filesystem::path& mxf_path);

/// Validate HDR metadata
std::vector<Note> check_hdr_compliance(const HdrMetadata& hdr, const std::filesystem::path& source);

/// Netflix IMF delivery specification checks
struct NetflixDeliveryResult
{
  bool compliant = true;
  std::string app_id; // Expected Application ID
  std::vector<std::string> violations;
};

/// Check against Netflix IMF delivery specs
NetflixDeliveryResult check_netflix_delivery(const std::filesystem::path& imf_dir);

/// Convert Netflix results to notes
std::vector<Note> netflix_to_notes(const NetflixDeliveryResult& result,
                                   const std::filesystem::path& source);

/// ProRes essence detection
struct ProResInfo
{
  bool detected = false;
  std::string codec_variant; // "ProRes 422", "ProRes 4444", etc.
  uint32_t width = 0;
  uint32_t height = 0;
  double frame_rate = 0.0;
};

/// Detect ProRes tracks in MXF
ProResInfo detect_prores(const std::filesystem::path& mxf_path);

/// Extended HFR (up to 120fps) and High Bitrate validation
struct ExtendedHfrInfo
{
  double frame_rate = 0.0;
  bool is_hfr = false; // > 30fps
  bool is_ultra_hfr = false; // > 60fps (e.g. 120fps)
  double bitrate_mbps = 0.0;
  bool is_hbr = false; // > 250 Mbps
  bool exceeds_dci_limit = false;
};

/// Validate extended HFR/HBR content
std::vector<Note> check_extended_hfr(const std::filesystem::path& cpl_path);

/// Accessibility track types
enum class AccessibilityType
{
  audio_description,
  hi_subtitles,
  closed_captions,
  sign_language
};

/// Accessibility track info
struct AccessibilityTrack
{
  AccessibilityType type;
  std::string language;
  std::string label;
  bool valid = false;
};

/// Validate accessibility tracks in a DCP/IMF
std::vector<Note> check_accessibility(const std::filesystem::path& package_dir);

/// Content fingerprint (perceptual hash)
struct ContentFingerprint
{
  std::string hash; // Hex-encoded perceptual hash
  uint32_t frame_sampled = 0; // Which frame was sampled
  uint32_t width = 0;
  uint32_t height = 0;
};

/// Generate perceptual fingerprint from first decoded frame
ContentFingerprint generate_fingerprint(const std::filesystem::path& mxf_path);

/// Compare two fingerprints (0.0 = identical, 1.0 = completely different)
double compare_fingerprints(const ContentFingerprint& a, const ContentFingerprint& b);

} // namespace dcpdoctor
