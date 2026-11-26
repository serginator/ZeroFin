#pragma once

#include <cstdint>

namespace zerofin::ui {

enum class Key {
  None,
  Up,
  Down,
  Left,
  Right,
  Enter,
  Esc,
  Tab,
  Backspace,
  Delete,
  Home,
  End,
  PageUp,
  PageDown,
  PlayPause,  // fn+Q on the CP0
  Prev,       // fn+W
  Next,       // fn+E
  VolumeDown, // fn+S
  VolumeUp,   // fn+D
  Char        // a printable ASCII character in KeyEvent::ch (letters, digits, space, punctuation)
};

struct KeyEvent {
  Key key = Key::None;
  char ch = 0;
  bool repeat = false;  // the key is held down and the system repeats it
};

// Linux evdev key codes (include/uapi/linux/input-event-codes.h). Listed here so the
// mapping builds and is tested on any host.
namespace evdev {
constexpr unsigned kLeftShift = 42;
constexpr unsigned kRightShift = 54;
constexpr unsigned kCapsLock = 58;
}  // namespace evdev

inline bool is_shift_code(unsigned code) { return code == evdev::kLeftShift || code == evdev::kRightShift; }

// Which codes a keyboard sends. A normal keyboard sends the standard US codes. The Cardputer Zero
// keyboard (a TCA8418 matrix) sends its own codes for the sym layer, and several of them are the
// standard codes of other keys (26 is "!" there, but "[" on a US keyboard).
enum class KeyLayout { Us, Cp0 };

// Maps a Linux evdev key code to a key event. `shift` is true while a shift key is down
// (the caller also folds Caps Lock into it for letters). Unknown codes return Key::None.
// On the Cp0 layout the sym codes give their symbol and ignore `shift`.
KeyEvent map_evdev_key(unsigned code, bool shift, KeyLayout layout = KeyLayout::Us);

// F X Z C act as Up, Down, Left, Right (any case), so the arrows work without fn. Other events
// come back unchanged. Screens that take text must not use it.
KeyEvent translate_navigation_key(const KeyEvent& event);

}  // namespace zerofin::ui
