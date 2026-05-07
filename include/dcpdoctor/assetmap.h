#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace dcpdoctor
{

struct Asset
{
  std::string id;
  std::string path;
  std::string type;
  std::optional<std::string> hash;
  std::optional<int64_t> size;
};

struct AssetMap
{
  std::string id;
  std::string creator;
  std::string issue_date;
  std::vector<Asset> assets;

  static std::optional<AssetMap> parse(const std::filesystem::path& file);
};

} // namespace dcpdoctor
