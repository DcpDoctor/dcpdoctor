#include <spdlog/spdlog.h>
#include <chrono>
#include <set>
#include <sstream>
#include <thread>

#include "dcpdoctor/server.h"
#include "dcpdoctor/report.h"

#ifndef _WIN32
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#endif

namespace dcpdoctor
{
namespace fs = std::filesystem;

void watch_directory(const fs::path& dir, const VerifyOptions& opts, WatchCallback callback,
                     int poll_interval_ms)
{
  spdlog::info("Watching directory: {}", dir.string());

  std::set<std::string> processed;

  while(true)
  {
    std::error_code ec;
    for(auto& entry : fs::directory_iterator(dir, ec))
    {
      if(!entry.is_directory())
        continue;

      auto path = entry.path();
      auto key = path.string();

      // Skip already processed
      if(processed.contains(key))
        continue;

      // Check if this looks like a DCP (has ASSETMAP)
      if(!fs::exists(path / "ASSETMAP.xml") && !fs::exists(path / "ASSETMAP"))
        continue;

      spdlog::info("New DCP detected: {}", path.string());
      processed.insert(key);

      auto result = verify(path, opts);
      callback(path, result);
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(poll_interval_ms));
  }
}

// Minimal REST API implementation using raw sockets
// For production use, would integrate cpp-httplib or similar
void serve_api(const std::string& bind_addr, int port, const VerifyOptions& default_opts)
{
#ifdef _WIN32
  spdlog::error("REST API server is not yet supported on Windows");
  return;
#else
  spdlog::info("REST API server starting on {}:{}", bind_addr, port);
  spdlog::info("Endpoints:");
  spdlog::info("  POST /validate  - validate a DCP directory");
  spdlog::info("  GET  /health    - health check");

  // Use POSIX sockets for minimal dependency
  int server_fd = socket(AF_INET, SOCK_STREAM, 0);
  if(server_fd < 0)
  {
    spdlog::error("Failed to create socket");
    return;
  }

  int opt = 1;
  setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

  struct sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = INADDR_ANY;
  addr.sin_port = htons(port);

  if(bind(server_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0)
  {
    spdlog::error("Failed to bind to port {}", port);
    close(server_fd);
    return;
  }

  listen(server_fd, 10);
  spdlog::info("Server listening on port {}", port);

  while(true)
  {
    int client_fd = accept(server_fd, nullptr, nullptr);
    if(client_fd < 0)
      continue;

    // Read request
    char buf[8192] = {};
    ssize_t n = read(client_fd, buf, sizeof(buf) - 1);
    if(n <= 0)
    {
      close(client_fd);
      continue;
    }

    std::string request(buf, n);

    // Parse method and path
    std::string response;
    if(request.starts_with("GET /health"))
    {
      response = "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n\r\n"
                 "{\"status\":\"ok\",\"version\":\"0.1.0\"}\n";
    }
    else if(request.starts_with("POST /validate"))
    {
      // Extract JSON body (after \r\n\r\n)
      auto body_pos = request.find("\r\n\r\n");
      std::string body = (body_pos != std::string::npos) ? request.substr(body_pos + 4) : "";

      // Simple JSON path extraction (find "path":"...")
      std::string dcp_path;
      auto path_pos = body.find("\"path\"");
      if(path_pos != std::string::npos)
      {
        auto quote1 = body.find('"', path_pos + 6);
        auto quote2 = body.find('"', quote1 + 1);
        if(quote1 != std::string::npos && quote2 != std::string::npos)
          dcp_path = body.substr(quote1 + 1, quote2 - quote1 - 1);
      }

      if(dcp_path.empty() || !fs::is_directory(dcp_path))
      {
        response = "HTTP/1.1 400 Bad Request\r\nContent-Type: application/json\r\n\r\n"
                   "{\"error\":\"Invalid or missing path\"}\n";
      }
      else
      {
        auto result = verify(fs::path(dcp_path), default_opts);
        std::ostringstream json_out;
        write_report(result, fs::path(dcp_path), json_out, ReportFormat::json);
        std::string json_body = json_out.str();

        response = "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n"
                   "Content-Length: " +
                   std::to_string(json_body.size()) + "\r\n\r\n" + json_body;
      }
    }
    else
    {
      response = "HTTP/1.1 404 Not Found\r\nContent-Type: application/json\r\n\r\n"
                 "{\"error\":\"Not found\"}\n";
    }

    write(client_fd, response.c_str(), response.size());
    close(client_fd);
  }

  close(server_fd);
#endif // _WIN32
}

} // namespace dcpdoctor
