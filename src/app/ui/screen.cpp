#include "ui/screen.h"

#include "ui/widgets.h"

namespace zerofin::app {

Screen::Screen(AppContext& ctx, std::string title)
    : ctx_(ctx), title_text_(std::move(title)), alive_(Async::make_alive()) {}

Screen::~Screen() {
  *alive_ = false;
  for (auto& [timer, slot] : timers_) {
    if (!slot->dead) lv_timer_delete(timer);
  }
  if (root_) lv_obj_delete(root_);
}

void Screen::attach(lv_obj_t* parent) {
  root_ = make_plain_obj(parent, 0, 0, kScreenW, kScreenH);
  lv_obj_set_style_bg_color(root_, lv_color_hex(color::kBackground), 0);
  lv_obj_set_style_bg_opa(root_, LV_OPA_COVER, 0);
  if (has_title_bar()) {
    // The player has no title bar and keeps its own plain background for the video.
    if (ctx_.background) {
      lv_obj_t* bg = lv_image_create(root_);
      lv_image_set_src(bg, ctx_.background);
      lv_obj_set_pos(bg, 0, 0);
      lv_obj_remove_flag(bg, LV_OBJ_FLAG_CLICKABLE);
    }
    lv_obj_t* bar = make_plain_obj(root_, 0, 0, kScreenW, kTitleH);
    lv_obj_set_style_bg_color(bar, lv_color_hex(color::kBar), 0);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);
    const int h = lv_font_get_line_height(ctx_.fonts.bold);
    title_ = make_label(bar, ctx_.fonts.bold, color::kText, 8, (kTitleH - h) / 2, 190);
    status_ = make_label(bar, ctx_.fonts.small, color::kDim, 200,
                         (kTitleH - lv_font_get_line_height(ctx_.fonts.small)) / 2, 112, LV_LABEL_LONG_MODE_CLIP);
    lv_obj_set_style_text_align(status_, LV_TEXT_ALIGN_RIGHT, 0);
    lv_label_set_text(title_, title_text_.c_str());
  }
  build();
}

void Screen::set_status(const std::string& text, uint32_t color) {
  if (!status_) return;
  lv_label_set_text(status_, text.c_str());
  lv_obj_set_style_text_color(status_, lv_color_hex(color), 0);
}

lv_timer_t* Screen::add_timer(uint32_t period_ms, std::function<void()> fn) {
  auto slot = std::make_unique<TimerSlot>(TimerSlot{std::move(fn), this});
  lv_timer_t* timer = lv_timer_create(
      [](lv_timer_t* t) {
        auto* s = static_cast<TimerSlot*>(lv_timer_get_user_data(t));
        Screen* owner = s->owner;
        s->running = true;
        s->fn();
        s->running = false;
        // The callback may have removed its own timer: free the slot now that it has returned.
        if (s->dead) owner->drop_dead_timers();
      },
      period_ms, slot.get());
  timers_.emplace_back(timer, std::move(slot));
  return timer;
}

void Screen::remove_timer(lv_timer_t* timer) {
  for (auto it = timers_.begin(); it != timers_.end(); ++it) {
    if (it->first != timer || it->second->dead) continue;
    lv_timer_delete(timer);
    if (it->second->running) {
      it->second->dead = true;
    } else {
      timers_.erase(it);
    }
    return;
  }
}

void Screen::drop_dead_timers() {
  for (auto it = timers_.begin(); it != timers_.end();) {
    if (it->second->dead && !it->second->running) it = timers_.erase(it); else ++it;
  }
}

// ---------------------------------------------------------------- Navigator

void Navigator::push(std::unique_ptr<Screen> screen) {
  pending_.push_back({Op::Type::Push, std::move(screen)});
}
void Navigator::pop() { pending_.push_back({Op::Type::Pop, nullptr}); }
void Navigator::reset(std::unique_ptr<Screen> screen) {
  pending_.push_back({Op::Type::Reset, std::move(screen)});
}

bool Navigator::handle_key(const ui::KeyEvent& event) {
  if (stack_.empty()) return false;
  const ui::KeyEvent key = stack_.back()->takes_text() ? event : ui::translate_navigation_key(event);
  bool used = stack_.back()->on_key(key);
  if (!used && key.key == ui::Key::Esc && stack_.size() > 1 && !event.repeat) {
    pop();
    used = true;
  }
  process_pending();
  return used;
}

void Navigator::show_top() {
  if (stack_.empty()) return;
  Screen* top = stack_.back().get();
  lv_obj_remove_flag(top->root(), LV_OBJ_FLAG_HIDDEN);
  top->on_show();
}

void Navigator::process_pending() {
  while (!pending_.empty()) {
    std::vector<Op> ops;
    ops.swap(pending_);
    for (auto& op : ops) {
      switch (op.type) {
        case Op::Type::Push:
          if (!stack_.empty()) lv_obj_add_flag(stack_.back()->root(), LV_OBJ_FLAG_HIDDEN);
          op.screen->attach(parent_);
          stack_.push_back(std::move(op.screen));
          show_top();
          break;
        case Op::Type::Pop:
          if (stack_.size() > 1) {
            stack_.pop_back();
            show_top();
          }
          break;
        case Op::Type::Reset:
          stack_.clear();
          op.screen->attach(parent_);
          stack_.push_back(std::move(op.screen));
          show_top();
          break;
      }
    }
  }
}

}  // namespace zerofin::app
