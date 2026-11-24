#include "ui_core/status_info.h"

#include <cstdio>
#include <cstdlib>
#include <sstream>
#include <vector>

namespace zerofin::ui {
namespace {

std::string trim(const std::string& s) {
  const auto b = s.find_first_not_of(" \t\r\n");
  if (b == std::string::npos) return "";
  const auto e = s.find_last_not_of(" \t\r\n");
  return s.substr(b, e - b + 1);
}

// Parses the whole token as an integer. A trailing '.' is allowed (the kernel prints "-23.").
std::optional<int> parse_int_token(std::string token) {
  if (!token.empty() && token.back() == '.') token.pop_back();
  if (token.empty()) return std::nullopt;
  char* end = nullptr;
  const long v = std::strtol(token.c_str(), &end, 10);
  if (end == token.c_str() || *end != '\0') return std::nullopt;
  return static_cast<int>(v);
}

}  // namespace

std::optional<BatteryInfo> parse_battery(const std::string& capacity_text, const std::string& status_text) {
  const auto percent = parse_int_token(trim(capacity_text));
  if (!percent || *percent < 0 || *percent > 100) return std::nullopt;
  BatteryInfo info;
  info.percent = *percent;
  info.charging = trim(status_text) == "Charging";
  return info;
}

std::optional<int> parse_wireless_dbm(const std::string& proc_net_wireless) {
  std::stringstream lines(proc_net_wireless);
  std::string line;
  while (std::getline(lines, line)) {
    if (line.find('|') != std::string::npos) continue;  // the two header lines
    const auto colon = line.find(':');
    if (colon == std::string::npos) continue;
    std::stringstream fields(line.substr(colon + 1));
    std::vector<std::string> tokens;
    std::string t;
    while (fields >> t) tokens.push_back(t);
    // status, link quality, level, noise
    if (tokens.size() < 3) return std::nullopt;
    return parse_int_token(tokens[2]);
  }
  return std::nullopt;
}

int signal_bars(int dbm) {
  if (dbm >= -55) return 4;
  if (dbm >= -65) return 3;
  if (dbm >= -75) return 2;
  if (dbm >= -85) return 1;
  return 0;
}

std::string format_clock(int hour, int minute) {
  char buf[8];
  std::snprintf(buf, sizeof(buf), "%02d:%02d", hour, minute);
  return buf;
}

}  // namespace zerofin::ui
