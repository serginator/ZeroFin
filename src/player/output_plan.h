#pragma once

#include <optional>
#include <string_view>

#include "jellyfin/device_profile.h"
#include "player/backend.h"
#include "player/hdmi.h"

namespace zerofin::player {

// Decides where the video and the sound go.
//  - `connected`: the monitor found on the HDMI port, if any.
//  - `monitor_has_audio`: the monitor's data lists an audio format (it may still have no speakers).
//  - `sound_on_speaker`: the user chose the CP0 speaker for the sound.
BackendOptions plan_output(OutputSetting setting, const std::optional<HdmiConnector>& connected,
                           bool monitor_has_audio, bool sound_on_speaker);

// The server profile for an output. `quality` is "720" or "1080". It only matters for a monitor.
jf::Target target_for(Output output, std::string_view quality);

}  // namespace zerofin::player
