#pragma once

#include <memory>

#include "player/system_volume.h"

namespace zerofin::player {

// The volume the player screen shows and changes. With a device mixer it sets the global volume of
// the device, so 100% here is 100% of the device. Without one (the simulator, or a device with no
// mixer control) it sets the volume of the player and keeps its own level.
class Volume {
 public:
  // `system` may be null. `software_level` is the start level when there is no device mixer.
  Volume(std::unique_ptr<SystemVolume> system, int software_level);

  bool uses_system() const { return system_ != nullptr; }
  int level() const { return level_; }  // 0 to 100, kept while muted
  bool muted() const { return muted_; }

  // Adds `delta` percent. Ends the mute.
  void change(int delta);
  void toggle_mute();

  // The volume the player backend must use. 100 when the device mixer does the work.
  int backend_percent() const;

 private:
  void apply();

  std::unique_ptr<SystemVolume> system_;
  int level_ = 100;
  bool muted_ = false;
};

}  // namespace zerofin::player
