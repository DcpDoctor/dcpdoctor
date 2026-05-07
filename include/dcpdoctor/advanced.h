#pragma once

#include "dcpdoctor/dcpdoctor.h"
#include <filesystem>
#include <vector>

namespace dcpdoctor
{

/// BV2.1 (SMPTE Bv2.1) application profile compliance check
/// This is what theaters actually require - more strict than raw SMPTE
std::vector<Note> check_bv21_compliance(const std::filesystem::path& dcp_dir, Standard standard);

/// Manifest comparison: compare DCP against a reference manifest
/// manifest_path should be a JSON file listing expected assets, hashes, sizes
std::vector<Note> compare_manifest(const std::filesystem::path& dcp_dir,
                                   const std::filesystem::path& manifest_path);

/// Batch processing summary: multi-DCP pass/fail report
struct BatchResult
{
  std::filesystem::path dcp_path;
  bool passed = false;
  int errors = 0;
  int warnings = 0;
  Standard standard = Standard::unknown;
};

void write_batch_summary(std::ostream& out, const std::vector<BatchResult>& results);

} // namespace dcpdoctor
