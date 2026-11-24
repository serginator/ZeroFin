#include "ui_core/text_edit.h"

#include <algorithm>

namespace zerofin::ui {

TextEdit::TextEdit(std::string initial, size_t max_length) : max_length_(max_length) { set_text(std::move(initial)); }

void TextEdit::set_text(std::string text) {
  text_ = std::move(text);
  if (text_.size() > max_length_) text_.resize(max_length_);
  cursor_ = text_.size();
}

void TextEdit::clear() {
  text_.clear();
  cursor_ = 0;
}

bool TextEdit::handle(const KeyEvent& e) {
  switch (e.key) {
    case Key::Char:
      if (e.ch < 32 || e.ch > 126) return false;
      if (text_.size() < max_length_) {
        text_.insert(cursor_, 1, e.ch);
        ++cursor_;
      }
      return true;
    case Key::Backspace:
      if (cursor_ > 0) {
        text_.erase(cursor_ - 1, 1);
        --cursor_;
      }
      return true;
    case Key::Delete:
      if (cursor_ < text_.size()) text_.erase(cursor_, 1);
      return true;
    case Key::Left:
      if (cursor_ > 0) --cursor_;
      return true;
    case Key::Right:
      if (cursor_ < text_.size()) ++cursor_;
      return true;
    case Key::Home:
      cursor_ = 0;
      return true;
    case Key::End:
      cursor_ = text_.size();
      return true;
    default:
      return false;
  }
}

TextEdit::View TextEdit::view(size_t max_chars, bool mask) const {
  View v;
  if (max_chars == 0) return v;
  const size_t start = cursor_ >= max_chars ? cursor_ - (max_chars - 1) : 0;
  v.text = text_.substr(start, max_chars);
  v.caret = cursor_ - start;
  if (mask) std::fill(v.text.begin(), v.text.end(), '*');
  return v;
}

}  // namespace zerofin::ui
