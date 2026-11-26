#include "ui_core/player_controls.h"

#include <cctype>

namespace zerofin::ui {

KeyEvent translate_player_key(const KeyEvent& event) {
  if (event.key != Key::Char) return event;
  const KeyEvent nav = translate_navigation_key(event);
  if (nav.key != Key::Char) return nav;
  Key mapped = Key::None;
  switch (std::tolower(static_cast<unsigned char>(event.ch))) {
    case 'q': mapped = Key::PlayPause; break;
    case 's': mapped = Key::VolumeDown; break;
    case 'd': mapped = Key::VolumeUp; break;
    default: return event;
  }
  KeyEvent out;
  out.key = mapped;
  out.repeat = event.repeat;
  return out;
}

void ControlsModel::set_neighbors(bool has_previous, bool has_next) {
  has_previous_ = has_previous;
  has_next_ = has_next;
  if (!icon_enabled(focus_)) focus_ = PlayerIcon::PlayPause;
}

void ControlsModel::set_pinned(bool pinned) {
  pinned_ = pinned;
  visible_ = false;
}

bool ControlsModel::icon_enabled(PlayerIcon icon) const {
  if (icon == PlayerIcon::Prev) return has_previous_;
  if (icon == PlayerIcon::Next) return has_next_;
  return true;
}

PlayerAction ControlsModel::media_action(Key key) const {
  switch (key) {
    case Key::PlayPause: return PlayerAction::TogglePause;
    case Key::Prev: return has_previous_ ? PlayerAction::Previous : PlayerAction::None;
    case Key::Next: return has_next_ ? PlayerAction::Next : PlayerAction::None;
    case Key::VolumeDown: return PlayerAction::VolumeDown;
    case Key::VolumeUp: return PlayerAction::VolumeUp;
    default: return PlayerAction::None;
  }
}

PlayerAction ControlsModel::icon_action(PlayerIcon icon) const {
  switch (icon) {
    case PlayerIcon::Prev: return PlayerAction::Previous;
    case PlayerIcon::SeekBack: return PlayerAction::SeekBack;
    case PlayerIcon::PlayPause: return PlayerAction::TogglePause;
    case PlayerIcon::SeekForward: return PlayerAction::SeekForward;
    case PlayerIcon::Next: return PlayerAction::Next;
    case PlayerIcon::Tracks: return PlayerAction::OpenTracks;
  }
  return PlayerAction::None;
}

void ControlsModel::move_focus(int direction) {
  int index = static_cast<int>(focus_);
  while (true) {
    index += direction;
    if (index < 0 || index > static_cast<int>(PlayerIcon::Tracks)) return;  // no wrap
    if (icon_enabled(static_cast<PlayerIcon>(index))) {
      focus_ = static_cast<PlayerIcon>(index);
      return;
    }
  }
}

PlayerAction ControlsModel::on_key(const KeyEvent& event, uint32_t now_ms) {
  // A held Esc repeats. Only the first press counts, so holding it cannot hide the controls and
  // then leave the player.
  if (event.key == Key::Esc && event.repeat) return PlayerAction::None;

  // Letter shortcuts that act at once, like the media keys: A mute, W back 15 s, E forward 15 s.
  if (event.key == Key::Char) {
    PlayerAction shortcut = PlayerAction::None;
    switch (std::tolower(static_cast<unsigned char>(event.ch))) {
      case 'a': shortcut = event.repeat ? PlayerAction::None : PlayerAction::ToggleMute; break;
      case 'w': shortcut = PlayerAction::SeekBack; break;
      case 'e': shortcut = PlayerAction::SeekForward; break;
      default: break;
    }
    if (shortcut != PlayerAction::None) {
      if (visible_) last_key_ms_ = now_ms;
      return shortcut;
    }
  }

  const bool media = event.key == Key::PlayPause || event.key == Key::Prev || event.key == Key::Next ||
                     event.key == Key::VolumeDown || event.key == Key::VolumeUp;

  if (media) {
    if (visible_) last_key_ms_ = now_ms;
    return media_action(event.key);
  }

  if (!visible()) {
    if (event.key == Key::Esc) return PlayerAction::Exit;
    visible_ = true;
    focus_ = PlayerIcon::PlayPause;
    last_key_ms_ = now_ms;
    return PlayerAction::None;
  }

  last_key_ms_ = now_ms;
  switch (event.key) {
    case Key::Esc:
      if (pinned_) return PlayerAction::Exit;
      visible_ = false;
      return PlayerAction::None;
    case Key::Left:
      move_focus(-1);
      return PlayerAction::None;
    case Key::Right:
      move_focus(1);
      return PlayerAction::None;
    case Key::Enter:
      return event.repeat ? PlayerAction::None : icon_action(focus_);
    case Key::Tab:
      return event.repeat ? PlayerAction::None : PlayerAction::OpenTracks;
    default:
      return PlayerAction::None;
  }
}

bool ControlsModel::tick(uint32_t now_ms) {
  if (pinned_ || !visible_) return false;
  if (now_ms - last_key_ms_ >= kControlsHideMs) {
    visible_ = false;
    return true;
  }
  return false;
}

}  // namespace zerofin::ui
