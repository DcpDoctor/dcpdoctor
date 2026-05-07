#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace dcpdoctor
{

struct Reel
{
  std::string id;
  struct
  {
    std::string id;
    std::string edit_rate;
    int64_t duration = 0;
    int64_t entry_point = 0;
  } picture;
  struct
  {
    std::string id;
    std::string edit_rate;
    int64_t duration = 0;
    int64_t entry_point = 0;
  } sound;
};

struct Cpl
{
  std::string id;
  std::string content_title;
  std::string content_kind;
  std::string issue_date;
  std::vector<Reel> reels;

  static std::optional<Cpl> parse(const std::filesystem::path& file);
};

} // namespace dcpdoctor
