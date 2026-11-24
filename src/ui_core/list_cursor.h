#pragma once

#include <cstddef>
#include <vector>

namespace zerofin::ui {

// Selection and scroll position for a list that shows `visible` rows at a time.
// Some rows are headers. The selection skips them, but a header is scrolled into view
// when the selection reaches the first row under it.
class ListCursor {
 public:
  explicit ListCursor(size_t visible = 6) : visible_(visible ? visible : 1) {}

  // `selectable[i]` tells whether row i can be selected. Keeps the selection when possible.
  void set_rows(std::vector<bool> selectable);

  size_t row_count() const { return selectable_.size(); }
  size_t visible() const { return visible_; }
  // Index of the selected row, or -1 when nothing is selectable.
  int selected() const { return selected_; }
  // Index of the first visible row.
  size_t top() const { return top_; }

  // Moves to the next selectable row in the direction (+1 down, -1 up). Returns true if it moved.
  bool move(int direction);
  // Moves by one page. Returns true if the selection changed.
  bool page(int direction);
  bool select_first();
  bool select_last();
  // Selects row `index` if it is selectable. Returns true if the selection changed.
  bool select(size_t index);

 private:
  int next_selectable(int from, int direction) const;
  void reveal(bool moving_up);

  std::vector<bool> selectable_;
  size_t visible_;
  int selected_ = -1;
  size_t top_ = 0;
};

}  // namespace zerofin::ui
