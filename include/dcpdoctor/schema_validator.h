#pragma once

#include "dcpdoctor/dcpdoctor.h"
#include <filesystem>
#include <string>
#include <vector>

namespace dcpdoctor
{

/// Validate XML against XSD schema
std::vector<Note> validate_schema(const std::filesystem::path& xml_file,
                                  const std::filesystem::path& schema_file);

/// Validate XML against embedded namespace schemas
std::vector<Note> validate_namespace(const std::filesystem::path& xml_file, Standard expected);

} // namespace dcpdoctor
