#include "ui/context.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <string>

#include "ui/screen.h"
#include "ui/widgets.h"

namespace zerofin::app {

player::BackendOptions AppContext::output_plan() const {
  std::string forced = std::getenv("ZEROFIN_OUTPUT") ? std::getenv("ZEROFIN_OUTPUT") : "";
  std::transform(forced.begin(), forced.end(), forced.begin(), [](unsigned char ch) { return std::tolower(ch); });
  std::optional<player::HdmiConnector> monitor = find_hdmi ? find_hdmi() : std::nullopt;
  player::OutputSetting setting = player::output_setting_from_text(forced.empty() ? config.video_output : forced);
  if (forced == "hdmi") {
    setting = player::OutputSetting::Auto;
    if (!monitor) monitor = player::HdmiConnector{"/dev/dri/card0", "HDMI-A-1"};
  }
  return player::plan_output(setting, monitor, monitor && player::monitor_has_audio(), config.hdmi_sound == "speaker");
}

bool AppContext::monitor_present() const {
  const char* forced = std::getenv("ZEROFIN_OUTPUT");
  if (forced && std::string(forced) == "hdmi") return true;
  return find_hdmi && find_hdmi().has_value();
}

jf::Target AppContext::playback_target() const {
  return player::target_for(output_plan().output, config.hdmi_quality);
}

bool AppContext::save_config() {
  if (!store) return true;
  auto r = store->save(config);
  if (!r && toast) toast->show("Cannot save settings: " + r.error().message, Toast::Kind::Error, 4000);
  return r.ok();
}

void AppContext::use_server(const std::string& url, const std::string& name) {
  config.server_url = url;
  config.server_name = name;
  config.token.clear();
  config.user_id.clear();
  config.user_name.clear();
  client = client_factory ? client_factory(url) : nullptr;
  save_config();
}

void AppContext::set_login(const jf::AuthResult& auth) {
  config.token = auth.access_token;
  config.user_id = auth.user.id;
  config.user_name = auth.user.name;
  save_config();
}

void AppContext::clear_login(bool forget_server) {
  config.token.clear();
  config.user_id.clear();
  config.user_name.clear();
  if (forget_server) {
    config.server_url.clear();
    config.server_name.clear();
    client.reset();
  } else if (client_factory) {
    client = client_factory(config.server_url);
  }
  save_config();
}

}  // namespace zerofin::app
