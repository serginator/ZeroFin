#pragma once

#include <optional>
#include <string>

namespace zerofin::ui {

struct BatteryInfo {
  int percent = 0;
  bool charging = false;
};

// `capacity_text` and `status_text` are the contents of the sysfs files `capacity` and
// `status` of a battery. Null when the percent is missing or outside 0 to 100.
// `charging` is true only for the status "Charging".
std::optional<BatteryInfo> parse_battery(const std::string& capacity_text, const std::string& status_text);

// The signal level in dBm of the first interface in the text of /proc/net/wireless.
// Null when there is no usable interface line.
std::optional<int> parse_wireless_dbm(const std::string& proc_net_wireless);

// 0 to 4 bars: >= -55 is 4, >= -65 is 3, >= -75 is 2, >= -85 is 1, else 0.
int signal_bars(int dbm);

// "20:29". Zero padded.
std::string format_clock(int hour, int minute);

}  // namespace zerofin::ui
