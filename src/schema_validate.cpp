#include "dcpdoctor/schema_validate.h"
#include "dcpdoctor/platform.h"
#include <spdlog/spdlog.h>
#include <array>
#include <fstream>
#include <regex>
#include <sstream>
#include <string>

namespace fs = std::filesystem;

namespace dcpdoctor
{

// Parse xmllint error output into structured SchemaError objects
static std::vector<SchemaError> parse_xmllint_output(const std::string& output)
{
  std::vector<SchemaError> errors;
  // xmllint format: filename:line: element error : message
  std::regex error_re(R"RE(([^:]+):(\d+):\s*(.*))RE");
  std::istringstream stream(output);
  std::string line;
  while(std::getline(stream, line))
  {
    if(line.empty())
      continue;
    std::smatch m;
    if(std::regex_match(line, m, error_re))
    {
      SchemaError err;
      err.file = m[1].str();
      err.line = static_cast<uint32_t>(std::stoul(m[2].str()));
      err.message = m[3].str();
      err.is_warning = line.find("warning") != std::string::npos;
      errors.push_back(std::move(err));
    }
    else if(!line.empty() && !errors.empty())
    {
      errors.back().message += " " + line;
    }
  }
  return errors;
}

static bool xmllint_available()
{
#ifdef _WIN32
  return system("where xmllint >NUL 2>&1") == 0;
#else
  return system("which xmllint >/dev/null 2>&1") == 0;
#endif
}

// Find XML files of a given type in an IMP directory
static std::vector<fs::path> find_xml_files(const fs::path& imp_dir, const std::string& root_tag)
{
  std::vector<fs::path> results;
  for(const auto& entry : fs::directory_iterator(imp_dir))
  {
    if(!entry.is_regular_file())
      continue;
    auto ext = entry.path().extension().string();
    if(ext != ".xml" && ext != ".XML")
      continue;

    // Quick check: read first few lines for root element
    std::ifstream f(entry.path());
    std::string line;
    int lines_read = 0;
    while(std::getline(f, line) && lines_read < 10)
    {
      if(line.find("<" + root_tag) != std::string::npos)
      {
        results.push_back(entry.path());
        break;
      }
      ++lines_read;
    }
  }
  return results;
}

SchemaValidationResult validate_xml_file(const fs::path& xml_file, const fs::path& xsd_file)
{
  SchemaValidationResult result;

  if(!fs::exists(xml_file))
  {
    SchemaError err;
    err.file = xml_file.string();
    err.message = "File not found";
    result.errors.push_back(std::move(err));
    return result;
  }

  if(!fs::exists(xsd_file))
  {
    SchemaError err;
    err.file = xsd_file.string();
    err.message = "Schema file not found";
    result.errors.push_back(std::move(err));
    return result;
  }

  std::ostringstream cmd;
  cmd << "xmllint --noout --schema " << xsd_file.string() << " " << xml_file.string() << " 2>&1";

  std::array<char, 4096> buf{};
  std::string output;
  FILE* pipe = DCPDOCTOR_POPEN(cmd.str().c_str(), "r");
  if(!pipe)
  {
    SchemaError err;
    err.message = "Failed to execute xmllint";
    result.errors.push_back(std::move(err));
    return result;
  }
  while(fgets(buf.data(), static_cast<int>(buf.size()), pipe))
    output += buf.data();
  int ret = DCPDOCTOR_PCLOSE(pipe);

  result.errors = parse_xmllint_output(output);

  // xmllint prints "filename validates" on success
  if(ret == 0 && output.find("validates") != std::string::npos)
    result.valid = true;

  return result;
}

SchemaValidationResult validate_against_schema(const SchemaValidateOptions& opts)
{
  SchemaValidationResult result;

  if(!fs::exists(opts.imp_dir))
  {
    SchemaError err;
    err.message = "IMP directory not found: " + opts.imp_dir.string();
    result.errors.push_back(std::move(err));
    return result;
  }

  if(!xmllint_available())
  {
    SchemaError err;
    err.message = "xmllint not found in PATH. Install libxml2-utils (Linux) or "
                  "libxml2 (macOS/Windows)";
    result.errors.push_back(std::move(err));
    return result;
  }

  bool all_valid = true;

  // Validate CPLs
  if(opts.validate_cpl)
  {
    auto cpls = find_xml_files(opts.imp_dir, "CompositionPlaylist");
    if(cpls.empty())
    {
      spdlog::warn("No CPL files found in {}", opts.imp_dir.string());
    }
    for(const auto& cpl : cpls)
    {
      spdlog::info("Validating CPL: {}", cpl.filename().string());

      // Determine schema version from namespace
      std::ifstream f(cpl);
      std::string content((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());

      fs::path schema;
      if(!opts.schema_dir.empty())
      {
        if(content.find("2067-2:2020") != std::string::npos ||
           content.find("2067-2/2020") != std::string::npos)
        {
          schema = opts.schema_dir / "st2067-2-2020.xsd";
          result.schema_version = "ST 2067-2:2020";
        }
        else if(content.find("2067-2:2016") != std::string::npos ||
                content.find("2067-2/2016") != std::string::npos)
        {
          schema = opts.schema_dir / "st2067-2-2016.xsd";
          result.schema_version = "ST 2067-2:2016";
        }
        else
        {
          schema = opts.schema_dir / "st2067-2-2020.xsd";
          result.schema_version = "ST 2067-2:2020 (assumed)";
        }

        if(fs::exists(schema))
        {
          auto r = validate_xml_file(cpl, schema);
          if(!r.valid)
            all_valid = false;
          result.errors.insert(result.errors.end(), r.errors.begin(), r.errors.end());
        }
        else
        {
          spdlog::warn("Schema file not found: {}", schema.string());
          // Fall back to well-formedness check
          std::ostringstream cmd;
          cmd << "xmllint --noout " << cpl.string() << " 2>&1";
          int ret = system(cmd.str().c_str());
          if(ret != 0)
            all_valid = false;
        }
      }
      else
      {
        // No schema dir: just check well-formedness
        std::ostringstream cmd;
        cmd << "xmllint --noout " << cpl.string() << " 2>&1";
        std::array<char, 4096> buf{};
        std::string output;
        FILE* pipe = DCPDOCTOR_POPEN(cmd.str().c_str(), "r");
        if(pipe)
        {
          while(fgets(buf.data(), static_cast<int>(buf.size()), pipe))
            output += buf.data();
          int ret = DCPDOCTOR_PCLOSE(pipe);
          if(ret != 0)
          {
            all_valid = false;
            auto errs = parse_xmllint_output(output);
            result.errors.insert(result.errors.end(), errs.begin(), errs.end());
          }
        }
      }
    }
  }

  // Validate PKLs
  if(opts.validate_pkl)
  {
    auto pkls = find_xml_files(opts.imp_dir, "PackingList");
    for(const auto& pkl : pkls)
    {
      spdlog::info("Validating PKL: {}", pkl.filename().string());
      if(!opts.schema_dir.empty())
      {
        auto schema = opts.schema_dir / "st0429-8-2007.xsd";
        if(fs::exists(schema))
        {
          auto r = validate_xml_file(pkl, schema);
          if(!r.valid)
            all_valid = false;
          result.errors.insert(result.errors.end(), r.errors.begin(), r.errors.end());
        }
      }
    }
  }

  // Validate ASSETMAP
  if(opts.validate_assetmap)
  {
    auto maps = find_xml_files(opts.imp_dir, "AssetMap");
    for(const auto& am : maps)
    {
      spdlog::info("Validating AssetMap: {}", am.filename().string());
      if(!opts.schema_dir.empty())
      {
        auto schema = opts.schema_dir / "st0429-9-2007.xsd";
        if(fs::exists(schema))
        {
          auto r = validate_xml_file(am, schema);
          if(!r.valid)
            all_valid = false;
          result.errors.insert(result.errors.end(), r.errors.begin(), r.errors.end());
        }
      }
    }
  }

  if(opts.strict)
  {
    for(const auto& e : result.errors)
    {
      if(e.is_warning)
      {
        all_valid = false;
        break;
      }
    }
  }

  result.valid = all_valid && result.errors.empty();

  if(result.valid)
    spdlog::info("Schema validation passed");
  else
    spdlog::warn("Schema validation found {} issue(s)", result.errors.size());

  return result;
}

} // namespace dcpdoctor
