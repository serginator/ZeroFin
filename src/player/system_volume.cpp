#include "player/system_volume.h"

#include <algorithm>

#if defined(ZEROFIN_HAS_ALSA)
#include "player/alsa_volume.h"
#endif

namespace zerofin::player {

long percent_to_raw(int percent, long raw_min, long raw_max) {
  const long span = raw_max - raw_min;
  if (span <= 0) return raw_min;
  const long p = std::clamp(percent, 0, 100);
  return raw_min + (p * span + 50) / 100;
}

int raw_to_percent(long raw, long raw_min, long raw_max) {
  const long span = raw_max - raw_min;
  if (span <= 0) return 0;
  const long r = std::clamp(raw, raw_min, raw_max) - raw_min;
  return static_cast<int>((r * 100 + span / 2) / span);
}

std::unique_ptr<SystemVolume> make_system_volume() {
#if defined(ZEROFIN_HAS_ALSA)
  return open_alsa_volume();
#else
  return nullptr;
#endif
}

}  // namespace zerofin::player
