#include <doctest/doctest.h>

#include "ui_core/player_controls.h"

using namespace zerofin::ui;

namespace {
KeyEvent k(Key key, bool repeat = false) {
  KeyEvent e;
  e.key = key;
  e.repeat = repeat;
  return e;
}
KeyEvent ch(char c) {
  KeyEvent e;
  e.key = Key::Char;
  e.ch = c;
  return e;
}
}  // namespace

TEST_CASE("hidden: every non-media key only opens the controls") {
  const Key keys[] = {Key::Up, Key::Down, Key::Left, Key::Right, Key::Enter, Key::Tab, Key::Backspace};
  for (Key key : keys) {
    ControlsModel m;
    CHECK(m.on_key(k(key), 0) == PlayerAction::None);
    CHECK(m.visible());
    CHECK(m.focus() == PlayerIcon::PlayPause);
  }
  ControlsModel m;
  CHECK(m.on_key(ch('k'), 0) == PlayerAction::None);
  CHECK(m.visible());
  CHECK(m.focus() == PlayerIcon::PlayPause);
}

TEST_CASE("hidden: media keys act and stay hidden") {
  ControlsModel m;
  CHECK(m.on_key(k(Key::PlayPause), 0) == PlayerAction::TogglePause);
  CHECK_FALSE(m.visible());
  CHECK(m.on_key(k(Key::VolumeUp), 0) == PlayerAction::VolumeUp);
  CHECK_FALSE(m.visible());
  CHECK(m.on_key(k(Key::VolumeDown), 0) == PlayerAction::VolumeDown);
  CHECK_FALSE(m.visible());
}

TEST_CASE("prev and next act only when enabled") {
  ControlsModel m;
  m.set_neighbors(false, true);
  CHECK(m.on_key(k(Key::Prev), 0) == PlayerAction::None);
  CHECK(m.on_key(k(Key::Next), 0) == PlayerAction::Next);
  m.set_neighbors(true, false);
  CHECK(m.on_key(k(Key::Prev), 0) == PlayerAction::Previous);
  CHECK(m.on_key(k(Key::Next), 0) == PlayerAction::None);
}

TEST_CASE("esc: exits when hidden, hides when visible") {
  ControlsModel m;
  CHECK(m.on_key(k(Key::Esc), 0) == PlayerAction::Exit);
  CHECK(m.on_key(k(Key::Up), 0) == PlayerAction::None);
  CHECK(m.visible());
  CHECK(m.on_key(k(Key::Esc), 0) == PlayerAction::None);
  CHECK_FALSE(m.visible());
  CHECK(m.on_key(k(Key::Esc), 0) == PlayerAction::Exit);
}

TEST_CASE("focus moves and skips disabled icons") {
  ControlsModel m;
  m.set_neighbors(false, true);
  m.on_key(k(Key::Up), 0);
  CHECK(m.focus() == PlayerIcon::PlayPause);
  m.on_key(k(Key::Left), 0);
  CHECK(m.focus() == PlayerIcon::SeekBack);
  m.on_key(k(Key::Left), 0);
  CHECK(m.focus() == PlayerIcon::SeekBack);  // Prev is disabled
  m.on_key(k(Key::Right), 0);
  m.on_key(k(Key::Right), 0);
  CHECK(m.focus() == PlayerIcon::SeekForward);
  m.on_key(k(Key::Right), 0);
  CHECK(m.focus() == PlayerIcon::Next);
  m.on_key(k(Key::Right), 0);
  CHECK(m.focus() == PlayerIcon::Tracks);
  m.on_key(k(Key::Right), 0);
  CHECK(m.focus() == PlayerIcon::Tracks);
}

TEST_CASE("enter runs the focused icon, repeats are ignored") {
  ControlsModel m;
  m.on_key(k(Key::Up), 0);
  m.on_key(k(Key::Right), 0);
  CHECK(m.focus() == PlayerIcon::SeekForward);
  CHECK(m.on_key(k(Key::Enter), 0) == PlayerAction::SeekForward);
  CHECK(m.on_key(k(Key::Enter, true), 0) == PlayerAction::None);
  m.on_key(k(Key::Left), 0);
  CHECK(m.on_key(k(Key::Enter), 0) == PlayerAction::TogglePause);
}

TEST_CASE("tab opens the track menu only when visible") {
  ControlsModel m;
  CHECK(m.on_key(k(Key::Tab), 0) == PlayerAction::None);
  CHECK(m.visible());
  CHECK(m.on_key(k(Key::Tab), 0) == PlayerAction::OpenTracks);
}

TEST_CASE("hide timer") {
  ControlsModel m;
  m.on_key(k(Key::Up), 1000);
  CHECK_FALSE(m.tick(4999));
  CHECK(m.visible());
  CHECK(m.tick(5000));
  CHECK_FALSE(m.visible());
  m.on_key(k(Key::Up), 6000);
  m.on_key(k(Key::Down), 8000);
  CHECK_FALSE(m.tick(11999));
  CHECK(m.tick(12000));
}

TEST_CASE("media key while visible restarts the timer") {
  ControlsModel m;
  m.on_key(k(Key::Up), 0);
  CHECK(m.on_key(k(Key::PlayPause), 3000) == PlayerAction::TogglePause);
  CHECK_FALSE(m.tick(6999));
  CHECK(m.tick(7000));
}

TEST_CASE("esc auto-repeat is ignored") {
  ControlsModel m;
  CHECK(m.on_key(k(Key::Esc, true), 0) == PlayerAction::None);  // hidden: no exit
  m.on_key(k(Key::Up), 0);
  CHECK(m.on_key(k(Key::Esc), 0) == PlayerAction::None);        // hides the controls
  CHECK_FALSE(m.visible());
  CHECK(m.on_key(k(Key::Esc, true), 0) == PlayerAction::None);  // the held key does not leave
  CHECK(m.on_key(k(Key::Esc), 0) == PlayerAction::Exit);
}

TEST_CASE("letter shortcuts: a is mute, w and e seek, and they act at once while hidden") {
  ControlsModel m;
  CHECK(m.on_key(ch('a'), 0) == PlayerAction::ToggleMute);
  CHECK(m.on_key(ch('w'), 0) == PlayerAction::SeekBack);
  CHECK(m.on_key(ch('e'), 0) == PlayerAction::SeekForward);
  CHECK(m.on_key(ch('W'), 0) == PlayerAction::SeekBack);
  CHECK(m.on_key(ch('E'), 0) == PlayerAction::SeekForward);
  CHECK_FALSE(m.visible());
}

TEST_CASE("letter shortcuts restart the hide timer while the controls are visible") {
  ControlsModel m;
  m.on_key(k(Key::Up), 0);
  CHECK(m.on_key(ch('e'), 3000) == PlayerAction::SeekForward);
  CHECK_FALSE(m.tick(6999));
  CHECK(m.tick(7000));
}

TEST_CASE("translate_player_key turns the navigation and media letters into keys") {
  CHECK(translate_player_key(ch('f')).key == Key::Up);
  CHECK(translate_player_key(ch('F')).key == Key::Up);
  CHECK(translate_player_key(ch('x')).key == Key::Down);
  CHECK(translate_player_key(ch('z')).key == Key::Left);
  CHECK(translate_player_key(ch('c')).key == Key::Right);
  CHECK(translate_player_key(ch('q')).key == Key::PlayPause);
  CHECK(translate_player_key(ch('s')).key == Key::VolumeDown);
  CHECK(translate_player_key(ch('d')).key == Key::VolumeUp);
}

TEST_CASE("translate_player_key leaves other keys alone and keeps the repeat flag") {
  CHECK(translate_player_key(ch('k')).key == Key::Char);
  CHECK(translate_player_key(ch('k')).ch == 'k');
  CHECK(translate_player_key(k(Key::Enter)).key == Key::Enter);
  CHECK(translate_player_key(k(Key::Up)).key == Key::Up);
  KeyEvent held = ch('c');
  held.repeat = true;
  CHECK(translate_player_key(held).repeat);
}

TEST_CASE("pinned: the controls are visible and the first key acts at once") {
  ControlsModel m;
  m.set_pinned(true);
  CHECK(m.visible());
  CHECK(m.focus() == PlayerIcon::PlayPause);
  CHECK(m.on_key(k(Key::Enter), 0) == PlayerAction::TogglePause);
  CHECK(m.on_key(k(Key::Right), 10) == PlayerAction::None);
  CHECK(m.focus() == PlayerIcon::SeekForward);
  CHECK(m.on_key(k(Key::Enter), 20) == PlayerAction::SeekForward);
  CHECK(m.on_key(k(Key::Tab), 30) == PlayerAction::OpenTracks);
}

TEST_CASE("pinned: the controls never hide and Esc leaves the player") {
  ControlsModel m;
  m.set_pinned(true);
  CHECK_FALSE(m.tick(100000));
  CHECK(m.visible());
  CHECK(m.on_key(k(Key::Esc), 100001) == PlayerAction::Exit);
  CHECK(m.visible());
}

TEST_CASE("pinned: unpinning hides the controls again") {
  ControlsModel m;
  m.set_pinned(true);
  m.set_pinned(false);
  CHECK_FALSE(m.visible());
  CHECK(m.on_key(k(Key::Esc), 0) == PlayerAction::Exit);
}
