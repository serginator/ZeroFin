#pragma once

#include <cstdint>

#include "ui_core/keys.h"

namespace zerofin::ui {

// The icons of the control layer, left to right.
enum class PlayerIcon { Prev, SeekBack, PlayPause, SeekForward, Next, Tracks };

enum class PlayerAction {
  None,
  TogglePause,
  SeekBack,
  SeekForward,
  Previous,
  Next,
  OpenTracks,
  VolumeDown,
  VolumeUp,
  ToggleMute,
  Exit
};

constexpr uint32_t kControlsHideMs = 4000;
constexpr int kSeekSeconds = 15;

// The letter keys that act like other keys in the player (no fn needed): F X Z C are the
// arrows, Q is play/pause, S and D are volume down and up. Any case. Other keys come back
// unchanged. A and W and E are handled by ControlsModel (mute, seek back, seek forward).
KeyEvent translate_player_key(const KeyEvent& event);

// Key handling of the player: which key does what while the controls are hidden or shown.
// Pure logic: the caller passes the time.
class ControlsModel {
 public:
  // Enables or greys out the Prev and Next icons.
  void set_neighbors(bool has_previous, bool has_next);

  PlayerAction on_key(const KeyEvent& event, uint32_t now_ms);

  // Hides the controls after kControlsHideMs without a key. True when it just hid them.
  bool tick(uint32_t now_ms);

  bool visible() const { return visible_; }
  PlayerIcon focus() const { return focus_; }
  bool icon_enabled(PlayerIcon icon) const;

 private:
  PlayerAction media_action(Key key) const;
  PlayerAction icon_action(PlayerIcon icon) const;
  void move_focus(int direction);

  bool visible_ = false;
  PlayerIcon focus_ = PlayerIcon::PlayPause;
  bool has_previous_ = false;
  bool has_next_ = false;
  uint32_t last_key_ms_ = 0;
};

}  // namespace zerofin::ui
