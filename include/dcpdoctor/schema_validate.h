#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace dcpdoctor
{

struct SchemaError
{
  std::string file;
  uint32_t line = 0;
  uint32_t column = 0;
  std::string message;
  bool is_warning = false;
};

struct SchemaValidationResult
{
  bool valid = false;
  std::vector<SchemaError> errors;
  std::string schema_version; // e.g. "ST 2067-2:2020"
};

struct SchemaValidateOptions
{
  std::filesystem::path imp_dir;
  std::filesystem::path schema_dir; // Directory containing XSD files (optional, uses built-in)
  bool validate_cpl = true;
  bool validate_pkl = true;
  bool validate_assetmap = true;
  bool strict = false; // Fail on warnings
};

// Validate IMP XML files against SMPTE ST 2067 XSD schemas
SchemaValidationResult validate_against_schema(const SchemaValidateOptions& opts);

// Validate a single XML file against a specific XSD
SchemaValidationResult validate_xml_file(const std::filesystem::path& xml_file,
                                         const std::filesystem::path& xsd_file);

} // namespace dcpdoctor
