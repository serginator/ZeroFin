#include <algorithm>

#include "jellyfin/url.h"
#include "screens/screens.h"
#include "ui/widgets.h"

namespace zerofin::app {
namespace {

// Stores the login, greets the user and opens the home screen.
void finish_login(AppContext& ctx, const jf::AuthResult& auth) {
  ctx.set_login(auth);
  ctx.toast->show("Signed in as " + auth.user.name, Toast::Kind::Success);
  go_home(ctx);
}

// ---------------------------------------------------------------- server list

class SetupScreen : public Screen {
 public:
  explicit SetupScreen(AppContext& ctx) : Screen(ctx, "Connect to a server") {}

  void build() override {
    list_ = std::make_unique<ListView>(root_, ctx_.fonts, 0, kTitleH, kScreenW, 6);
    list_->set_detail_width(128);
    rebuild();
  }

  void on_show() override {
    if (!scanned_) {
      scanned_ = true;
      scan();
    }
  }

  bool on_key(const ui::KeyEvent& e) override {
    if (list_->handle_key(e)) return true;
    if (e.key == ui::Key::Enter) {
      const int s = list_->selected();
      if (s >= 0 && static_cast<size_t>(s) < actions_.size() && actions_[static_cast<size_t>(s)]) {
        actions_[static_cast<size_t>(s)]();
      }
      return true;
    }
    if (e.key == ui::Key::Char && (e.ch == 'r' || e.ch == 'R')) {
      scan();
      return true;
    }
    return false;
  }

 private:
  void scan() {
    searching_ = true;
    rebuild();
    auto discover = ctx_.discover;
    async([discover] { return discover ? discover() : std::vector<jf::DiscoveredServer>{}; },
          [this](std::vector<jf::DiscoveredServer> found) {
            servers_ = std::move(found);
            searching_ = false;
            rebuild();
          });
  }

  void rebuild() {
    std::vector<ListRow> rows;
    actions_.clear();
    auto header = [&](const std::string& text) {
      ListRow r;
      r.header = true;
      r.primary = text;
      rows.push_back(std::move(r));
      actions_.emplace_back();
    };
    auto item = [&](const std::string& primary, const std::string& detail, std::function<void()> action) {
      ListRow r;
      r.primary = primary;
      r.detail = detail;
      rows.push_back(std::move(r));
      actions_.push_back(std::move(action));
    };

    header(searching_ ? "Looking for servers..." : (servers_.empty() ? "No server found on this network" : "Servers found"));
    for (const auto& s : servers_) {
      item(s.name.empty() ? short_address(s.address) : s.name, short_address(s.address), [this, s] { choose(s); });
    }
    header("Other");
    item("Enter an address", "", [this] { ask_address(); });
    item("Search again", "", [this] { scan(); });
    list_->set_rows(std::move(rows));
  }

  void choose(const jf::DiscoveredServer& server) {
    ctx_.use_server(server.address, server.name);
    sign_in_or_open();
  }

  // A server that is already saved with a login opens at once. Any other needs Quick Connect.
  void sign_in_or_open() {
    if (ctx_.config.logged_in()) {
      go_home(ctx_);
    } else {
      ctx_.nav->push(make_connect_screen(ctx_));
    }
  }

  void ask_address() {
    TextInputSpec spec;
    spec.title = "Server address";
    spec.prompt = "Address of your Jellyfin server";
    spec.placeholder = "IP or name. Port and http:// are optional";
    spec.on_submit = [this](const std::string& text) {
      auto urls = jf::server_url_candidates(text);
      if (urls.empty()) {
        ctx_.toast->show("That address is not valid", Toast::Kind::Error);
        return false;
      }
      check_server(std::move(urls));
      return true;
    };
    ctx_.nav->push(make_text_input_screen(ctx_, std::move(spec)));
  }

  // Asks the addresses one after the other and saves the first one that answers. Nothing is saved
  // before an answer, so a mistyped address does not replace the working server (and its login).
  void check_server(std::vector<std::string> urls) {
    if (checking_) return;
    checking_ = true;
    try_server(std::make_shared<const std::vector<std::string>>(std::move(urls)), 0, std::nullopt);
  }

  void try_server(std::shared_ptr<const std::vector<std::string>> urls, size_t i, std::optional<jf::Error> first_error) {
    auto client = ctx_.client_factory ? ctx_.client_factory((*urls)[i]) : nullptr;
    if (!client) {
      checking_ = false;
      set_status("");
      ctx_.toast->show("Cannot connect to a server", Toast::Kind::Error, 3500);
      return;
    }
    const bool several = urls->size() > 1;
    set_status(several ? "Trying " + std::to_string(i + 1) + "/" + std::to_string(urls->size()) + "..." : "Checking...");
    // A short wait per address when there are several, so a wrong one does not hold the others back.
    const int timeout_ms = several ? kProbeTimeoutMs : 0;
    async([client, timeout_ms] { return client->public_info(timeout_ms); },
          [this, urls, i, first_error](jf::Result<jf::ServerInfo> r) mutable {
            if (r) {
              checking_ = false;
              set_status("");
              ctx_.use_server((*urls)[i], r.value().name);
              sign_in_or_open();
              return;
            }
            if (!first_error) first_error = r.error();
            if (i + 1 < urls->size()) {
              try_server(urls, i + 1, std::move(first_error));
              return;
            }
            checking_ = false;
            set_status("");
            ctx_.toast->show(urls->size() > 1 ? "No Jellyfin server answered at that address" : error_text(*first_error),
                             Toast::Kind::Error, 3500);
          });
  }

  std::unique_ptr<ListView> list_;
  std::vector<jf::DiscoveredServer> servers_;
  std::vector<std::function<void()>> actions_;
  bool searching_ = false;
  bool scanned_ = false;
  bool checking_ = false;  // addresses are being tried
  static constexpr int kProbeTimeoutMs = 3500;
};

// -------------------------------------------------------------- Quick Connect

class ConnectScreen : public Screen {
 public:
  explicit ConnectScreen(AppContext& ctx) : Screen(ctx, "Sign in") {}

  void build() override {
    const std::string server = ctx_.config.server_name.empty() ? short_address(ctx_.config.server_url)
                                                                  : ctx_.config.server_name;
    lv_obj_t* server_label = make_label(root_, ctx_.fonts.bold, color::kText, 8, 25, 304);
    lv_label_set_text(server_label, server.c_str());
    lv_obj_t* help = make_label(root_, ctx_.fonts.small, color::kDim, 8, 44, 304, LV_LABEL_LONG_MODE_WRAP);
    lv_label_set_text(help, "On another device open Jellyfin, go to\nSettings > Quick Connect and type this code:");
    code_ = make_label(root_, ctx_.fonts.big, color::kAccent, 0, 76, kScreenW, LV_LABEL_LONG_MODE_CLIP);
    lv_obj_set_style_text_align(code_, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(code_, "...");
    info_ = make_label(root_, ctx_.fonts.small, color::kDim, 8, 124, 304);
    hint_ = make_label(root_, ctx_.fonts.small, color::kDim, 8, 148, 304);
    lv_label_set_text(hint_, kHintBack);
  }

  void on_show() override {
    // The first screen after "Log out" has nothing below it. Esc then opens the server list.
    lv_label_set_text(hint_, ctx_.nav->depth() > 1 ? kHintBack : kHintServer);
    if (!started_) {
      started_ = true;
      start();
    }
  }

  bool on_key(const ui::KeyEvent& e) override {
    if (e.key == ui::Key::Tab) {
      ctx_.nav->push(make_password_login_screen(ctx_));
      return true;
    }
    if (e.key == ui::Key::Enter && !e.repeat) {
      start();
      return true;
    }
    if (e.key == ui::Key::Esc && !e.repeat && ctx_.nav->depth() <= 1) {
      go_servers(ctx_);  // keeps the saved servers until another one is chosen
      return true;
    }
    return false;
  }

 private:
  static constexpr const char* kHintBack = "Tab: password   Enter: new code   Esc: back";
  static constexpr const char* kHintServer = "Tab: password   Enter: new code   Esc: server";

  void say(const std::string& text, uint32_t c = color::kDim) {
    lv_label_set_text(info_, text.c_str());
    lv_obj_set_style_text_color(info_, lv_color_hex(c), 0);
  }

  // `notice` (an error from the last try) stays on the screen next to the new code.
  void start(const std::string& notice = {}) {
    if (starting_ || signing_in_) return;
    starting_ = true;
    secret_.clear();
    lv_label_set_text(code_, "...");
    say(notice.empty() ? "Asking the server for a code..." : notice, notice.empty() ? color::kDim : color::kError);
    auto client = ctx_.client;
    async([client] { return client->quick_connect_initiate(); }, [this, notice](jf::Result<jf::QuickConnectSession> r) {
      starting_ = false;
      if (!r) {
        say(error_text(r.error()) + ". Enter: try again", color::kError);
        return;
      }
      secret_ = r.value().secret;
      std::string code = r.value().code;
      if (code.size() == 6) code.insert(3, " ");
      lv_label_set_text(code_, code.c_str());
      if (notice.empty()) say("Waiting for you to approve it...");
      if (!timer_) timer_ = add_timer(2000, [this] { poll(); });
    });
  }

  void poll() {
    if (in_flight_ || secret_.empty() || signing_in_ || starting_) return;
    in_flight_ = true;
    auto client = ctx_.client;
    auto secret = secret_;
    async([client, secret] { return client->quick_connect_authorized(secret); }, [this, secret](jf::Result<bool> r) {
      in_flight_ = false;
      if (secret != secret_) return;  // the user asked for a new code meanwhile: this answer is old
      if (!r) {
        if (r.error().kind == jf::ErrorKind::Network) {
          say("No connection. Trying again...", color::kWarn);
        } else {
          start();  // the code expired
        }
        return;
      }
      if (r.value()) sign_in(secret);
    });
  }

  void sign_in(const std::string& secret) {
    signing_in_ = true;
    say("Signing in...");
    auto client = ctx_.client;
    async([client, secret] { return client->login_quick_connect(secret); }, [this](jf::Result<jf::AuthResult> r) {
      signing_in_ = false;
      if (!r) {
        start(error_text(r.error()) + ". New code:");
        return;
      }
      finish_login(ctx_, r.value());
    });
  }

  lv_obj_t* code_ = nullptr;
  lv_obj_t* info_ = nullptr;
  lv_obj_t* hint_ = nullptr;
  lv_timer_t* timer_ = nullptr;
  std::string secret_;
  bool started_ = false;
  bool starting_ = false;
  bool in_flight_ = false;
  bool signing_in_ = false;
};

// ------------------------------------------------------------ password login

class PasswordLoginScreen : public Screen {
 public:
  bool takes_text() const override { return true; }
  explicit PasswordLoginScreen(AppContext& ctx) : Screen(ctx, "Sign in with a password") {}

  void build() override {
    lv_obj_t* l1 = make_label(root_, ctx_.fonts.small, color::kDim, 8, 26, 304);
    lv_label_set_text(l1, "Username");
    user_ = std::make_unique<TextFieldView>(root_, ctx_.fonts, 8, 40, 304, 28);
    lv_obj_t* l2 = make_label(root_, ctx_.fonts.small, color::kDim, 8, 76, 304);
    lv_label_set_text(l2, "Password");
    pass_ = std::make_unique<TextFieldView>(root_, ctx_.fonts, 8, 90, 304, 28);
    pass_->set_mask(true);
    lv_obj_t* hint = make_label(root_, ctx_.fonts.small, color::kDim, 8, 128, 304, LV_LABEL_LONG_MODE_WRAP);
    lv_label_set_text(hint, "Up/Down or Tab: switch field\nEnter: next, then sign in   Esc: back");
    focus(0);
  }

  bool on_key(const ui::KeyEvent& e) override {
    if (busy_) return true;
    TextFieldView* field = field_ == 0 ? user_.get() : pass_.get();
    if (field->handle_key(e)) return true;
    switch (e.key) {
      case ui::Key::Up: focus(0); return true;
      case ui::Key::Down: focus(1); return true;
      case ui::Key::Tab: focus(field_ == 0 ? 1 : 0); return true;
      case ui::Key::Enter:
        if (field_ == 0) {
          focus(1);
        } else {
          submit();
        }
        return true;
      default: return false;
    }
  }

 private:
  void focus(int which) {
    field_ = which;
    user_->set_focused(which == 0);
    pass_->set_focused(which == 1);
  }

  void submit() {
    if (user_->text().empty()) {
      ctx_.toast->show("Type your username", Toast::Kind::Warning);
      focus(0);
      return;
    }
    busy_ = true;
    set_status("Signing in...");
    auto client = ctx_.client;
    auto user = user_->text();
    auto password = pass_->text();
    async([client, user, password] { return client->login_password(user, password); },
          [this](jf::Result<jf::AuthResult> r) {
            busy_ = false;
            set_status("");
            if (!r) {
              const bool wrong = r.error().kind == jf::ErrorKind::Unauthorized;
              ctx_.toast->show(wrong ? "Wrong username or password" : error_text(r.error()), Toast::Kind::Error, 3500);
              return;
            }
            finish_login(ctx_, r.value());
          });
  }

  std::unique_ptr<TextFieldView> user_;
  std::unique_ptr<TextFieldView> pass_;
  int field_ = 0;
  bool busy_ = false;
};

// ----------------------------------------------------------------- text input

class TextInputScreen : public Screen {
 public:
  bool takes_text() const override { return true; }
  TextInputScreen(AppContext& ctx, TextInputSpec spec) : Screen(ctx, spec.title), spec_(std::move(spec)) {}

  void build() override {
    lv_obj_t* prompt = make_label(root_, ctx_.fonts.small, color::kDim, 8, 30, 304);
    lv_label_set_text(prompt, spec_.prompt.c_str());
    field_ = std::make_unique<TextFieldView>(root_, ctx_.fonts, 8, 48, 304, 30);
    field_->set_mask(spec_.mask);
    field_->set_placeholder(spec_.placeholder);
    field_->set_text(spec_.initial);
    lv_obj_t* hint = make_label(root_, ctx_.fonts.small, color::kDim, 8, 90, 304);
    lv_label_set_text(hint, "Enter: OK   Esc: cancel");
  }

  bool on_key(const ui::KeyEvent& e) override {
    if (field_->handle_key(e)) return true;
    if (e.key == ui::Key::Enter && !e.repeat) {
      if (spec_.on_submit && spec_.on_submit(field_->text())) ctx_.nav->pop();
      return true;
    }
    return false;
  }

 private:
  TextInputSpec spec_;
  std::unique_ptr<TextFieldView> field_;
};

}  // namespace

std::unique_ptr<Screen> make_setup_screen(AppContext& ctx) { return std::make_unique<SetupScreen>(ctx); }
std::unique_ptr<Screen> make_connect_screen(AppContext& ctx) { return std::make_unique<ConnectScreen>(ctx); }
std::unique_ptr<Screen> make_password_login_screen(AppContext& ctx) {
  return std::make_unique<PasswordLoginScreen>(ctx);
}
std::unique_ptr<Screen> make_text_input_screen(AppContext& ctx, TextInputSpec spec) {
  return std::make_unique<TextInputScreen>(ctx, std::move(spec));
}

}  // namespace zerofin::app
