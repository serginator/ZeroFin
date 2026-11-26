#pragma once

#include <memory>
#include <optional>

namespace zerofin::player {

// The volume of the device (the global volume, the one the OS and other apps share). On the CP0 it
// is the PipeWire output volume, reached through the ALSA "default" mixer.
class SystemVolume {
 public:
  virtual ~SystemVolume() = default;

  // 0 to 100. Empty when the device volume cannot be read.
  virtual std::optional<int> volume() = 0;
  virtual bool set_volume(int percent) = 0;
  // Empty when the device has no mute switch.
  virtual std::optional<bool> muted() = 0;
  virtual bool set_muted(bool muted) = 0;
};

// Converts between a percentage and the raw range of a mixer control. Both round to the nearest step.
// They are free functions, so the unit tests can check them without a sound card.
long percent_to_raw(int percent, long raw_min, long raw_max);
int raw_to_percent(long raw, long raw_min, long raw_max);

// The ALSA mixer of the device, or nullptr when the build has no ALSA or no usable mixer control.
std::unique_ptr<SystemVolume> make_system_volume();

}  // namespace zerofin::player
