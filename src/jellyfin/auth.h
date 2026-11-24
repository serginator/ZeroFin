#pragma once

#include <string>

namespace zerofin::jf {

struct ClientInfo {
  std::string client = "ZeroFin";
  std::string device = "Cardputer Zero";
  std::string device_id;  // stable, random, stored in the config
  std::string version = "0.1.0";
};

// Value of the "Authorization" header: MediaBrowser Client="...", Device="...", ...
// Adds Token="..." when `token` is not empty. Removes double quotes from all values.
std::string authorization_header_value(const ClientInfo& info, const std::string& token);

// Makes a random 32-character hex id for ClientInfo::device_id and for play sessions.
std::string random_hex_id();

}  // namespace zerofin::jf
