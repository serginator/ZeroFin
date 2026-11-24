#pragma once

#include <string>

#include "lvgl.h"
#include "platform/status_reader.h"
#include "ui/theme.h"
#include "ui_core/player_controls.h"

namespace zerofin::app {

// The widgets drawn over the video: the control layer (status at the top right, six icons, the
// position line), a volume level, and a banner for messages. Pure drawing: the player screen
// decides what to show.
class PlayerOverlay {
 public:
  static constexpr int kIconCount = 6;  // ui::PlayerIcon values

  PlayerOverlay(lv_obj_t* parent, const Fonts& fonts);
  ~PlayerOverlay();
  PlayerOverlay(const PlayerOverlay&) = delete;
  PlayerOverlay& operator=(const PlayerOverlay&) = delete;

  void set_controls_visible(bool visible);
  bool controls_visible() const { return controls_visible_; }

  // `enabled` is indexed by ui::PlayerIcon.
  void set_focus(ui::PlayerIcon icon, const bool enabled[kIconCount]);
  // The play/pause icon shows the action of Enter: the pause glyph while playing.
  void set_playing(bool playing);
  // A duration of 0 hides the bar and the total time.
  void set_position(double position_s, double duration_s);
  void set_status(const platform::StatusSnapshot& status);

  // Shows the volume level for one second. The control layer does not open.
  void show_volume(int percent);
  // Shows a short text ("Muted", "+15 s") for one second in the same box, without a level bar.
  void show_notice(const std::string& text);
  // An empty text hides the banner.
  void set_banner(const std::string& text, uint32_t color);

 private:
  static void on_volume_timer(lv_timer_t* timer);
  void show_volume_box();  // shows the box and (re)starts its one-second timer

  const Fonts& fonts_;
  lv_obj_t* controls_ = nullptr;
  lv_obj_t* wifi_ = nullptr;
  lv_obj_t* battery_ = nullptr;
  lv_obj_t* percent_ = nullptr;
  lv_obj_t* time_ = nullptr;
  lv_obj_t* cells_[kIconCount] = {};
  lv_obj_t* labels_[kIconCount] = {};
  lv_obj_t* position_label_ = nullptr;
  lv_obj_t* duration_label_ = nullptr;
  lv_obj_t* track_ = nullptr;
  lv_obj_t* fill_ = nullptr;
  lv_obj_t* banner_box_ = nullptr;
  lv_obj_t* banner_ = nullptr;
  lv_obj_t* volume_box_ = nullptr;
  lv_obj_t* volume_label_ = nullptr;
  lv_obj_t* volume_track_ = nullptr;
  lv_obj_t* volume_fill_ = nullptr;
  lv_timer_t* volume_timer_ = nullptr;

  bool controls_visible_ = false;
  int focus_ = -1;
  bool enabled_[kIconCount] = {};
  bool playing_ = true;
  int position_text_s_ = -1;
  int duration_text_s_ = -1;
  int fill_w_ = -1;
  std::string banner_text_;
  uint32_t banner_color_ = color::kText;  // the colour the label already has
  bool banner_shown_ = false;
};

}  // namespace zerofin::app
