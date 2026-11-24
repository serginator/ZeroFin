#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "jellyfin/result.h"

namespace zerofin::jf {

struct DiscoveredServer {
  std::string id;
  std::string name;
  std::string address;  // e.g. "http://192.168.1.157:8096"
};

// Parses one UDP reply from a server on port 7359.
Result<DiscoveredServer> parse_discovery_reply(std::string_view json);

// Broadcasts "Who is JellyfinServer?" on UDP 7359 and collects replies for `timeout_ms`.
// Removes duplicates by server id. Blocks the calling thread. Returns an empty list on error.
std::vector<DiscoveredServer> discover_servers(int timeout_ms = 1500);

}  // namespace zerofin::jf
