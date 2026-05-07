#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace dcpdoctor
{

struct PklAsset
{
  std::string id;
  std::string type;
  std::string original_filename;
  std::string hash;
  std::string hash_algorithm;
  int64_t size = 0;
};

struct Pkl
{
  std::string id;
  std::string creator;
  std::string issue_date;
  std::vector<PklAsset> assets;

  static std::optional<Pkl> parse(const std::filesystem::path& file);
};

} // namespace dcpdoctor
