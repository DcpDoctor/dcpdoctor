#pragma once

#include "dcpdoctor/dcpdoctor.h"
#include <filesystem>
#include <string>
#include <vector>

namespace dcpdoctor
{

/// Verify XML digital signature
std::vector<Note> verify_signature(const std::filesystem::path& xml_file);

/// Verify certificate chain
std::vector<Note> verify_certificate_chain(const std::filesystem::path& xml_file);

} // namespace dcpdoctor
