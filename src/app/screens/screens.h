#pragma once

#include <functional>
#include <memory>
#include <string>

#include "jellyfin/client.h"
#include "jellyfin/models.h"
#include "ui/screen.h"
#include "ui_core/format.h"

namespace zerofin::app {

struct ItemListSpec {
  std::string title;
  // Runs on the worker thread. `start` and `limit` page through the list.
  std::function<jf::Result<jf::ItemList>(int start, int limit)> loader;
  std::function<void(const jf::Item&)> on_select;  // UI thread
  ui::RowContext context = ui::RowContext::Browse;
  bool posters = false;  // a grid of posters, else a text list
  bool paged = true;  // false when the loader returns everything in one call
  std::string empty_text = "Nothing here";
};

struct TextInputSpec {
  std::string title;
  std::string prompt;
  std::string initial;
  std::string placeholder;
  bool mask = false;
  // Return true to close the screen. Return false to keep it open (after showing a toast).
  std::function<bool(const std::string&)> on_submit;
};

struct MessageSpec {
  std::string title;
  std::string text;
  std::string confirm_label;  // empty: the screen only informs
  std::function<void()> on_confirm;
  uint32_t text_color = color::kText;
};

std::unique_ptr<Screen> make_setup_screen(AppContext& ctx);
std::unique_ptr<Screen> make_connect_screen(AppContext& ctx);
std::unique_ptr<Screen> make_password_login_screen(AppContext& ctx);
std::unique_ptr<Screen> make_text_input_screen(AppContext& ctx, TextInputSpec spec);
std::unique_ptr<Screen> make_home_screen(AppContext& ctx);
std::unique_ptr<Screen> make_item_list_screen(AppContext& ctx, ItemListSpec spec);
std::unique_ptr<Screen> make_search_screen(AppContext& ctx);
std::unique_ptr<Screen> make_detail_screen(AppContext& ctx, jf::Item item);
std::unique_ptr<Screen> make_settings_screen(AppContext& ctx);
std::unique_ptr<Screen> make_message_screen(AppContext& ctx, MessageSpec spec);
// The servers saved on this device. Open one, remove one, or add one.
std::unique_ptr<Screen> make_server_list_screen(AppContext& ctx);

// An address without its scheme ("https://h:8096" gives "h:8096"), short enough for a list row.
std::string short_address(const std::string& address);

// Opens the right screen for an item: details for a movie or episode, a list of seasons for
// a series, and so on.
void open_item(AppContext& ctx, const jf::Item& item);

void go_home(AppContext& ctx);     // clears the stack and shows the home screen
void go_connect(AppContext& ctx);  // clears the stack and shows the Quick Connect screen
void go_setup(AppContext& ctx);    // clears the stack and shows the server screen
// Clears the stack and shows the saved servers. Shows the server screen when none is saved.
void go_servers(AppContext& ctx);

// If `error` means the login ended, forgets it and moves to the Quick Connect screen.
// Returns true in that case.
bool handle_auth_error(AppContext& ctx, const jf::Error& error);

// Handles an auth error as above. Any other error shows an error toast. Returns true when the
// login ended.
bool report_error(AppContext& ctx, const jf::Error& error, uint32_t toast_ms = 3500);

std::string error_text(const jf::Error& error);
std::string reason_text(jf::Reason reason);

// The full-screen player. Needs `ctx.make_backend` to return a backend.
std::unique_ptr<Screen> make_player_screen(AppContext& ctx, jf::PlaybackPlan plan, jf::Item item);

// Used by `start_playback` when the build has no player: shows what the player would do.
void show_playback_plan(AppContext& ctx, const jf::PlaybackPlan& plan, const jf::Item& item);

}  // namespace zerofin::app
