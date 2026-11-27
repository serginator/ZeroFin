#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "jellyfin/client.h"
#include "jellyfin/discovery.h"
#include "player/backend.h"
#include "player/output_plan.h"
#include "player/system_volume.h"
#include "storage/config_store.h"
#include "ui/async.h"
#include "ui/theme.h"

namespace zerofin::app {

class ImageLoader;
class Navigator;
class Toast;

// Shared state and services for all screens. Owned by App.
struct AppContext {
  Async* async = nullptr;
  Navigator* nav = nullptr;
  Toast* toast = nullptr;
  ImageLoader* images = nullptr;  // poster downloads. Null in tests.
  const lv_image_dsc_t* background = nullptr;  // the picture behind the screens. Null: a plain colour.
  Fonts fonts;

  storage::ConfigStore* store = nullptr;
  storage::AppConfig config;

  // Screens copy this pointer into async work. Only the worker thread calls it.
  std::shared_ptr<jf::Client> client;
  std::function<std::shared_ptr<jf::Client>(const std::string& server_url)> client_factory;
  std::function<std::vector<jf::DiscoveredServer>()> discover;

  // Makes the player backend. Always set. The backend it returns is null when the build has no player.
  std::function<std::unique_ptr<player::PlayerBackend>(const player::BackendOptions&)> make_backend;
  // The monitor on the HDMI port, if any. Null (the tests) means no monitor.
  std::function<std::optional<player::HdmiConnector>()> find_hdmi;
  bool demo = false;  // ZEROFIN_DEMO: fake server, fake player, fake status values
  // Opens the device mixer. Null in the demo, so the demo never changes the real volume. The
  // mixer may be unavailable: the factory then returns nullptr.
  std::function<std::unique_ptr<player::SystemVolume>()> make_system_volume;
  int volume = 100;   // 0 to 100. The player volume when there is no device mixer, kept while the app runs

  // Called when the user starts a title. Opens the player, or shows the plan when the build has no player.
  std::function<void(const jf::PlaybackPlan&, const jf::Item&)> start_playback;

  // Where the next video goes: the LCD, or the monitor when one is connected (unless Settings says LCD only).
  // ZEROFIN_OUTPUT=lcd|auto|hdmi overrides the setting. "hdmi" works with no monitor, for the simulator.
  player::BackendOptions output_plan() const;
  // True when a monitor is on the HDMI port (or ZEROFIN_OUTPUT=hdmi). Then the player menu offers the output choice.
  bool monitor_present() const;
  // The server profile for that output.
  jf::Target playback_target() const;

  jf::PlayMode play_mode() const {
    return config.play_mode == "transcode" ? jf::PlayMode::AlwaysTranscode : jf::PlayMode::Auto;
  }

  // Writes the config file. Shows a toast on failure.
  bool save_config();
  // Points the app at a server. The login stays when the server is the active one or a saved one. A new
  // address starts signed out. The server that was active is kept in the saved list.
  void use_server(const std::string& url, const std::string& name);
  // Removes a server and its login from this device: the active one or a saved one. The token is revoked on
  // the server first, best effort. Removing the active server leaves the app with no active server.
  void forget_server(const std::string& url);
  // Stores a login in the config and in the client.
  void set_login(const jf::AuthResult& auth);
  // Forgets the login of the active server. Keeps its address.
  void clear_login();
};

}  // namespace zerofin::app
