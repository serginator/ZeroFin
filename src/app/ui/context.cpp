#include "ui/context.h"

#include "ui/screen.h"
#include "ui/widgets.h"

namespace zerofin::app {

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
