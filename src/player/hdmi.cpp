#include "player/hdmi.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <system_error>

namespace zerofin::player {
namespace fs = std::filesystem;

namespace {

std::string read_text(const fs::path& path) {
  std::ifstream in(path);
  std::stringstream ss;
  ss << in.rdbuf();
  return ss.str();
}

std::string trimmed(std::string s) {
  while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back()))) s.pop_back();
  while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front()))) s.erase(s.begin());
  return s;
}

std::string lower(std::string_view s) {
  std::string out(s);
  std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c) { return std::tolower(c); });
  return out;
}

}  // namespace

std::optional<HdmiConnector> find_connected_hdmi(const fs::path& drm_dir) {
  std::error_code ec;
  for (fs::directory_iterator it(drm_dir, ec), end; !ec && it != end; it.increment(ec)) {
    const std::string name = it->path().filename().string();  // "card0-HDMI-A-1"
    const size_t dash = name.find('-');
    if (name.rfind("card", 0) != 0 || dash == std::string::npos) continue;
    const std::string connector = name.substr(dash + 1);
    if (connector.rfind("HDMI-A-", 0) != 0) continue;
    if (trimmed(read_text(it->path() / "status")) != "connected") continue;
    return HdmiConnector{"/dev/dri/" + name.substr(0, dash), connector};
  }
  return std::nullopt;
}

bool hdmi_connected(const fs::path& drm_dir) { return find_connected_hdmi(drm_dir).has_value(); }

Output choose_output(OutputSetting setting, bool hdmi_is_connected) {
  return setting == OutputSetting::Auto && hdmi_is_connected ? Output::Hdmi : Output::Lcd;
}

OutputSetting output_setting_from_text(std::string_view text) {
  return lower(text) == "lcd" ? OutputSetting::LcdOnly : OutputSetting::Auto;
}

const char* output_setting_text(OutputSetting setting) { return setting == OutputSetting::LcdOnly ? "lcd" : "auto"; }

bool eld_has_audio(std::string_view eld_text) {
  std::istringstream in{std::string(eld_text)};
  std::string line;
  while (std::getline(in, line)) {
    if (line.rfind("sad_count", 0) != 0) continue;
    const std::string value = trimmed(line.substr(std::string("sad_count").size()));
    return !value.empty() && std::atoi(value.c_str()) > 0;
  }
  return false;
}

bool monitor_has_audio(const fs::path& asound_dir) {
  bool found_eld = false;
  std::error_code ec;
  for (fs::directory_iterator card(asound_dir, ec), end; !ec && card != end; card.increment(ec)) {
    if (card->path().filename().string().rfind("card", 0) != 0) continue;
    std::error_code ec2;
    for (fs::directory_iterator f(card->path(), ec2), end2; !ec2 && f != end2; f.increment(ec2)) {
      if (f->path().filename().string().rfind("eld#", 0) != 0) continue;
      found_eld = true;
      if (eld_has_audio(read_text(f->path()))) return true;
    }
  }
  return !found_eld;
}

}  // namespace zerofin::player
