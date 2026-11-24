#include <doctest/doctest.h>

#include "ui_core/keys.h"

using namespace zerofin::ui;

TEST_CASE("letters map to lower and upper case") {
  CHECK(map_evdev_key(16, false).ch == 'q');
  CHECK(map_evdev_key(16, true).ch == 'Q');
  CHECK(map_evdev_key(30, false).ch == 'a');
  CHECK(map_evdev_key(38, true).ch == 'L');
  CHECK(map_evdev_key(44, false).ch == 'z');
  CHECK(map_evdev_key(50, false).ch == 'm');
  CHECK(map_evdev_key(25, false).key == Key::Char);
}

TEST_CASE("digits and their shifted symbols") {
  CHECK(map_evdev_key(2, false).ch == '1');
  CHECK(map_evdev_key(11, false).ch == '0');
  CHECK(map_evdev_key(2, true).ch == '!');
  CHECK(map_evdev_key(4, true).ch == '#');
  CHECK(map_evdev_key(11, true).ch == ')');
}

TEST_CASE("punctuation needed for URLs and passwords") {
  CHECK(map_evdev_key(52, false).ch == '.');
  CHECK(map_evdev_key(53, false).ch == '/');
  CHECK(map_evdev_key(39, true).ch == ':');
  CHECK(map_evdev_key(12, false).ch == '-');
  CHECK(map_evdev_key(12, true).ch == '_');
  CHECK(map_evdev_key(40, true).ch == '"');
  CHECK(map_evdev_key(43, false).ch == '\\');
  CHECK(map_evdev_key(51, false).ch == ',');
  CHECK(map_evdev_key(57, false).ch == ' ');
}

TEST_CASE("control keys") {
  CHECK(map_evdev_key(1, false).key == Key::Esc);
  CHECK(map_evdev_key(14, false).key == Key::Backspace);
  CHECK(map_evdev_key(15, false).key == Key::Tab);
  CHECK(map_evdev_key(28, false).key == Key::Enter);
  CHECK(map_evdev_key(111, false).key == Key::Delete);
}

TEST_CASE("arrow keys, as sent for fn+F, fn+Z, fn+X, fn+C") {
  CHECK(map_evdev_key(103, false).key == Key::Up);
  CHECK(map_evdev_key(105, false).key == Key::Left);
  CHECK(map_evdev_key(108, false).key == Key::Down);
  CHECK(map_evdev_key(106, false).key == Key::Right);
}

TEST_CASE("media keys, as sent for fn+Q, fn+W, fn+E, fn+S, fn+D") {
  CHECK(map_evdev_key(164, false).key == Key::PlayPause);
  CHECK(map_evdev_key(165, false).key == Key::Prev);
  CHECK(map_evdev_key(163, false).key == Key::Next);
  CHECK(map_evdev_key(114, false).key == Key::VolumeDown);
  CHECK(map_evdev_key(115, false).key == Key::VolumeUp);
}

TEST_CASE("unknown codes and modifier codes give no key") {
  CHECK(map_evdev_key(0, false).key == Key::None);
  CHECK(map_evdev_key(42, false).key == Key::None);  // left shift
  CHECK(map_evdev_key(9999, true).key == Key::None);
}

TEST_CASE("shift codes") {
  CHECK(is_shift_code(42));
  CHECK(is_shift_code(54));
  CHECK_FALSE(is_shift_code(58));
  CHECK_FALSE(is_shift_code(16));
}

TEST_CASE("sym+b on the CP0 sends code 91 and types a dot") {
  CHECK(map_evdev_key(91, false).key == Key::Char);
  CHECK(map_evdev_key(91, false).ch == '.');
  CHECK(map_evdev_key(383, false).key == Key::None);  // the sym key itself is ignored
}

TEST_CASE("sym+s on the CP0 sends code 80 and types a colon") {
  CHECK(map_evdev_key(80, false).key == Key::Char);
  CHECK(map_evdev_key(80, false).ch == ':');
}

TEST_CASE("translate_navigation_key turns F X Z C into the arrows, in any case") {
  KeyEvent f;
  f.key = Key::Char;
  f.ch = 'f';
  CHECK(translate_navigation_key(f).key == Key::Up);
  f.ch = 'F';
  CHECK(translate_navigation_key(f).key == Key::Up);
  f.ch = 'x';
  CHECK(translate_navigation_key(f).key == Key::Down);
  f.ch = 'z';
  CHECK(translate_navigation_key(f).key == Key::Left);
  f.ch = 'C';
  CHECK(translate_navigation_key(f).key == Key::Right);
}

TEST_CASE("translate_navigation_key leaves every other key alone and keeps the repeat flag") {
  KeyEvent q;
  q.key = Key::Char;
  q.ch = 'q';
  CHECK(translate_navigation_key(q).key == Key::Char);
  CHECK(translate_navigation_key(q).ch == 'q');
  KeyEvent enter;
  enter.key = Key::Enter;
  CHECK(translate_navigation_key(enter).key == Key::Enter);
  KeyEvent held;
  held.key = Key::Char;
  held.ch = 'c';
  held.repeat = true;
  CHECK(translate_navigation_key(held).repeat);
}
