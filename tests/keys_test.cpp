#include <doctest/doctest.h>

#include <string>

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

// The CP0 keyboard layers, read from the device: the driver tables "linux,keymap1" (sym) in the
// device tree and /usr/share/keymaps/tca8418_keypad_m5stack_keymap.map.
TEST_CASE("CP0 sym layer: digits row gives ! @ # $ % ^ & * ( )") {
  const char expect[] = "!@#$%^&*()";
  const unsigned codes[] = {26, 27, 39, 40, 41, 43, 51, 52, 53, 94};
  for (int i = 0; i < 10; ++i) {
    CHECK(map_evdev_key(codes[i], false, KeyLayout::Cp0).ch == expect[i]);
  }
}

TEST_CASE("CP0 sym layer: Q row gives ~ ` _ - + = [ ] { }") {
  const char expect[] = "~`_-+=[]{}";
  const unsigned codes[] = {55, 69, 70, 71, 72, 73, 74, 75, 76, 77};
  for (int i = 0; i < 10; ++i) {
    CHECK(map_evdev_key(codes[i], false, KeyLayout::Cp0).ch == expect[i]);
  }
}

TEST_CASE("CP0 sym+e types an underscore") {
  CHECK(map_evdev_key(70, false, KeyLayout::Cp0).key == Key::Char);
  CHECK(map_evdev_key(70, false, KeyLayout::Cp0).ch == '_');
}

TEST_CASE("CP0 sym layer: A row and bottom row") {
  CHECK(map_evdev_key(79, false, KeyLayout::Cp0).ch == ';');   // sym+a
  CHECK(map_evdev_key(80, false, KeyLayout::Cp0).ch == ':');   // sym+s
  CHECK(map_evdev_key(81, false, KeyLayout::Cp0).ch == '\'');  // sym+d
  CHECK(map_evdev_key(82, false, KeyLayout::Cp0).ch == '"');   // sym+g
  CHECK(map_evdev_key(83, false, KeyLayout::Cp0).ch == '<');   // sym+h
  CHECK(map_evdev_key(85, false, KeyLayout::Cp0).ch == '>');   // sym+j
  CHECK(map_evdev_key(86, false, KeyLayout::Cp0).ch == '\\');  // sym+k
  CHECK(map_evdev_key(89, false, KeyLayout::Cp0).ch == '|');   // sym+l
  CHECK(map_evdev_key(90, false, KeyLayout::Cp0).ch == ',');   // sym+v
  CHECK(map_evdev_key(91, false, KeyLayout::Cp0).ch == '.');   // sym+b
  CHECK(map_evdev_key(92, false, KeyLayout::Cp0).ch == '/');   // sym+n
  CHECK(map_evdev_key(93, false, KeyLayout::Cp0).ch == '?');   // sym+m
}

TEST_CASE("CP0 sym codes ignore shift") {
  CHECK(map_evdev_key(91, true, KeyLayout::Cp0).ch == '.');
  CHECK(map_evdev_key(70, true, KeyLayout::Cp0).ch == '_');
  CHECK(map_evdev_key(26, true, KeyLayout::Cp0).ch == '!');
}

TEST_CASE("the same code means a different key on a US keyboard") {
  CHECK(map_evdev_key(26, false, KeyLayout::Us).ch == '[');
  CHECK(map_evdev_key(26, false, KeyLayout::Cp0).ch == '!');
  CHECK(map_evdev_key(40, false, KeyLayout::Us).ch == '\'');
  CHECK(map_evdev_key(40, false, KeyLayout::Cp0).ch == '$');
}

TEST_CASE("every printable ASCII character can be typed on the CP0") {
  // Collect what the plain keys, shift and the sym layer give, then check all 95 characters.
  std::string typed;
  for (unsigned code = 0; code < 400; ++code) {
    for (bool shift : {false, true}) {
      const KeyEvent e = map_evdev_key(code, shift, KeyLayout::Cp0);
      if (e.key == Key::Char) typed += e.ch;
    }
  }
  for (char c = 32; c < 127; ++c) {
    INFO("missing character: " << c);
    CHECK(typed.find(c) != std::string::npos);
  }
}

TEST_CASE("CP0 layer and modifier keys give no character") {
  CHECK(map_evdev_key(383, false, KeyLayout::Cp0).key == Key::None);  // sym
  CHECK(map_evdev_key(389, false, KeyLayout::Cp0).key == Key::None);  // fn
  CHECK(map_evdev_key(29, false, KeyLayout::Cp0).key == Key::None);   // ctrl
  CHECK(map_evdev_key(56, false, KeyLayout::Cp0).key == Key::None);   // alt
  CHECK(map_evdev_key(59, false, KeyLayout::Cp0).key == Key::None);   // fn+1 is F1
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
