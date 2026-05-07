#pragma once

#include <cstdint>
#include <optional>
#include <string>

namespace dcpdoctor
{

// SMPTE ST 2067-21 HDR/WCG color metadata for CPL
enum class TransferFunction
{
  SDR_BT1886, // BT.1886 (traditional SDR)
  PQ, // SMPTE ST 2084 (HDR10, Dolby Vision base)
  HLG, // ARIB STD-B67 (Hybrid Log-Gamma)
  Linear, // Scene-linear (ACES)
};

enum class Colorimetry
{
  BT709, // ITU-R BT.709
  BT2020, // ITU-R BT.2020 (wide color gamut)
  P3D65, // DCI-P3 with D65 white
  P3DCI, // DCI-P3 with DCI white
  ACES, // Academy Color Encoding System
};

enum class Quantization
{
  QE1, // Full range (0-1023 for 10-bit)
  QE2, // Narrow range (64-940 for 10-bit)
};

struct MasteringDisplay
{
  // CIE 1931 xy chromaticity of display primaries (x1000)
  uint16_t red_x = 0, red_y = 0;
  uint16_t green_x = 0, green_y = 0;
  uint16_t blue_x = 0, blue_y = 0;
  uint16_t white_x = 0, white_y = 0;

  // Luminance in cd/m² (nits)
  uint32_t max_luminance = 0; // e.g. 1000, 4000, 10000
  uint32_t min_luminance = 0; // in 0.0001 cd/m² units (e.g. 50 = 0.005 nits)
};

struct ContentLightLevel
{
  uint16_t max_cll = 0; // Maximum Content Light Level
  uint16_t max_fall = 0; // Maximum Frame-Average Light Level
};

struct ImfHdrMetadata
{
  TransferFunction transfer = TransferFunction::SDR_BT1886;
  Colorimetry colorimetry = Colorimetry::BT709;
  Quantization quantization = Quantization::QE2;
  uint16_t bit_depth = 10;

  std::optional<MasteringDisplay> mastering_display;
  std::optional<ContentLightLevel> content_light;
};

// Preset HDR configurations
ImfHdrMetadata hdr10_preset(); // PQ + BT.2020 + typical mastering display
ImfHdrMetadata hlg_preset(); // HLG + BT.2020
ImfHdrMetadata p3d65_sdr_preset(); // SDR + P3-D65

// Generate XML element for CPL color metadata (ST 2067-21)
std::string generate_color_metadata_xml(const ImfHdrMetadata& meta);

} // namespace dcpdoctor
