#include "ui_core/list_cursor.h"

#include <algorithm>
#include <cstdlib>

namespace zerofin::ui {

int ListCursor::next_selectable(int from, int direction) const {
  const int n = static_cast<int>(selectable_.size());
  for (int i = from + direction; i >= 0 && i < n; i += direction) {
    if (selectable_[static_cast<size_t>(i)]) return i;
  }
  return -1;
}

void ListCursor::reveal(bool moving_up) {
  if (selected_ < 0) {
    top_ = 0;
    return;
  }
  size_t first_needed = static_cast<size_t>(selected_);
  if (moving_up) {
    // Show the headers right above the selected row.
    while (first_needed > 0 && !selectable_[first_needed - 1]) --first_needed;
  }
  if (first_needed < top_) top_ = first_needed;
  const size_t sel = static_cast<size_t>(selected_);
  if (sel >= top_ + visible_) top_ = sel + 1 - visible_;
  const size_t max_top = selectable_.size() > visible_ ? selectable_.size() - visible_ : 0;
  top_ = std::min(top_, max_top);
}

void ListCursor::set_rows(std::vector<bool> selectable) {
  selectable_ = std::move(selectable);
  const int n = static_cast<int>(selectable_.size());
  if (selected_ >= n || selected_ < 0 || !selectable_[static_cast<size_t>(selected_)]) {
    // Keep the position when the old row is still valid. Otherwise pick the first selectable row.
    selected_ = -1;
    for (int i = 0; i < n; ++i) {
      if (selectable_[static_cast<size_t>(i)]) {
        selected_ = i;
        break;
      }
    }
    top_ = 0;
  }
  reveal(true);
}

bool ListCursor::move(int direction) {
  if (selected_ < 0) return false;
  const int next = next_selectable(selected_, direction < 0 ? -1 : 1);
  if (next < 0) return false;
  selected_ = next;
  reveal(direction < 0);
  return true;
}

bool ListCursor::page(int direction) {
  if (selected_ < 0) return false;
  const int step = direction < 0 ? -1 : 1;
  int target = selected_;
  for (size_t moved = 0; moved < visible_;) {
    const int next = next_selectable(target, step);
    if (next < 0) break;
    moved += static_cast<size_t>(std::abs(next - target));
    target = next;
  }
  if (target == selected_) return false;
  selected_ = target;
  reveal(direction < 0);
  return true;
}

bool ListCursor::select_first() {
  const int first = next_selectable(-1, 1);
  if (first < 0 || first == selected_) return false;
  selected_ = first;
  top_ = 0;
  reveal(true);
  return true;
}

bool ListCursor::select_last() {
  const int last = next_selectable(static_cast<int>(selectable_.size()), -1);
  if (last < 0 || last == selected_) return false;
  selected_ = last;
  reveal(false);
  return true;
}

bool ListCursor::select(size_t index) {
  if (index >= selectable_.size() || !selectable_[index]) return false;
  const bool changed = static_cast<int>(index) != selected_;
  const bool up = static_cast<int>(index) < selected_;
  selected_ = static_cast<int>(index);
  reveal(up);
  return changed;
}

}  // namespace zerofin::ui
