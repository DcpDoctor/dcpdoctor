#include <spdlog/spdlog.h>
#include <algorithm>
#include <array>
#include <cstdio>
#include <sstream>

#include "dcpdoctor/validate.h"
#include "dcpdoctor/platform.h"

namespace dcpdoctor
{

bool photon_available()
{
#ifdef _WIN32
  return system("where java >NUL 2>&1") == 0;
#else
  return system("which java >/dev/null 2>&1") == 0;
#endif
}

ValidationResult validate_with_photon(const std::filesystem::path& imp_dir)
{
  ValidationResult result;

  if(!photon_available())
  {
    result.notes.push_back(
        {ValidationNote::Severity::warning, "Java not found — cannot run Photon validator", ""});
    return result;
  }

  // Look for Photon jar in standard locations
  std::string photon_jar;
  for(const auto& path :
      {"photon.jar", "/usr/local/share/photon/photon.jar", "/opt/photon/photon.jar"})
  {
    if(std::filesystem::exists(path))
    {
      photon_jar = path;
      break;
    }
  }

  if(photon_jar.empty())
  {
    result.notes.push_back({ValidationNote::Severity::warning,
                            "Photon jar not found — install Netflix Photon for IMF validation",
                            ""});
    return result;
  }

  std::string cmd = "java -jar " + photon_jar + " -i " + imp_dir.string() + " 2>&1";

  std::array<char, 4096> buffer;
  std::string output;

  FILE* pipe = DCPDOCTOR_POPEN(cmd.c_str(), "r");
  if(!pipe)
  {
    result.notes.push_back({ValidationNote::Severity::error, "Failed to run Photon", ""});
    return result;
  }

  while(fgets(buffer.data(), buffer.size(), pipe) != nullptr)
    output += buffer.data();

  int ret = DCPDOCTOR_PCLOSE(pipe);
  result.valid = (ret == 0);

  // Parse Photon output for errors/warnings
  std::istringstream stream(output);
  std::string line;
  while(std::getline(stream, line))
  {
    if(line.find("ERROR") != std::string::npos)
    {
      result.notes.push_back({ValidationNote::Severity::error, line, ""});
    }
    else if(line.find("WARN") != std::string::npos)
    {
      result.notes.push_back({ValidationNote::Severity::warning, line, ""});
    }
  }

  spdlog::info(
      "Photon validation {}: {} errors, {} warnings", result.valid ? "passed" : "failed",
      std::count_if(result.notes.begin(), result.notes.end(),
                    [](const auto& n) { return n.severity == ValidationNote::Severity::error; }),
      std::count_if(result.notes.begin(), result.notes.end(),
                    [](const auto& n) { return n.severity == ValidationNote::Severity::warning; }));

  return result;
}

} // namespace dcpdoctor
