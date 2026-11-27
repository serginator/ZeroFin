#include <algorithm>

#include "screens/screens.h"
#include "storage/config_store.h"
#include "ui/widgets.h"

namespace zerofin::app {

std::string short_address(const std::string& address) {
  const auto scheme = address.find("://");
  return scheme == std::string::npos ? address : address.substr(scheme + 3);
}

namespace {

// The servers this device knows: the active one first, then the saved ones. Enter opens a server. Backspace or
// Delete removes it. "Add a server" opens the address screen.
class ServerListScreen : public Screen {
 public:
  explicit ServerListScreen(AppContext& ctx) : Screen(ctx, "Servers") {}

  void build() override {
    list_ = std::make_unique<ListView>(root_, ctx_.fonts, 0, kTitleH, kScreenW, 6);
    list_->set_detail_width(128);
    rebuild();
  }

  // Also runs when a screen above is popped, for example after a remove was confirmed.
  void on_show() override { rebuild(); }

  bool on_key(const ui::KeyEvent& e) override {
    if (list_->handle_key(e)) return true;
    const int s = list_->selected();
    if (s < 0 || static_cast<size_t>(s) >= actions_.size() || e.repeat) return false;
    const size_t row = static_cast<size_t>(s);
    if (e.key == ui::Key::Enter && actions_[row]) {
      actions_[row]();
      return true;
    }
    if ((e.key == ui::Key::Backspace || e.key == ui::Key::Delete) && !urls_[row].empty()) {
      confirm_remove(urls_[row], names_[row]);
      return true;
    }
    return false;
  }

 private:
  void rebuild() {
    const int keep = list_->selected();
    std::vector<ListRow> rows;
    actions_.clear();
    urls_.clear();
    names_.clear();
    auto add = [&](ListRow row, std::string url, std::string name, std::function<void()> action) {
      rows.push_back(std::move(row));
      urls_.push_back(std::move(url));
      names_.push_back(std::move(name));
      actions_.push_back(std::move(action));
    };
    auto header = [&](const std::string& text) {
      ListRow r;
      r.header = true;
      r.primary = text;
      add(std::move(r), "", "", nullptr);
    };
    auto server = [&](const std::string& url, const std::string& name, const std::string& detail) {
      ListRow r;
      r.primary = display_name(url, name);
      r.detail = detail;
      add(std::move(r), url, display_name(url, name), [this, url] { open(url); });
    };

    const auto& cfg = ctx_.config;
    if (!cfg.server_url.empty() || !cfg.servers.empty()) header("Your servers");
    if (!cfg.server_url.empty()) server(cfg.server_url, cfg.server_name, "current");
    for (const auto& s : cfg.servers) server(s.url, s.name, s.logged_in() ? s.user_name : "signed out");
    header("Other");
    ListRow add_row;
    add_row.primary = "Add a server";
    add(std::move(add_row), "", "", [this] { ctx_.nav->push(make_setup_screen(ctx_)); });
    list_->set_rows(std::move(rows));
    if (keep > 0) list_->select(static_cast<size_t>(std::min<int>(keep, static_cast<int>(actions_.size()) - 1)));
    set_status("Del: remove");
  }

  static std::string display_name(const std::string& url, const std::string& name) {
    return name.empty() ? short_address(url) : name;
  }

  // Opens a server with the login it has. A server without a login (or with a token that it rejects) goes
  // to Quick Connect.
  void open(const std::string& url) {
    ctx_.use_server(url, "");
    if (ctx_.config.logged_in()) {
      go_home(ctx_);
    } else {
      go_connect(ctx_);
    }
  }

  void confirm_remove(const std::string& url, const std::string& name) {
    MessageSpec spec;
    spec.title = "Remove server";
    spec.text = "Remove " + name + "? Its login is deleted from this device and signed out on the server.";
    spec.confirm_label = "remove";
    spec.on_confirm = [this, url] {
      const bool was_active = storage::same_server_address(ctx_.config.server_url, url);
      ctx_.forget_server(url);
      if (was_active) {
        go_servers(ctx_);  // this screen is gone after the call
      } else {
        ctx_.nav->pop();  // the confirm screen. This list rebuilds in on_show.
      }
    };
    ctx_.nav->push(make_message_screen(ctx_, std::move(spec)));
  }

  std::unique_ptr<ListView> list_;
  std::vector<std::function<void()>> actions_;
  std::vector<std::string> urls_;   // per row: the server address, empty for other rows
  std::vector<std::string> names_;  // per row: the name shown for it
};

}  // namespace

std::unique_ptr<Screen> make_server_list_screen(AppContext& ctx) { return std::make_unique<ServerListScreen>(ctx); }

}  // namespace zerofin::app
