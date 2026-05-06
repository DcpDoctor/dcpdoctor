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

/// Simple terminal progress bar (respects NO_COLOR, non-TTY)
class ProgressBar {
public:
    explicit ProgressBar(int total, const std::string& label = "");
    void update(int current);
    void finish();

private:
    int total_;
    int last_pct_ = -1;
    std::string label_;
    bool is_tty_;
};

} // namespace dcpdoctor
