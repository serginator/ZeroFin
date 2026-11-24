#pragma once

#include <cstddef>
#include <string>

#include "ui_core/keys.h"

namespace zerofin::ui {

// Single-line ASCII text with a cursor. No LVGL types, so it can be tested on any host.
class TextEdit {
 public:
  explicit TextEdit(std::string initial = "", size_t max_length = 256);

  const std::string& text() const { return text_; }
  size_t cursor() const { return cursor_; }
  bool empty() const { return text_.empty(); }
  void set_text(std::string text);  // puts the cursor at the end
  void clear();

  // Handles Char, Backspace, Delete, Left, Right, Home and End. Returns true when the key
  // was used (even if nothing changed, for example Left at the start).
  bool handle(const KeyEvent& event);

  struct View {
    std::string text;   // the part that fits, masked when asked
    size_t caret = 0;   // caret position inside `text`
  };
  // The slice of text that fits in `max_chars` characters and shows the caret.
  View view(size_t max_chars, bool mask) const;

 private:
  std::string text_;
  size_t cursor_ = 0;
  size_t max_length_;
};

}  // namespace zerofin::ui
