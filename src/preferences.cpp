#include "dcpdoctor/preferences.h"
#include "postkit/preferences.h"

#include <spdlog/spdlog.h>

#include <cstdlib>
#include <fstream>
#include <sstream>

static constexpr uint32_t CURRENT_PREFS_VERSION = 1;

static std::vector<postkit::PrefsMigration> migrations()
{
  return {
    {1, "Initial versioned schema", [](std::string const& json) {
      auto j = postkit::json_insert_if_missing(json, "default_standard", "\"SMPTE\"");
      j = postkit::json_insert_if_missing(j, "verify_hashes", "true");
      j = postkit::json_insert_if_missing(j, "verify_schemas", "true");
      j = postkit::json_insert_if_missing(j, "check_bitrate", "true");
      j = postkit::json_insert_if_missing(j, "check_loudness", "false");
      j = postkit::json_insert_if_missing(j, "max_bitrate_mbps", "250");
      j = postkit::json_insert_if_missing(j, "theme", "\"dark\"");
      j = postkit::json_insert_if_missing(j, "show_advanced_options", "false");
      return j;
    }},
  };
}

namespace dcpdoctor
{

std::filesystem::path preferences_path()
{
#ifdef _WIN32
  const char* appdata = std::getenv("APPDATA");
  if (appdata)
    return std::filesystem::path(appdata) / "dcpdoctor" / "preferences.json";
  return "preferences.json";
#elif defined(__APPLE__)
  const char* home = std::getenv("HOME");
  if (home)
    return std::filesystem::path(home) / "Library" / "Application Support" / "dcpdoctor" / "preferences.json";
  return "preferences.json";
#else
  const char* xdg = std::getenv("XDG_CONFIG_HOME");
  if (xdg)
    return std::filesystem::path(xdg) / "dcpdoctor" / "preferences.json";
  const char* home = std::getenv("HOME");
  if (home)
    return std::filesystem::path(home) / ".config" / "dcpdoctor" / "preferences.json";
  return "preferences.json";
#endif
}

static std::string json_string(const std::string& json, const std::string& key)
{
  auto pos = json.find("\"" + key + "\"");
  if (pos == std::string::npos) return "";
  pos = json.find(':', pos);
  if (pos == std::string::npos) return "";
  pos = json.find('"', pos + 1);
  if (pos == std::string::npos) return "";
  auto end = json.find('"', pos + 1);
  if (end == std::string::npos) return "";
  return json.substr(pos + 1, end - pos - 1);
}

static int json_int(const std::string& json, const std::string& key, int def)
{
  auto pos = json.find("\"" + key + "\"");
  if (pos == std::string::npos) return def;
  pos = json.find(':', pos);
  if (pos == std::string::npos) return def;
  pos++;
  while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t')) pos++;
  try { return std::stoi(json.substr(pos)); } catch (...) { return def; }
}

static bool json_bool(const std::string& json, const std::string& key, bool def)
{
  auto pos = json.find("\"" + key + "\"");
  if (pos == std::string::npos) return def;
  pos = json.find(':', pos);
  if (pos == std::string::npos) return def;
  pos++;
  while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t')) pos++;
  if (json.substr(pos, 4) == "true") return true;
  if (json.substr(pos, 5) == "false") return false;
  return def;
}

Preferences load_preferences()
{
  Preferences prefs;
  auto path = preferences_path();

  if (!std::filesystem::exists(path))
  {
    spdlog::debug("No preferences file at {}, using defaults", path.string());
    return prefs;
  }

  std::ifstream f(path);
  if (!f.is_open()) return prefs;

  std::ostringstream ss;
  ss << f.rdbuf();
  std::string json = ss.str();
  f.close();

  uint32_t file_version = postkit::prefs_version(json);
  if (file_version < CURRENT_PREFS_VERSION)
  {
    spdlog::info("Migrating preferences from version {} to {}", file_version, CURRENT_PREFS_VERSION);
    json = postkit::migrate_preferences(json, migrations());
    std::ofstream out(path);
    if (out.is_open())
    {
      out << json;
      out.close();
    }
  }

  auto s = [&](const std::string& key, std::string& field) {
    auto v = json_string(json, key);
    if (!v.empty()) field = v;
  };

  s("default_standard", prefs.default_standard);
  prefs.verify_hashes = json_bool(json, "verify_hashes", true);
  prefs.verify_schemas = json_bool(json, "verify_schemas", true);
  prefs.check_bitrate = json_bool(json, "check_bitrate", true);
  prefs.check_loudness = json_bool(json, "check_loudness", true);
  prefs.max_bitrate_mbps = static_cast<uint32_t>(json_int(json, "max_bitrate_mbps", 250));
  s("report_format", prefs.report_format);
  s("default_output_dir", prefs.default_output_dir);
  s("schema_dir", prefs.schema_dir);
  s("signing_certificate_path", prefs.signing_certificate_path);
  s("signing_key_path", prefs.signing_key_path);
  prefs.server_port = static_cast<uint32_t>(json_int(json, "server_port", 8080));
  s("server_bind", prefs.server_bind);
  s("theme", prefs.theme);
  prefs.show_advanced_options = json_bool(json, "show_advanced_options", false);

  spdlog::debug("Loaded preferences from {}", path.string());
  return prefs;
}

static std::string escape_json(const std::string& s)
{
  std::string out;
  for (char c : s)
  {
    if (c == '"') out += "\\\"";
    else if (c == '\\') out += "\\\\";
    else out += c;
  }
  return out;
}

int save_preferences(const Preferences& prefs)
{
  auto path = preferences_path();
  std::filesystem::create_directories(path.parent_path());

  std::ofstream f(path);
  if (!f.is_open())
  {
    spdlog::error("Failed to write preferences to {}", path.string());
    return 1;
  }

  f << "{\n";
  f << "  \"version\": " << CURRENT_PREFS_VERSION << ",\n";
  f << "  \"default_standard\": \"" << escape_json(prefs.default_standard) << "\",\n";
  f << "  \"verify_hashes\": " << (prefs.verify_hashes ? "true" : "false") << ",\n";
  f << "  \"verify_schemas\": " << (prefs.verify_schemas ? "true" : "false") << ",\n";
  f << "  \"check_bitrate\": " << (prefs.check_bitrate ? "true" : "false") << ",\n";
  f << "  \"check_loudness\": " << (prefs.check_loudness ? "true" : "false") << ",\n";
  f << "  \"max_bitrate_mbps\": " << prefs.max_bitrate_mbps << ",\n";
  f << "  \"report_format\": \"" << escape_json(prefs.report_format) << "\",\n";
  f << "  \"default_output_dir\": \"" << escape_json(prefs.default_output_dir) << "\",\n";
  f << "  \"schema_dir\": \"" << escape_json(prefs.schema_dir) << "\",\n";
  f << "  \"signing_certificate_path\": \"" << escape_json(prefs.signing_certificate_path) << "\",\n";
  f << "  \"signing_key_path\": \"" << escape_json(prefs.signing_key_path) << "\",\n";
  f << "  \"server_port\": " << prefs.server_port << ",\n";
  f << "  \"server_bind\": \"" << escape_json(prefs.server_bind) << "\",\n";
  f << "  \"theme\": \"" << escape_json(prefs.theme) << "\",\n";
  f << "  \"show_advanced_options\": " << (prefs.show_advanced_options ? "true" : "false") << ",\n";
  f << "  \"theater_list\": [\n";
  for (size_t i = 0; i < prefs.theater_list.size(); i++)
  {
    f << "    {\"name\": \"" << escape_json(prefs.theater_list[i].name)
      << "\", \"certificate_path\": \"" << escape_json(prefs.theater_list[i].certificate_path) << "\"}";
    if (i + 1 < prefs.theater_list.size()) f << ",";
    f << "\n";
  }
  f << "  ]\n";
  f << "}\n";

  spdlog::info("Saved preferences to {}", path.string());
  return 0;
}

} // namespace dcpdoctor
