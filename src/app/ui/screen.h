#pragma once

#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "lvgl.h"
#include "ui/async.h"
#include "ui/context.h"
#include "ui/theme.h"
#include "ui_core/keys.h"

namespace zerofin::app {

// One full-screen page: a 20 px title bar and a 150 px content area.
class Screen {
 public:
  Screen(AppContext& ctx, std::string title);
  virtual ~Screen();
  Screen(const Screen&) = delete;
  Screen& operator=(const Screen&) = delete;

  // Called by the Navigator. Creates the root object and the title bar, then calls build().
  void attach(lv_obj_t* parent);
  lv_obj_t* root() const { return root_; }

  // False for a screen that fills the whole panel (the player). attach() then skips the title bar.
  virtual bool has_title_bar() const { return true; }
  // True for a screen with a text field. Only the other screens turn F X Z C into the arrows.
  virtual bool takes_text() const { return false; }
  virtual void build() {}
  // The screen became the top screen. Also called again when a screen above it is popped.
  virtual void on_show() {}
  // Returns true when the key was used. Unused Esc pops the screen.
  virtual bool on_key(const ui::KeyEvent& /*event*/) { return false; }

  void set_status(const std::string& text, uint32_t color = color::kDim);

 protected:
  // Runs `work` on the worker thread and `done` on the UI thread. `done` is dropped when this
  // screen is gone by then.
  template <typename Work, typename Done>
  void async(Work work, Done done) {
    ctx_.async->run(alive_, std::move(work), std::move(done));
  }
  lv_timer_t* add_timer(uint32_t period_ms, std::function<void()> fn);
  void remove_timer(lv_timer_t* timer);

  AppContext& ctx_;
  lv_obj_t* root_ = nullptr;

 private:
  // A timer may remove itself from inside its own callback. The slot then stays until the callback
  // has returned (`dead`), because the running std::function lives in it.
  struct TimerSlot {
    std::function<void()> fn;
    Screen* owner = nullptr;
    bool running = false;
    bool dead = false;
  };
  void drop_dead_timers();

  std::string title_text_;
  lv_obj_t* title_ = nullptr;
  lv_obj_t* status_ = nullptr;
  Async::Alive alive_;
  std::vector<std::pair<lv_timer_t*, std::unique_ptr<TimerSlot>>> timers_;
};

// A stack of screens. Changes requested while a key or a result is being handled are applied
// afterwards, so a screen is never deleted while one of its methods is running.
class Navigator {
 public:
  explicit Navigator(lv_obj_t* parent) : parent_(parent) {}

  void push(std::unique_ptr<Screen> screen);
  void pop();
  void reset(std::unique_ptr<Screen> screen);    // clears the stack and shows `screen`

  // Sends the key to the top screen. An unused Esc pops when there is a screen below.
  bool handle_key(const ui::KeyEvent& event);
  void process_pending();

 private:
  struct Op {
    enum class Type { Push, Pop, Reset } type;
    std::unique_ptr<Screen> screen;
  };
  void show_top();

  lv_obj_t* parent_;
  std::vector<std::unique_ptr<Screen>> stack_;
  std::vector<Op> pending_;
};

}  // namespace zerofin::app
