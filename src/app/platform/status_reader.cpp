#include "platform/status_reader.h"

#include <ctime>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <system_error>

namespace zerofin::platform {
namespace {

std::string read_file(const std::filesystem::path& path) {
  std::ifstream in(path);
  if (!in) return "";
  std::stringstream ss;
  ss << in.rdbuf();
  return ss.str();
}

std::optional<ui::BatteryInfo> read_battery() {
  namespace fs = std::filesystem;
  std::error_code ec;
  for (const auto& entry : fs::directory_iterator("/sys/class/power_supply", ec)) {
    std::string type = read_file(entry.path() / "type");
    while (!type.empty() && (type.back() == '\n' || type.back() == '\r' || type.back() == ' ')) type.pop_back();
    if (type != "Battery") continue;
    if (auto info = ui::parse_battery(read_file(entry.path() / "capacity"), read_file(entry.path() / "status"))) {
      return info;
    }
  }
  return std::nullopt;
}

}  // namespace

StatusSnapshot read_status(bool demo) {
  StatusSnapshot s;
  const std::time_t now = std::time(nullptr);
  std::tm local{};
  localtime_r(&now, &local);
  s.hour = local.tm_hour;
  s.minute = local.tm_min;
  if (demo) {
    s.battery = ui::BatteryInfo{67, false};
    s.dbm = -45;
    return s;
  }
  s.battery = read_battery();
  s.dbm = ui::parse_wireless_dbm(read_file("/proc/net/wireless"));
  return s;
}

}  // namespace zerofin::platform
