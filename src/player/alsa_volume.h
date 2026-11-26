#pragma once

#include <memory>

#include "player/system_volume.h"

namespace zerofin::player {

// Opens the first ALSA mixer that has an output volume control. Tries "default" first (PipeWire
// and PulseAudio answer there), then each sound card. Returns nullptr when none has one.
std::unique_ptr<SystemVolume> open_alsa_volume();

}  // namespace zerofin::player
