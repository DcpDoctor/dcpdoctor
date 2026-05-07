#include <filesystem>
#include <cstring>

#include "dcpdoctor/cache.h"

// Minimal SQLite3 interface (we use the system sqlite3 if available,
// otherwise provide a simple file-based fallback)
#ifdef __has_include
#if __has_include(<sqlite3.h>)
#define HAS_SQLITE 1
#include <sqlite3.h>
#endif
#endif

#ifndef HAS_SQLITE
// Fallback: simple file-based cache using a text format
#include <fstream>
#include <map>
#include <sstream>
#endif

namespace dcpdoctor
{
namespace fs = std::filesystem;

#ifdef HAS_SQLITE

HashCache::HashCache(const fs::path& cache_path)
{
  sqlite3* db = nullptr;
  if(sqlite3_open(cache_path.string().c_str(), &db) == SQLITE_OK)
  {
    db_ = db;
    const char* sql = "CREATE TABLE IF NOT EXISTS hashes ("
                      "path TEXT PRIMARY KEY, "
                      "hash TEXT NOT NULL, "
                      "mtime INTEGER NOT NULL, "
                      "size INTEGER NOT NULL)";
    sqlite3_exec(static_cast<sqlite3*>(db_), sql, nullptr, nullptr, nullptr);
  }
}

HashCache::~HashCache()
{
  if(db_)
    sqlite3_close(static_cast<sqlite3*>(db_));
}

std::string HashCache::get(const fs::path& file) const
{
  if(!db_)
    return {};

  auto* db = static_cast<sqlite3*>(db_);
  std::error_code ec;
  auto mtime = fs::last_write_time(file, ec);
  auto size = fs::file_size(file, ec);
  if(ec)
    return {};

  auto mtime_val = mtime.time_since_epoch().count();

  sqlite3_stmt* stmt = nullptr;
  const char* sql = "SELECT hash FROM hashes WHERE path = ? AND mtime = ? AND size = ?";
  if(sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK)
    return {};

  std::string path_str = file.string();
  sqlite3_bind_text(stmt, 1, path_str.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_int64(stmt, 2, mtime_val);
  sqlite3_bind_int64(stmt, 3, static_cast<int64_t>(size));

  std::string result;
  if(sqlite3_step(stmt) == SQLITE_ROW)
  {
    auto text = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
    if(text)
      result = text;
  }
  sqlite3_finalize(stmt);
  return result;
}

void HashCache::put(const fs::path& file, const std::string& hash)
{
  if(!db_)
    return;

  auto* db = static_cast<sqlite3*>(db_);
  std::error_code ec;
  auto mtime = fs::last_write_time(file, ec);
  auto size = fs::file_size(file, ec);
  if(ec)
    return;

  auto mtime_val = mtime.time_since_epoch().count();

  sqlite3_stmt* stmt = nullptr;
  const char* sql = "INSERT OR REPLACE INTO hashes (path, hash, mtime, size) VALUES (?, ?, ?, ?)";
  if(sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK)
    return;

  std::string path_str = file.string();
  sqlite3_bind_text(stmt, 1, path_str.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 2, hash.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_int64(stmt, 3, mtime_val);
  sqlite3_bind_int64(stmt, 4, static_cast<int64_t>(size));

  sqlite3_step(stmt);
  sqlite3_finalize(stmt);
}

void HashCache::clear()
{
  if(!db_)
    return;
  sqlite3_exec(static_cast<sqlite3*>(db_), "DELETE FROM hashes", nullptr, nullptr, nullptr);
}

int HashCache::size() const
{
  if(!db_)
    return 0;
  auto* db = static_cast<sqlite3*>(db_);
  sqlite3_stmt* stmt = nullptr;
  if(sqlite3_prepare_v2(db, "SELECT COUNT(*) FROM hashes", -1, &stmt, nullptr) != SQLITE_OK)
    return 0;
  int count = 0;
  if(sqlite3_step(stmt) == SQLITE_ROW)
    count = sqlite3_column_int(stmt, 0);
  sqlite3_finalize(stmt);
  return count;
}

#else
// Fallback implementation without SQLite

struct CacheEntry
{
  std::string hash;
  int64_t mtime;
  int64_t size;
};

// Store cache in a simple map (persisted to file on destruction)
struct CacheData
{
  fs::path path;
  std::map<std::string, CacheEntry> entries;
};

HashCache::HashCache(const fs::path& cache_path)
{
  auto* data = new CacheData;
  data->path = cache_path;
  db_ = data;

  // Load existing cache
  std::ifstream f(cache_path);
  if(f)
  {
    std::string line;
    while(std::getline(f, line))
    {
      std::istringstream iss(line);
      std::string path, hash;
      int64_t mtime, size;
      if(iss >> path >> hash >> mtime >> size)
        data->entries[path] = {hash, mtime, size};
    }
  }
}

HashCache::~HashCache()
{
  if(!db_)
    return;
  auto* data = static_cast<CacheData*>(db_);

  // Save to file
  std::ofstream f(data->path);
  if(f)
  {
    for(auto& [path, entry] : data->entries)
      f << path << " " << entry.hash << " " << entry.mtime << " " << entry.size << "\n";
  }
  delete data;
}

std::string HashCache::get(const fs::path& file) const
{
  if(!db_)
    return {};
  auto* data = static_cast<CacheData*>(db_);

  std::error_code ec;
  auto mtime = fs::last_write_time(file, ec);
  auto size = fs::file_size(file, ec);
  if(ec)
    return {};

  auto it = data->entries.find(file.string());
  if(it == data->entries.end())
    return {};

  if(it->second.mtime != mtime.time_since_epoch().count())
    return {};
  if(it->second.size != static_cast<int64_t>(size))
    return {};

  return it->second.hash;
}

void HashCache::put(const fs::path& file, const std::string& hash)
{
  if(!db_)
    return;
  auto* data = static_cast<CacheData*>(db_);

  std::error_code ec;
  auto mtime = fs::last_write_time(file, ec);
  auto size = fs::file_size(file, ec);
  if(ec)
    return;

  data->entries[file.string()] = {hash, mtime.time_since_epoch().count(),
                                  static_cast<int64_t>(size)};
}

void HashCache::clear()
{
  if(!db_)
    return;
  static_cast<CacheData*>(db_)->entries.clear();
}

int HashCache::size() const
{
  if(!db_)
    return 0;
  return static_cast<int>(static_cast<CacheData*>(db_)->entries.size());
}

#endif

} // namespace dcpdoctor
