#pragma once

#include "dcpdoctor/dcpdoctor.h"
#include <string>
#include <vector>

namespace dcpdoctor {

/// A suggested fix for a detected issue
struct FixSuggestion {
    Code related_code;
    std::string description;    // Human-readable explanation
    std::string command;        // Optional CLI command to fix (empty if manual)
    bool auto_fixable = false;  // Whether dcpdoctor can fix it automatically
};

/// Generate fix suggestions for a set of notes
std::vector<FixSuggestion> suggest_fixes(const std::vector<Note>& notes);

/// Apply auto-fixable fixes (returns count of fixes applied)
int apply_fixes(const std::filesystem::path& dcp_dir,
                const std::vector<FixSuggestion>& suggestions);

} // namespace dcpdoctor
