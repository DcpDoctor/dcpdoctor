#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace dcpdoctor
{

struct ChecksumEntry
{
  std::string asset_id;
  std::string filename;
  std::string expected_hash;
  std::string computed_hash;
  int64_t expected_size = 0;
  int64_t actual_size = 0;
  bool hash_match = false;
  bool size_match = false;
  bool file_exists = false;
};

struct ChecksumVerifyResult
{
  std::vector<ChecksumEntry> entries;
  uint32_t total_assets = 0;
  uint32_t verified_ok = 0;
  uint32_t hash_mismatches = 0;
  uint32_t size_mismatches = 0;
  uint32_t missing_files = 0;
  bool all_valid = false;
  bool success = false;
  std::string error;
};

struct ChecksumVerifyOptions
{
  std::filesystem::path package_dir; // DCP or IMP directory
  bool verify_sizes = true;
  bool verify_hashes = true;
  bool stop_on_first_error = false;
};

/// Verify all asset checksums in a DCP/IMF package against PKL hashes
ChecksumVerifyResult verify_package_checksums(const ChecksumVerifyOptions& opts);

} // namespace dcpdoctor
