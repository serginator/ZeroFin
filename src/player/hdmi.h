#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

namespace zerofin::player {

// Where the video goes. Lcd is the 320x170 panel. Hdmi is a monitor on the HDMI port.
enum class Output { Lcd, Hdmi };

// The user setting. Auto uses HDMI when a monitor is connected.
enum class OutputSetting { Auto, LcdOnly };

// The DRM connector of a monitor, in the form mpv wants it.
struct HdmiConnector {
  std::string device;     // "/dev/dri/card0"
  std::string connector;  // "HDMI-A-1"
};

// Looks for a connected "cardN-HDMI-A-M" connector under `drm_dir` (default /sys/class/drm).
std::optional<HdmiConnector> find_connected_hdmi(const std::filesystem::path& drm_dir = "/sys/class/drm");
bool hdmi_connected(const std::filesystem::path& drm_dir = "/sys/class/drm");

Output choose_output(OutputSetting setting, bool hdmi_is_connected);
OutputSetting output_setting_from_text(std::string_view text);  // unknown text = Auto
const char* output_setting_text(OutputSetting setting);

// True when the ELD (the monitor's audio data, from ALSA) lists at least one audio format.
// A monitor with no speakers can still list one, so this does not prove that sound is audible.
bool eld_has_audio(std::string_view eld_text);
// Reads every `card*/eld#*` file under `asound_dir` (default /proc/asound). True when one lists audio.
// True when there is no ELD file at all (unknown: assume the monitor plays sound).
bool monitor_has_audio(const std::filesystem::path& asound_dir = "/proc/asound");

}  // namespace zerofin::player
