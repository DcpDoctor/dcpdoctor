#pragma once

#include "dcpdoctor/dcpdoctor.h"
#include <string>
#include <vector>

namespace dcpdoctor
{

/// Check if a CPL content title text follows ISDCF naming convention
/// Returns notes for any naming violations
std::vector<Note> check_isdcf_naming(const std::string& content_title,
                                     const std::filesystem::path& cpl_path);

} // namespace dcpdoctor
