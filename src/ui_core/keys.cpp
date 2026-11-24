#include "ui_core/keys.h"

#include <cctype>
#include <cstddef>

namespace zerofin::ui {
namespace {

KeyEvent ch(char c) { return KeyEvent{Key::Char, c, false}; }
KeyEvent just(Key k) { return KeyEvent{k, 0, false}; }

// Rows of the US layout, by evdev code.
constexpr const char kRowQ[] = "qwertyuiop";   // codes 16..25
constexpr const char kRowA[] = "asdfghjkl";    // codes 30..38
constexpr const char kRowZ[] = "zxcvbnm";      // codes 44..50
constexpr const char kDigits[] = "1234567890"; // codes 2..11
constexpr const char kDigitsShifted[] = "!@#$%^&*()";

}  // namespace

KeyEvent map_evdev_key(unsigned code, bool shift) {
  // Letters.
  const char* row = nullptr;
  unsigned first = 0;
  std::size_t count = 0;
  if (code >= 16 && code <= 25) {
    row = kRowQ;
    first = 16;
    count = 10;
  } else if (code >= 30 && code <= 38) {
    row = kRowA;
    first = 30;
    count = 9;
  } else if (code >= 44 && code <= 50) {
    row = kRowZ;
    first = 44;
    count = 7;
  }
  if (row && code - first < count) {
    const char c = row[code - first];
    return ch(shift ? static_cast<char>(std::toupper(static_cast<unsigned char>(c))) : c);
  }

  // Digits and their shifted symbols.
  if (code >= 2 && code <= 11) return ch(shift ? kDigitsShifted[code - 2] : kDigits[code - 2]);

  switch (code) {
    case 1: return just(Key::Esc);
    case 12: return ch(shift ? '_' : '-');
    case 13: return ch(shift ? '+' : '=');
    case 14: return just(Key::Backspace);
    case 15: return just(Key::Tab);
    case 26: return ch(shift ? '{' : '[');
    case 27: return ch(shift ? '}' : ']');
    case 28: return just(Key::Enter);
    case 39: return ch(shift ? ':' : ';');
    case 40: return ch(shift ? '"' : '\'');
    case 41: return ch(shift ? '~' : '`');
    case 43: return ch(shift ? '|' : '\\');
    case 51: return ch(shift ? '<' : ',');
    case 52: return ch(shift ? '>' : '.');
    case 53: return ch(shift ? '?' : '/');
    case 57: return ch(' ');
    case 80: return ch(':');  // sym+s on the CP0
    case 91: return ch(shift ? '>' : '.');  // sym+b on the CP0 (the sym key itself sends 383, ignored)
    case 96: return just(Key::Enter);  // keypad enter
    case 102: return just(Key::Home);
    case 103: return just(Key::Up);
    case 104: return just(Key::PageUp);
    case 105: return just(Key::Left);
    case 106: return just(Key::Right);
    case 107: return just(Key::End);
    case 108: return just(Key::Down);
    case 109: return just(Key::PageDown);
    case 111: return just(Key::Delete);
    case 114: return just(Key::VolumeDown);
    case 115: return just(Key::VolumeUp);
    case 163: return just(Key::Next);
    case 164: return just(Key::PlayPause);
    case 165: return just(Key::Prev);
    default: return just(Key::None);
  }
}

KeyEvent translate_navigation_key(const KeyEvent& event) {
  if (event.key != Key::Char) return event;
  Key mapped = Key::None;
  switch (std::tolower(static_cast<unsigned char>(event.ch))) {
    case 'f': mapped = Key::Up; break;
    case 'x': mapped = Key::Down; break;
    case 'z': mapped = Key::Left; break;
    case 'c': mapped = Key::Right; break;
    default: return event;
  }
  KeyEvent out;
  out.key = mapped;
  out.repeat = event.repeat;
  return out;
}

}  // namespace zerofin::ui
