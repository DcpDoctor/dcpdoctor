#pragma once

#include "dcpdoctor/dcpdoctor.h"
#include <filesystem>
#include <string>
#include <vector>

namespace dcpdoctor
{

/// IMF CPL (Composition Playlist) specific info
struct ImfCplInfo
{
  std::string id;
  std::string content_title;
  std::string edit_rate;
  std::string annotation;
  std::string application_id;
  uint32_t segment_count = 0;
  uint32_t reel_count = 0;
};

/// IMF (Interoperable Master Format) package info
struct ImfPackageInfo
{
  struct AssetEntry
  {
    std::string id;
    std::string path;
    bool is_packing_list = false;
  };

  bool valid = false;
  bool is_imf = false;
  bool has_assetmap = false;
  bool has_packing_list = false;
  std::string packing_list_id;
  std::vector<AssetEntry> assets;
  std::vector<ImfCplInfo> cpls;
  std::string error;
};

/// Validate an IMF package
ImfPackageInfo validate_imf_package(const std::filesystem::path& imf_dir);

/// Generate validation notes for IMF
std::vector<Note> check_imf_compliance(const ImfPackageInfo& info,
                                       const std::filesystem::path& imf_dir);

} // namespace dcpdoctor
