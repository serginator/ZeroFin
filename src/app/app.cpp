#include "app.h"

#include <unistd.h>

#include <atomic>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <memory>
#include <sstream>

#include "jellyfin/auth.h"
#include "jellyfin/client.h"
#include "jellyfin/curl_cli_http.h"
#include "jellyfin/demo_http.h"
#include "jellyfin/discovery.h"
#include "platform/platform.h"
#include "player/backend.h"
#include "screens/screens.h"
#include "script_runner.h"
#include "storage/config_store.h"
#include "ui/async.h"
#include "ui/context.h"
#include "ui/screen.h"
#include "ui/widgets.h"

#if ZEROFIN_HAS_LIBCURL
#include "jellyfin/curl_http.h"
#endif

namespace zerofin::app {
namespace {

std::atomic<bool> g_quit{false};

void on_signal(int) { g_quit = true; }

bool env_set(const char* name) {
  const char* v = std::getenv(name);
  return v && *v && std::string(v) != "0";
}

// Which HTTP transport to use. The simulator on macOS cannot use libcurl on the LAN, because
// macOS blocks locally built programs. It runs /usr/bin/curl instead.
std::unique_ptr<jf::HttpClient> make_transport(bool& demo) {
  demo = env_set("ZEROFIN_DEMO");
  if (demo) return std::make_unique<jf::DemoHttpClient>();
  const char* choice = std::getenv("ZEROFIN_TRANSPORT");
  const std::string t = choice ? choice : "";
  if (t == "cli") return std::make_unique<jf::CurlCliHttpClient>();
#if ZEROFIN_HAS_LIBCURL
  if (!(t.empty() && ZEROFIN_PREFER_CLI_CURL)) return std::make_unique<jf::CurlHttpClient>();
#endif
  return std::make_unique<jf::CurlCliHttpClient>();
}

jf::PlaybackProfile load_profile(const std::filesystem::path& dir) {
  std::ifstream in(dir / "profile.json");
  if (!in) return {};
  std::stringstream ss;
  ss << in.rdbuf();
  auto r = jf::playback_profile_from_json(ss.str());
  if (!r) {
    std::fprintf(stderr, "ignoring profile.json: %s\n", r.error().message.c_str());
    return {};
  }
  return r.value();
}

}  // namespace

int run_app() {
  std::signal(SIGINT, on_signal);
  std::signal(SIGTERM, on_signal);

  lv_init();
  lv_tick_set_cb(platform::tick_ms);
  auto plat = platform::create_platform();
  if (!plat || !plat->display()) {
    std::fprintf(stderr, "no display\n");
    return 1;
  }

  lv_obj_t* screen = lv_screen_active();
  lv_obj_set_style_bg_color(screen, lv_color_black(), 0);
  lv_obj_remove_flag(screen, LV_OBJ_FLAG_SCROLLABLE);

  AppContext ctx;
  ctx.fonts = load_fonts();

  // Settings.
  storage::ConfigStore store(storage::default_config_dir());
  ctx.store = &store;
  bool config_damaged = false;
  if (auto loaded = store.load()) {
    ctx.config = loaded.value();
  } else {
    std::fprintf(stderr, "config: %s\n", loaded.error().message.c_str());
    config_damaged = true;
  }
  if (ctx.config.device_id.empty()) {
    ctx.config.device_id = jf::random_hex_id();
    ctx.save_config();
  }

  // Network.
  bool demo = false;
  std::unique_ptr<jf::HttpClient> http = make_transport(demo);
  jf::ClientInfo info;
  info.device_id = ctx.config.device_id;
  info.version = ZEROFIN_VERSION_STRING;
  const jf::PlaybackProfile profile = load_profile(store.dir());

  Async async;
  Toast toast(lv_layer_top(), ctx.fonts);
  Navigator nav(screen);
  ctx.async = &async;
  ctx.nav = &nav;
  ctx.toast = &toast;
  ctx.client_factory = [&](const std::string& url) {
    auto c = std::make_shared<jf::Client>(*http, info, url);
    c->set_profile(profile);
    return c;
  };
  ctx.discover = [demo] {
    if (demo) return std::vector<jf::DiscoveredServer>{{"demo", "Demo Server", "http://demo.local:8096"}};
    return jf::discover_servers(1500);
  };
  ctx.demo = demo;
  ctx.make_backend = [demo] { return player::make_backend(demo); };
  ctx.start_playback = [&ctx](const jf::PlaybackPlan& plan, const jf::Item& item) {
    // A build without libmpv has no backend (and no demo flag): show the plan instead.
    if (!ctx.make_backend || !ctx.make_backend()) {
      show_playback_plan(ctx, plan, item);
      return;
    }
    ctx.nav->push(make_player_screen(ctx, plan, item));
  };

  // First screen: home when signed in, the sign-in screen when only the server is known.
  if (!ctx.config.server_url.empty()) {
    ctx.client = ctx.client_factory(ctx.config.server_url);
    if (ctx.config.logged_in()) ctx.client->set_session({ctx.config.token, ctx.config.user_id});
  }
  if (ctx.config.logged_in()) {
    nav.reset(make_home_screen(ctx));
  } else if (!ctx.config.server_url.empty()) {
    nav.reset(make_connect_screen(ctx));
  } else {
    nav.reset(make_setup_screen(ctx));
  }
  nav.process_pending();
  if (config_damaged) toast.show("Settings file was damaged. Starting fresh.", Toast::Kind::Warning, 4000);

  std::unique_ptr<ScriptRunner> script;
  if (const char* s = std::getenv("ZEROFIN_SCRIPT")) {
    const char* dir = std::getenv("ZEROFIN_SHOT_DIR");
    script = std::make_unique<ScriptRunner>(s, dir ? dir : ".");
  }

  std::vector<ui::KeyEvent> keys;
  while (!g_quit) {
    keys.clear();
    if (!plat->poll(keys)) break;
    if (script && !script->tick(platform::tick_ms(), async, *plat, keys)) break;
    for (const auto& k : keys) nav.handle_key(k);
    async.pump();
    nav.process_pending();
    lv_timer_handler();
    usleep(5000);
  }
  return 0;
}

}  // namespace zerofin::app
