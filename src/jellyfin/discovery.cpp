#include "jellyfin/discovery.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <chrono>
#include <nlohmann/json.hpp>

namespace zerofin::jf {

Result<DiscoveredServer> parse_discovery_reply(std::string_view text) {
  try {
    auto j = nlohmann::json::parse(text);
    if (!j.is_object()) return Error{ErrorKind::Parse, "expected a JSON object", 0};
    auto get = [&](const char* key) {
      auto it = j.find(key);
      return (it != j.end() && it->is_string()) ? it->get<std::string>() : std::string();
    };
    DiscoveredServer s{get("Id"), get("Name"), get("Address")};
    if (s.id.empty() || s.address.empty()) return Error{ErrorKind::Parse, "discovery reply is incomplete", 0};
    return s;
  } catch (const nlohmann::json::exception& e) {
    return Error{ErrorKind::Parse, e.what(), 0};
  }
}

std::vector<DiscoveredServer> discover_servers(int timeout_ms) {
  std::vector<DiscoveredServer> found;
  const int fd = ::socket(AF_INET, SOCK_DGRAM, 0);
  if (fd < 0) return found;

  int yes = 1;
  ::setsockopt(fd, SOL_SOCKET, SO_BROADCAST, &yes, sizeof(yes));

  sockaddr_in dest{};
  dest.sin_family = AF_INET;
  dest.sin_port = htons(7359);
  dest.sin_addr.s_addr = htonl(INADDR_BROADCAST);
  static const char kQuery[] = "who is JellyfinServer?";
  if (::sendto(fd, kQuery, sizeof(kQuery) - 1, 0, reinterpret_cast<sockaddr*>(&dest), sizeof(dest)) < 0) {
    ::close(fd);
    return found;
  }

  using Clock = std::chrono::steady_clock;
  const auto deadline = Clock::now() + std::chrono::milliseconds(timeout_ms);
  while (true) {
    const auto left = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - Clock::now()).count();
    if (left <= 0) break;
    pollfd pfd{fd, POLLIN, 0};
    if (::poll(&pfd, 1, static_cast<int>(left)) <= 0) break;
    char buf[2048];
    const ssize_t n = ::recv(fd, buf, sizeof(buf), 0);
    if (n <= 0) break;
    auto parsed = parse_discovery_reply(std::string_view(buf, static_cast<size_t>(n)));
    if (!parsed) continue;
    bool duplicate = false;
    for (const auto& s : found) duplicate = duplicate || s.id == parsed.value().id;
    if (!duplicate) found.push_back(parsed.value());
  }
  ::close(fd);
  return found;
}

}  // namespace zerofin::jf
