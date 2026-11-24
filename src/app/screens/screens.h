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

// Opens the right screen for an item: details for a movie or episode, a list of seasons for
// a series, and so on.
void open_item(AppContext& ctx, const jf::Item& item);

void go_home(AppContext& ctx);     // clears the stack and shows the home screen
void go_connect(AppContext& ctx);  // clears the stack and shows the Quick Connect screen
void go_setup(AppContext& ctx);    // clears the stack and shows the server screen

// If `error` means the login ended, forgets it and moves to the Quick Connect screen.
// Returns true in that case.
bool handle_auth_error(AppContext& ctx, const jf::Error& error);

std::string error_text(const jf::Error& error);
std::string reason_text(jf::Reason reason);

// The full-screen player. Needs `ctx.make_backend` to return a backend.
std::unique_ptr<Screen> make_player_screen(AppContext& ctx, jf::PlaybackPlan plan, jf::Item item);

// The default `start_playback`: shows what the player would do. Step 4 replaces it.
void show_playback_plan(AppContext& ctx, const jf::PlaybackPlan& plan, const jf::Item& item);

}  // namespace zerofin::app
