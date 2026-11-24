#pragma once

#include <optional>

#include "ui_core/status_info.h"

namespace zerofin::platform {

// What the top-right corner of the player shows.
struct StatusSnapshot {
  std::optional<ui::BatteryInfo> battery;  // none when the system has no battery file
  std::optional<int> dbm;                  // none when there is no Wi-Fi link
  int hour = 0;
  int minute = 0;
};

// Reads the battery (sysfs), the Wi-Fi level (/proc/net/wireless) and the local time.
// With `demo` true it returns fixed battery and Wi-Fi values and the real time.
StatusSnapshot read_status(bool demo);

}  // namespace zerofin::platform
