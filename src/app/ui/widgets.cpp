#include "ui/widgets.h"

#include <algorithm>

namespace zerofin::app {
namespace {

constexpr int kScrollW = 3;

int line_h(const lv_font_t* f) { return lv_font_get_line_height(f); }

}  // namespace

lv_obj_t* make_plain_obj(lv_obj_t* parent, int x, int y, int w, int h) {
  lv_obj_t* o = lv_obj_create(parent);
  lv_obj_remove_flag(o, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_remove_flag(o, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_style_pad_all(o, 0, 0);
  lv_obj_set_style_border_width(o, 0, 0);
  lv_obj_set_style_radius(o, 0, 0);
  lv_obj_set_style_bg_opa(o, LV_OPA_TRANSP, 0);
  lv_obj_set_pos(o, x, y);
  lv_obj_set_size(o, w, h);
  return o;
}

lv_obj_t* make_label(lv_obj_t* parent, const lv_font_t* font, uint32_t color, int x, int y, int w,
                     lv_label_long_mode_t mode) {
  lv_obj_t* l = lv_label_create(parent);
  lv_label_set_long_mode(l, mode);
  lv_obj_set_style_text_font(l, font, 0);
  lv_obj_set_style_text_color(l, lv_color_hex(color), 0);
  lv_obj_set_pos(l, x, y);
  lv_obj_set_width(l, w);
  if (mode == LV_LABEL_LONG_MODE_DOTS || mode == LV_LABEL_LONG_MODE_CLIP) lv_obj_set_height(l, line_h(font));
  lv_label_set_text(l, "");
  return l;
}

// ---------------------------------------------------------------- ListView

ListView::ListView(lv_obj_t* parent, const Fonts& fonts, int x, int y, int width, size_t visible_rows)
    : fonts_(fonts), width_(width), visible_(visible_rows), cursor_(visible_rows) {
  box_ = make_plain_obj(parent, x, y, width, static_cast<int>(visible_rows) * kRowH);

  objs_.resize(visible_);
  for (size_t i = 0; i < visible_; ++i) {
    RowObjs& o = objs_[i];
    o.row = make_plain_obj(box_, 0, static_cast<int>(i) * kRowH, width, kRowH);
    lv_obj_set_style_bg_color(o.row, lv_color_hex(color::kSelected), 0);
    o.accent = make_plain_obj(o.row, 0, 0, 3, kRowH);
    lv_obj_set_style_bg_color(o.accent, lv_color_hex(color::kAccent), 0);
    lv_obj_set_style_bg_opa(o.accent, LV_OPA_COVER, 0);
    o.primary = make_label(o.row, fonts_.normal, color::kText, 8, (kRowH - line_h(fonts_.normal)) / 2, width - 20);
    o.detail = make_label(o.row, fonts_.small, color::kDim, width - detail_w_ - kScrollW - 6,
                          (kRowH - line_h(fonts_.small)) / 2, detail_w_, LV_LABEL_LONG_MODE_CLIP);
    lv_obj_set_style_text_align(o.detail, LV_TEXT_ALIGN_RIGHT, 0);
    o.bar = make_plain_obj(o.row, 8, kRowH - 3, 0, 2);
    lv_obj_set_style_bg_color(o.bar, lv_color_hex(color::kAccent), 0);
    lv_obj_set_style_bg_opa(o.bar, LV_OPA_COVER, 0);
    lv_obj_add_flag(o.row, LV_OBJ_FLAG_HIDDEN);
  }

  scroll_track_ = make_plain_obj(box_, width - kScrollW, 0, kScrollW, static_cast<int>(visible_) * kRowH);
  lv_obj_set_style_bg_color(scroll_track_, lv_color_hex(color::kBar), 0);
  lv_obj_set_style_bg_opa(scroll_track_, LV_OPA_COVER, 0);
  scroll_thumb_ = make_plain_obj(scroll_track_, 0, 0, kScrollW, 10);
  lv_obj_set_style_bg_color(scroll_thumb_, lv_color_hex(color::kDim), 0);
  lv_obj_set_style_bg_opa(scroll_thumb_, LV_OPA_COVER, 0);
  lv_obj_add_flag(scroll_track_, LV_OBJ_FLAG_HIDDEN);

  message_ = make_label(box_, fonts_.normal, color::kDim, 10, 10, width - 20, LV_LABEL_LONG_MODE_WRAP);
  lv_obj_set_style_text_align(message_, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_add_flag(message_, LV_OBJ_FLAG_HIDDEN);
}

void ListView::set_rows(std::vector<ListRow> rows) {
  rows_ = std::move(rows);
  std::vector<bool> selectable;
  selectable.reserve(rows_.size());
  for (const auto& r : rows_) selectable.push_back(!r.header);
  cursor_.set_rows(std::move(selectable));
  message_active_ = false;
  lv_obj_add_flag(message_, LV_OBJ_FLAG_HIDDEN);
  refresh();
}

void ListView::set_message(const std::string& text, uint32_t color) {
  message_active_ = true;
  lv_label_set_text(message_, text.c_str());
  lv_obj_set_style_text_color(message_, lv_color_hex(color), 0);
  const int h = static_cast<int>(visible_) * kRowH;
  lv_obj_update_layout(message_);
  lv_obj_set_y(message_, std::max(4, (h - lv_obj_get_height(message_)) / 2));
  lv_obj_remove_flag(message_, LV_OBJ_FLAG_HIDDEN);
  refresh();
}

bool ListView::handle_key(const ui::KeyEvent& e) {
  bool changed = false;
  switch (e.key) {
    case ui::Key::Up: changed = cursor_.move(-1); break;
    case ui::Key::Down: changed = cursor_.move(1); break;
    case ui::Key::PageUp: changed = cursor_.page(-1); break;
    case ui::Key::PageDown: changed = cursor_.page(1); break;
    case ui::Key::Home: changed = cursor_.select_first(); break;
    case ui::Key::End: changed = cursor_.select_last(); break;
    default: return false;
  }
  if (changed) refresh();
  return true;
}

bool ListView::select(size_t index) {
  const bool changed = cursor_.select(index);
  if (changed) refresh();
  return changed;
}

bool ListView::near_end(size_t margin) const {
  return cursor_.selected() >= 0 && static_cast<size_t>(cursor_.selected()) + margin >= rows_.size();
}

void ListView::set_detail_width(int width) {
  detail_w_ = width;
  for (auto& o : objs_) {
    lv_obj_set_x(o.detail, width_ - detail_w_ - kScrollW - 6);
    lv_obj_set_width(o.detail, detail_w_);
  }
  refresh();
}

void ListView::set_active(bool active) {
  active_ = active;
  refresh();
}

void ListView::refresh() {
  const size_t top = cursor_.top();
  for (size_t r = 0; r < visible_; ++r) {
    RowObjs& o = objs_[r];
    const size_t idx = top + r;
    if (message_active_ || idx >= rows_.size()) {
      lv_obj_add_flag(o.row, LV_OBJ_FLAG_HIDDEN);
      continue;
    }
    lv_obj_remove_flag(o.row, LV_OBJ_FLAG_HIDDEN);
    const ListRow& row = rows_[idx];
    const bool selected = active_ && static_cast<int>(idx) == cursor_.selected();

    lv_obj_set_style_bg_opa(o.row, selected ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
    lv_obj_set_flag(o.accent, LV_OBJ_FLAG_HIDDEN, !selected);

    const bool has_detail = !row.detail.empty() || row.watched;
    const int primary_w = width_ - 8 - (has_detail ? detail_w_ + kScrollW + 10 : kScrollW + 8);
    lv_obj_set_width(o.primary, primary_w);
    lv_label_set_text(o.primary, row.primary.c_str());
    if (row.header) {
      lv_obj_set_style_text_font(o.primary, fonts_.small, 0);
      lv_obj_set_style_text_color(o.primary, lv_color_hex(color::kAccent), 0);
      lv_obj_set_y(o.primary, kRowH - line_h(fonts_.small) - 3);
    } else {
      lv_obj_set_style_text_font(o.primary, fonts_.normal, 0);
      lv_obj_set_style_text_color(o.primary, lv_color_hex(row.dim ? color::kDim : color::kText), 0);
      lv_obj_set_y(o.primary, (kRowH - line_h(fonts_.normal)) / 2);
    }

    std::string detail = row.detail;
    if (row.watched) detail += detail.empty() ? "\xE2\x9C\x93" : " \xE2\x9C\x93";
    lv_label_set_text(o.detail, detail.c_str());
    lv_obj_set_style_text_color(o.detail, lv_color_hex(row.watched ? color::kGood : color::kDim), 0);

    if (row.progress > 0) {
      lv_obj_set_width(o.bar, std::max(2, static_cast<int>((width_ - 16 - kScrollW) * std::min(1.0, row.progress))));
    }
    lv_obj_set_flag(o.bar, LV_OBJ_FLAG_HIDDEN, !(row.progress > 0));
  }

  const int track_h = static_cast<int>(visible_) * kRowH;
  const bool show_track = !message_active_ && rows_.size() > visible_;
  lv_obj_set_flag(scroll_track_, LV_OBJ_FLAG_HIDDEN, !show_track);
  if (show_track) {
    const int thumb_h = std::max(8, static_cast<int>(track_h * visible_ / rows_.size()));
    const int max_top = static_cast<int>(rows_.size() - visible_);
    const int thumb_y = max_top > 0 ? static_cast<int>((track_h - thumb_h) * static_cast<int>(top) / max_top) : 0;
    lv_obj_set_height(scroll_thumb_, thumb_h);
    lv_obj_set_y(scroll_thumb_, thumb_y);
  }
}

// ------------------------------------------------------------ TextFieldView

TextFieldView::TextFieldView(lv_obj_t* parent, const Fonts& fonts, int x, int y, int width, int height)
    : fonts_(fonts), width_(width) {
  box_ = make_plain_obj(parent, x, y, width, height);
  lv_obj_set_style_bg_color(box_, lv_color_hex(color::kBar), 0);
  lv_obj_set_style_bg_opa(box_, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(box_, 1, 0);
  lv_obj_set_style_radius(box_, 3, 0);
  label_ = make_label(box_, fonts_.normal, color::kText, 6, (height - 2 - line_h(fonts_.normal)) / 2, width - 14,
                      LV_LABEL_LONG_MODE_CLIP);
  render();
}

bool TextFieldView::handle_key(const ui::KeyEvent& event) {
  const bool used = edit_.handle(event);
  if (used) render();
  return used;
}

void TextFieldView::set_text(const std::string& text) {
  edit_.set_text(text);
  render();
}

void TextFieldView::set_mask(bool mask) {
  mask_ = mask;
  render();
}

void TextFieldView::set_placeholder(const std::string& text) {
  placeholder_ = text;
  render();
}

void TextFieldView::set_focused(bool focused) {
  focused_ = focused;
  render();
}

void TextFieldView::render() {
  lv_obj_set_style_border_color(box_, lv_color_hex(focused_ ? color::kAccent : color::kBar), 0);
  if (edit_.empty() && !placeholder_.empty() && !focused_) {
    lv_label_set_text(label_, placeholder_.c_str());
    lv_obj_set_style_text_color(label_, lv_color_hex(color::kDim), 0);
    return;
  }
  const size_t max_chars = static_cast<size_t>(std::max(4, (width_ - 20) / 7));
  ui::TextEdit::View v = edit_.view(max_chars, mask_);
  std::string shown = v.text;
  if (focused_) shown.insert(v.caret, "|");
  if (edit_.empty() && !placeholder_.empty()) {
    // Show the hint after the caret so it is clear the box is empty.
    shown += " " + placeholder_;
  }
  lv_label_set_text(label_, shown.c_str());
  lv_obj_set_style_text_color(label_, lv_color_hex(color::kText), 0);
}

// -------------------------------------------------------------------- Toast

Toast::Toast(lv_obj_t* layer, const Fonts& fonts) {
  box_ = make_plain_obj(layer, 0, kScreenH - 22, kScreenW, 22);
  lv_obj_set_style_bg_opa(box_, LV_OPA_COVER, 0);
  label_ = make_label(box_, fonts.normal, 0xFFFFFF, 8, (22 - line_h(fonts.normal)) / 2, kScreenW - 16);
  lv_obj_add_flag(box_, LV_OBJ_FLAG_HIDDEN);
}

Toast::~Toast() {
  if (timer_) lv_timer_delete(timer_);
}

void Toast::show(const std::string& text, Kind kind, uint32_t ms) {
  uint32_t bg = color::kBar;
  switch (kind) {
    case Kind::Success: bg = 0x2E7D32; break;
    case Kind::Warning: bg = 0x9A6B00; break;
    case Kind::Error: bg = 0xB3312E; break;
    case Kind::Info: bg = 0x1D5A7A; break;
  }
  lv_obj_set_style_bg_color(box_, lv_color_hex(bg), 0);
  lv_label_set_text(label_, text.c_str());
  lv_obj_remove_flag(box_, LV_OBJ_FLAG_HIDDEN);
  lv_obj_move_foreground(box_);
  if (timer_) lv_timer_delete(timer_);
  timer_ = lv_timer_create(&Toast::on_timer, ms, this);
  lv_timer_set_repeat_count(timer_, 1);
}

void Toast::hide() {
  lv_obj_add_flag(box_, LV_OBJ_FLAG_HIDDEN);
}

void Toast::on_timer(lv_timer_t* timer) {
  auto* self = static_cast<Toast*>(lv_timer_get_user_data(timer));
  self->timer_ = nullptr;  // LVGL deletes a one-shot timer after it runs
  self->hide();
}

}  // namespace zerofin::app
