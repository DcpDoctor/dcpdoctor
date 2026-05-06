#pragma once

#include "dcpdoctor/dcpdoctor.h"
#include <filesystem>
#include <iosfwd>
#include <string>

namespace dcpdoctor {

enum class ReportFormat { text, json, html };

void write_report(const VerifyResult& result,
                  const std::filesystem::path& dcp_dir,
                  std::ostream& out,
                  ReportFormat format = ReportFormat::text);

} // namespace dcpdoctor
