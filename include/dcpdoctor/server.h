#pragma once

#include "dcpdoctor/dcpdoctor.h"
#include <filesystem>
#include <functional>
#include <string>

namespace dcpdoctor
{

/// Watch a directory for new DCPs and auto-validate them
/// Calls the callback with results for each DCP found
using WatchCallback = std::function<void(const std::filesystem::path&, const VerifyResult&)>;

void watch_directory(const std::filesystem::path& dir, const VerifyOptions& opts,
                     WatchCallback callback, int poll_interval_ms = 5000);

/// Run a REST API server for DCP validation
/// Endpoints:
///   POST /validate  - body: {"path": "/path/to/dcp", "options": {...}}
///   GET  /health    - returns {"status": "ok"}
void serve_api(const std::string& bind_addr, int port, const VerifyOptions& default_opts);

} // namespace dcpdoctor
