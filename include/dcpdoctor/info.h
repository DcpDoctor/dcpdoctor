#pragma once

#include <filesystem>
#include <string>
#include <vector>
#include <cstdint>

namespace dcpdoctor
{

struct ImpTrackInfo
{
  std::string uuid;
  std::string filename;
  std::string type; // "video", "audio"
  uint64_t size = 0;
};

struct ImpInfo
{
  std::filesystem::path path;
  std::string cpl_uuid;
  std::string cpl_title;
  std::string pkl_uuid;
  std::string issuer;
  std::string issue_date;
  std::vector<ImpTrackInfo> tracks;
  bool valid = false;
  std::string error;
};

// Read and display info about an existing IMP
ImpInfo read_imp_info(const std::filesystem::path& imp_dir);

} // namespace dcpdoctor
