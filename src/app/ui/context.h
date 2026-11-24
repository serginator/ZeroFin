#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "jellyfin/client.h"
#include "jellyfin/discovery.h"
#include "player/backend.h"
#include "storage/config_store.h"
#include "ui/async.h"
#include "ui/theme.h"

namespace zerofin::app {

class Navigator;
class Toast;

// Shared state and services for all screens. Owned by App.
struct AppContext {
  Async* async = nullptr;
  Navigator* nav = nullptr;
  Toast* toast = nullptr;
  Fonts fonts;

  storage::ConfigStore* store = nullptr;
  storage::AppConfig config;

  // Screens copy this pointer into async work. Only the worker thread calls it.
  std::shared_ptr<jf::Client> client;
  std::function<std::shared_ptr<jf::Client>(const std::string& server_url)> client_factory;
  std::function<std::vector<jf::DiscoveredServer>()> discover;

  // Makes the player backend. Always set. The backend it returns is null when the build has no player.
  std::function<std::unique_ptr<player::PlayerBackend>()> make_backend;
  bool demo = false;  // ZEROFIN_DEMO: fake server, fake player, fake status values
  int volume = 100;   // 0 to 100, kept while the app runs

  // Called when the user starts a title. Opens the player, or shows the plan when the build has no player.
  std::function<void(const jf::PlaybackPlan&, const jf::Item&)> start_playback;

  jf::PlayMode play_mode() const {
    return config.play_mode == "transcode" ? jf::PlayMode::AlwaysTranscode : jf::PlayMode::Auto;
  }

  // Writes the config file. Shows a toast on failure.
  bool save_config();
  // Points the app at a server. Drops any session.
  void use_server(const std::string& url, const std::string& name);
  // Stores a login in the config and in the client.
  void set_login(const jf::AuthResult& auth);
  // Forgets the login. Keeps the server address unless `forget_server` is true.
  void clear_login(bool forget_server);
};

}  // namespace zerofin::app
