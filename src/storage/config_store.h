#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "jellyfin/result.h"

namespace zerofin::storage {

// A server the user used before, with the login it had. The active server is not in the list.
struct SavedServer {
  std::string url;
  std::string name;
  std::string token;  // empty when the user logged out of this server
  std::string user_id;
  std::string user_name;

  bool logged_in() const { return !token.empty() && !user_id.empty(); }
};

struct AppConfig {
  // The active server and its login. The other saved servers are in `servers`.
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
  std::vector<SavedServer> servers;     // the servers that are not active, most recently used first

  bool logged_in() const { return !server_url.empty() && !token.empty() && !user_id.empty(); }
};

// True when both addresses point at the same server: a trailing slash and the case of the text are ignored.
// Two empty addresses are not the same server.
bool same_server_address(const std::string& a, const std::string& b);

// Moves the active server (when it has an address) and its login to the front of `servers`. Clears the active
// fields. A list entry with the same address is replaced.
void stash_active_server(AppConfig& config);

// Makes the saved server at `url` the active one. The old active server goes to the front of the list.
// Returns false, and changes nothing, when `url` is not in the list.
bool activate_saved_server(AppConfig& config, const std::string& url);

// Makes the server at `url` the active one, with the name the server gave. The login stays when `url` is the
// active server or a saved one. A new address starts signed out. The old active server is kept in the list.
void select_server(AppConfig& config, const std::string& url, const std::string& name);

// Removes the saved server at `url` from the list. Returns whether it was there.
bool remove_saved_server(AppConfig& config, const std::string& url);

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
