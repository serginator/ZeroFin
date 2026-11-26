#pragma once

#include <filesystem>
#include <string>

#include "jellyfin/result.h"

namespace zerofin::storage {

struct AppConfig {
  std::string server_url;
  std::string server_name;
  std::string token;       // access token. The password is never stored.
  std::string user_id;
  std::string user_name;
  std::string device_id;   // random, made on first run
  std::string play_mode = "auto";  // "auto" or "transcode"
  std::string video_output = "auto";    // "auto" (HDMI when a monitor is connected) or "lcd"
  std::string hdmi_quality = "720";     // "720" or "1080": the transcode size for a monitor
  std::string hdmi_sound = "monitor";   // "monitor" or "speaker": where the sound goes with a monitor

  bool logged_in() const { return !server_url.empty() && !token.empty() && !user_id.empty(); }
};

// $ZEROFIN_HOME if set, else $HOME/.config/ZeroFin.
std::filesystem::path default_config_dir();

class ConfigStore {
 public:
  explicit ConfigStore(std::filesystem::path dir) : dir_(std::move(dir)) {}

  const std::filesystem::path& dir() const { return dir_; }
  std::filesystem::path file_path() const { return dir_ / "config.json"; }

  // A missing file gives the default config. A damaged file gives a Parse error.
  jf::Result<AppConfig> load() const;

  // Writes atomically (temp file, then rename). The file mode is 0600 and the folder 0700.
  jf::Result<jf::Unit> save(const AppConfig& config) const;

 private:
  std::filesystem::path dir_;
};

}  // namespace zerofin::storage
