#pragma once

#include "dcpdoctor/dcpdoctor.h"
#include <filesystem>
#include <string>
#include <vector>

namespace dcpdoctor {

/// Result of comparing two DCPs
struct DcpDiff {
    struct AssetDiff {
        std::string id;
        std::string filename;
        enum class Status { same, modified, added, removed } status;
        std::string detail;  // e.g. "size changed: 1234 → 5678"
    };

    std::filesystem::path dcp_a;
    std::filesystem::path dcp_b;
    Standard standard_a = Standard::unknown;
    Standard standard_b = Standard::unknown;
    std::vector<AssetDiff> assets;
    bool structure_identical = false;
    bool content_identical = false;
};

/// Compare two DCPs structurally and optionally by content hash
DcpDiff compare_dcps(const std::filesystem::path& dcp_a,
                      const std::filesystem::path& dcp_b,
                      bool check_hashes = false);

/// Format diff result as text
void write_diff_report(std::ostream& out, const DcpDiff& diff);

} // namespace dcpdoctor
