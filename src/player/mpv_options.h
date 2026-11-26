#pragma once

#include <string>
#include <utility>
#include <vector>

#include "player/backend.h"

namespace zerofin::player {

using MpvOptions = std::vector<std::pair<std::string, std::string>>;

// The mpv name of a sound device. "auto" is the system default.
std::string audio_device_name(AudioSink sink);

// The options set on the libmpv handle before it starts. Pure data, so the unit tests can read it.
// Lcd: libmpv draws into the app (render API) and scales to 320x170. Hdmi: mpv opens its own GPU output on the
// DRM connector of the monitor.
MpvOptions mpv_options(const BackendOptions& options);

}  // namespace zerofin::player
