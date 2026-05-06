#pragma once

#include "dcpdoctor/dcpdoctor.h"
#include <filesystem>
#include <string>
#include <vector>

namespace dcpdoctor {

/// Hash cache for fast re-validation (avoids rehashing unchanged files)
class HashCache {
public:
    /// Open or create cache at the given path (SQLite DB)
    explicit HashCache(const std::filesystem::path& cache_path);
    ~HashCache();

    /// Get cached hash for file (returns empty if not cached or stale)
    std::string get(const std::filesystem::path& file) const;

    /// Store hash for file (records mtime + size for staleness check)
    void put(const std::filesystem::path& file, const std::string& hash);

    /// Clear all cached entries
    void clear();

    /// Number of cached entries
    int size() const;

    bool is_open() const { return db_ != nullptr; }

private:
    void* db_ = nullptr;  // sqlite3*
};

} // namespace dcpdoctor
