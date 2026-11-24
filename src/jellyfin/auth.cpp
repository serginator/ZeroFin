#include "jellyfin/auth.h"

#include <algorithm>
#include <random>

namespace zerofin::jf {
namespace {

std::string clean(std::string value) {
  value.erase(std::remove(value.begin(), value.end(), '"'), value.end());
  return value;
}

}  // namespace

std::string authorization_header_value(const ClientInfo& info, const std::string& token) {
  std::string out = "MediaBrowser Client=\"" + clean(info.client) + "\", Device=\"" + clean(info.device) +
                    "\", DeviceId=\"" + clean(info.device_id) + "\", Version=\"" + clean(info.version) + "\"";
  if (!token.empty()) out += ", Token=\"" + clean(token) + "\"";
  return out;
}

std::string random_hex_id() {
  static const char* kHex = "0123456789abcdef";
  std::random_device rd;
  std::mt19937_64 gen(rd());
  std::uniform_int_distribution<int> dist(0, 15);
  std::string id;
  for (int i = 0; i < 32; ++i) id.push_back(kHex[dist(gen)]);
  return id;
}

}  // namespace zerofin::jf
