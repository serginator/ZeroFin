#include "player/volume.h"

#include <algorithm>

namespace zerofin::player {

Volume::Volume(std::unique_ptr<SystemVolume> system, int software_level)
    : system_(std::move(system)), level_(std::clamp(software_level, 0, 100)) {
  if (!system_) return;
  const std::optional<int> current = system_->volume();
  if (!current) {  // the mixer cannot be read: do not trust it
    system_.reset();
    return;
  }
  level_ = *current;
  muted_ = system_->muted().value_or(false);
}

void Volume::change(int delta) {
  level_ = std::clamp(level_ + delta, 0, 100);
  muted_ = false;
  if (system_) {
    system_->set_muted(false);
    system_->set_volume(level_);
  }
}

void Volume::toggle_mute() {
  muted_ = !muted_;
  apply();
}

void Volume::apply() {
  if (!system_) return;
  // A device without a mute switch is muted by setting its volume to 0.
  if (!system_->set_muted(muted_)) system_->set_volume(muted_ ? 0 : level_);
}

int Volume::backend_percent() const {
  if (system_) return 100;
  return muted_ ? 0 : level_;
}

}  // namespace zerofin::player
