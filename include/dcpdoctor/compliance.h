#pragma once

#include "dcpdoctor/dcpdoctor.h"
#include <filesystem>
#include <vector>

namespace dcpdoctor
{

/// SMPTE ST 429 and SMPTE ST 2067 (BV2.1) compliance checks
std::vector<Note> check_smpte_compliance(const std::filesystem::path& dcp_dir, Standard standard,
                                         bool strict);

} // namespace dcpdoctor
