#pragma once

#include "dcpdoctor/dcpdoctor.h"
#include <filesystem>
#include <vector>

namespace dcpdoctor
{

/// Validate a subtitle/timed text XML file (SMPTE 428-7 or Interop CineCanvas)
std::vector<Note> validate_subtitle(const std::filesystem::path& xml_path, Standard standard);

} // namespace dcpdoctor
