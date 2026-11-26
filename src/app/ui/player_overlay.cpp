#include "ui/player_overlay.h"

#include <algorithm>

#include "ui/widgets.h"
#include "ui_core/format.h"
#include "ui_core/status_info.h"

namespace zerofin::app {
namespace {

constexpr int kBandY = 104;
constexpr int kBandH = kScreenH - kBandY;
constexpr int kCellW = 44;
constexpr int kCellH = 26;
constexpr int kCellGap = 6;
constexpr int kTrackX = 62;
constexpr int kTrackW = 196;
constexpr uint32_t kVolumeMs = 1000;

lv_obj_t* make_box(lv_obj_t* parent, int x, int y, int w, int h, uint32_t color, lv_opa_t opa, int radius) {
  lv_obj_t* o = make_plain_obj(parent, x, y, w, h);
  lv_obj_set_style_bg_color(o, lv_color_hex(color), 0);
  lv_obj_set_style_bg_opa(o, opa, 0);
  lv_obj_set_style_radius(o, radius, 0);
  return o;
}

const char* battery_symbol(const ui::BatteryInfo& b) {
  if (b.charging) return LV_SYMBOL_CHARGE;
  if (b.percent >= 88) return LV_SYMBOL_BATTERY_FULL;
  if (b.percent >= 63) return LV_SYMBOL_BATTERY_3;
  if (b.percent >= 38) return LV_SYMBOL_BATTERY_2;
  if (b.percent >= 13) return LV_SYMBOL_BATTERY_1;
  return LV_SYMBOL_BATTERY_EMPTY;
}

}  // namespace

PlayerOverlay::PlayerOverlay(lv_obj_t* parent, const Fonts& fonts) : fonts_(fonts) {
  // --- the control layer ---
  controls_ = make_plain_obj(parent, 0, 0, kScreenW, kScreenH);
  lv_obj_add_flag(controls_, LV_OBJ_FLAG_HIDDEN);

  lv_obj_t* top = make_box(controls_, 196, 0, 124, 22, 0x000000, LV_OPA_70, 0);
  wifi_ = lv_label_create(top);
  lv_obj_set_style_text_font(wifi_, &lv_font_montserrat_14, 0);
  lv_obj_set_pos(wifi_, 6, 3);
  lv_label_set_text(wifi_, LV_SYMBOL_WIFI);
  battery_ = lv_label_create(top);
  lv_obj_set_style_text_font(battery_, &lv_font_montserrat_14, 0);
  lv_obj_set_pos(battery_, 26, 3);
  lv_label_set_text(battery_, LV_SYMBOL_BATTERY_FULL);
  percent_ = make_label(top, fonts_.small, color::kText, 46, 4, 30, LV_LABEL_LONG_MODE_CLIP);
  time_ = make_label(top, fonts_.bold, color::kText, 78, 3, 40, LV_LABEL_LONG_MODE_CLIP);
  lv_obj_set_style_text_align(time_, LV_TEXT_ALIGN_RIGHT, 0);

  lv_obj_t* band = make_box(controls_, 0, kBandY, kScreenW, kBandH, 0x000000, LV_OPA_70, 0);
  const int total_w = kIconCount * kCellW + (kIconCount - 1) * kCellGap;
  const int start_x = (kScreenW - total_w) / 2;
  const char* texts[kIconCount] = {LV_SYMBOL_PREV, "-15", LV_SYMBOL_PAUSE, "+15", LV_SYMBOL_NEXT, LV_SYMBOL_SETTINGS};
  for (int i = 0; i < kIconCount; ++i) {
    cells_[i] = make_plain_obj(band, start_x + i * (kCellW + kCellGap), 8, kCellW, kCellH);
    lv_obj_set_style_radius(cells_[i], 5, 0);
    labels_[i] = lv_label_create(cells_[i]);
    const bool symbol = i == 0 || i == 2 || i == 4 || i == 5;
    lv_obj_set_style_text_font(labels_[i], symbol ? &lv_font_montserrat_16 : fonts_.bold, 0);
    lv_label_set_text(labels_[i], texts[i]);
    lv_obj_center(labels_[i]);
    enabled_[i] = true;
  }

  position_label_ = make_label(band, fonts_.small, color::kText, 8, 44, 52, LV_LABEL_LONG_MODE_CLIP);
  track_ = make_box(band, kTrackX, 50, kTrackW, 4, color::kBar, LV_OPA_COVER, 2);
  fill_ = make_box(track_, 0, 0, 0, 4, color::kAccent, LV_OPA_COVER, 2);
  duration_label_ = make_label(band, fonts_.small, color::kText, 262, 44, 50, LV_LABEL_LONG_MODE_CLIP);
  lv_obj_set_style_text_align(duration_label_, LV_TEXT_ALIGN_RIGHT, 0);
  lv_label_set_text(position_label_, "0:00");

  // --- the volume level ---
  volume_box_ = make_box(parent, 90, 10, 140, 28, 0x000000, LV_OPA_80, 6);
  lv_obj_add_flag(volume_box_, LV_OBJ_FLAG_HIDDEN);
  volume_label_ = make_label(volume_box_, fonts_.normal, color::kText, 8, 1, 124, LV_LABEL_LONG_MODE_CLIP);
  volume_track_ = make_box(volume_box_, 8, 21, 124, 4, color::kBar, LV_OPA_COVER, 2);
  volume_fill_ = make_box(volume_track_, 0, 0, 0, 4, color::kAccent, LV_OPA_COVER, 2);

  // --- the banner ---
  banner_box_ = make_box(parent, 20, 66, 280, 34, 0x000000, LV_OPA_80, 6);
  lv_obj_add_flag(banner_box_, LV_OBJ_FLAG_HIDDEN);
  banner_ = make_label(banner_box_, fonts_.normal, color::kText, 6, 8, 268, LV_LABEL_LONG_MODE_WRAP);
  lv_obj_set_style_text_align(banner_, LV_TEXT_ALIGN_CENTER, 0);

  const bool all[kIconCount] = {true, true, true, true, true, true};
  set_focus(ui::PlayerIcon::PlayPause, all);
}

PlayerOverlay::~PlayerOverlay() {
  if (volume_timer_) lv_timer_delete(volume_timer_);
}

void PlayerOverlay::set_controls_visible(bool visible) {
  if (visible == controls_visible_) return;
  controls_visible_ = visible;
  lv_obj_set_flag(controls_, LV_OBJ_FLAG_HIDDEN, !visible);
}

void PlayerOverlay::set_focus(ui::PlayerIcon icon, const bool enabled[kIconCount]) {
  const int focus = static_cast<int>(icon);
  bool changed = focus != focus_;
  for (int i = 0; i < kIconCount; ++i) changed = changed || enabled[i] != enabled_[i];
  if (!changed) return;
  for (int i = 0; i < kIconCount; ++i) {
    enabled_[i] = enabled[i];
    const bool focused = i == focus;
    lv_obj_set_style_bg_color(cells_[i], lv_color_hex(color::kSelected), 0);
    lv_obj_set_style_bg_opa(cells_[i], focused ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(cells_[i], focused ? 1 : 0, 0);
    lv_obj_set_style_border_color(cells_[i], lv_color_hex(color::kAccent), 0);
    lv_obj_set_style_text_color(labels_[i], lv_color_hex(enabled[i] ? color::kText : color::kDim), 0);
    lv_obj_set_style_text_opa(labels_[i], enabled[i] ? LV_OPA_COVER : LV_OPA_50, 0);
  }
  focus_ = focus;
}

void PlayerOverlay::set_playing(bool playing) {
  if (playing == playing_) return;
  playing_ = playing;
  lv_label_set_text(labels_[static_cast<int>(ui::PlayerIcon::PlayPause)], playing ? LV_SYMBOL_PAUSE : LV_SYMBOL_PLAY);
  lv_obj_center(labels_[static_cast<int>(ui::PlayerIcon::PlayPause)]);
}

void PlayerOverlay::set_position(double position_s, double duration_s) {
  const int pos = static_cast<int>(std::max(0.0, position_s));
  if (pos != position_text_s_) {
    position_text_s_ = pos;
    lv_label_set_text(position_label_, ui::format_timecode(pos).c_str());
  }
  const bool known = duration_s > 0;
  const int dur = known ? static_cast<int>(duration_s) : 0;
  if (dur != duration_text_s_) {
    duration_text_s_ = dur;
    lv_label_set_text(duration_label_, known ? ui::format_timecode(dur).c_str() : "");
    lv_obj_set_flag(track_, LV_OBJ_FLAG_HIDDEN, !known);
  }
  const double fraction = known ? std::clamp(position_s / duration_s, 0.0, 1.0) : 0.0;
  const int w = static_cast<int>(fraction * kTrackW);
  if (w != fill_w_) {
    fill_w_ = w;
    lv_obj_set_width(fill_, w);
  }
}

void PlayerOverlay::set_status(const platform::StatusSnapshot& status) {
  lv_obj_set_flag(wifi_, LV_OBJ_FLAG_HIDDEN, !status.dbm);
  if (status.dbm) {
    const int bars = ui::signal_bars(*status.dbm);
    lv_obj_set_style_text_color(
        wifi_, lv_color_hex(bars >= 3 ? color::kText : (bars == 2 ? color::kWarn : color::kError)), 0);
  }
  lv_obj_set_flag(battery_, LV_OBJ_FLAG_HIDDEN, !status.battery);
  lv_obj_set_flag(percent_, LV_OBJ_FLAG_HIDDEN, !status.battery);
  if (status.battery) {
    lv_label_set_text(battery_, battery_symbol(*status.battery));
    lv_obj_set_style_text_color(
        battery_,
        lv_color_hex(status.battery->charging ? color::kGood : (status.battery->percent < 13 ? color::kError : color::kText)),
        0);
    lv_label_set_text(percent_, (std::to_string(status.battery->percent) + "%").c_str());
  }
  lv_label_set_text(time_, ui::format_clock(status.hour, status.minute).c_str());
}

void PlayerOverlay::on_volume_timer(lv_timer_t* timer) {
  auto* self = static_cast<PlayerOverlay*>(lv_timer_get_user_data(timer));
  lv_obj_add_flag(self->volume_box_, LV_OBJ_FLAG_HIDDEN);
  self->volume_timer_ = nullptr;  // a one-shot timer deletes itself
}

void PlayerOverlay::show_volume(int percent) {
  percent = std::clamp(percent, 0, 100);
  lv_label_set_text(volume_label_, ("Volume " + std::to_string(percent) + "%").c_str());
  lv_obj_set_y(volume_label_, 1);
  lv_obj_set_width(volume_fill_, percent * 124 / 100);
  lv_obj_remove_flag(volume_track_, LV_OBJ_FLAG_HIDDEN);
  show_volume_box();
}

void PlayerOverlay::show_notice(const std::string& text) {
  lv_label_set_text(volume_label_, text.c_str());
  lv_obj_set_y(volume_label_, 5);  // centered: there is no level bar
  lv_obj_add_flag(volume_track_, LV_OBJ_FLAG_HIDDEN);
  show_volume_box();
}

void PlayerOverlay::show_volume_box() {
  lv_obj_remove_flag(volume_box_, LV_OBJ_FLAG_HIDDEN);
  if (volume_timer_) {
    lv_timer_reset(volume_timer_);
  } else {
    volume_timer_ = lv_timer_create(on_volume_timer, kVolumeMs, this);
    lv_timer_set_repeat_count(volume_timer_, 1);
  }
}

void PlayerOverlay::set_banner(const std::string& text, uint32_t text_color) {
  if (text.empty()) {
    if (!banner_text_.empty()) lv_obj_add_flag(banner_box_, LV_OBJ_FLAG_HIDDEN);
    banner_text_.clear();
    banner_shown_ = false;
    return;
  }
  if (text_color != banner_color_) {
    banner_color_ = text_color;
    lv_obj_set_style_text_color(banner_, lv_color_hex(text_color), 0);
  }
  if (text != banner_text_) {
    banner_text_ = text;
    lv_label_set_text(banner_, text.c_str());
    // One line is 34 px high. Each extra line adds 18 px. The box stays centered.
    const int lines = 1 + static_cast<int>(std::count(text.begin(), text.end(), '\n'));
    const int h = 16 + lines * 18;
    lv_obj_set_size(banner_box_, 280, h);
    lv_obj_set_y(banner_box_, (kScreenH - h) / 2 - 10);
    lv_obj_set_y(banner_, 8);
  }
  if (!banner_shown_) {
    banner_shown_ = true;
    lv_obj_remove_flag(banner_box_, LV_OBJ_FLAG_HIDDEN);
  }
}

}  // namespace zerofin::app
