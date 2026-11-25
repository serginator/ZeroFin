#include "ui_core/grid_cursor.h"

#include <algorithm>

namespace zerofin::ui {

bool GridCursor::empty() const {
  return std::all_of(rows_.begin(), rows_.end(), [](const GridRow& r) { return r.size == 0; });
}

int GridCursor::next_row(int from, int direction) const {
  for (int r = from + direction; r >= 0 && r < static_cast<int>(rows_.size()); r += direction) {
    if (rows_[static_cast<size_t>(r)].size > 0) return r;
  }
  return -1;
}

void GridCursor::set_rows(std::vector<GridRow> rows) {
  rows_ = std::move(rows);
  left_.resize(rows_.size(), 0);
  if (empty()) {
    row_ = col_ = want_col_ = 0;
    std::fill(left_.begin(), left_.end(), 0);
    return;
  }
  if (row_ >= rows_.size()) row_ = rows_.size() - 1;
  if (rows_[row_].size == 0) {
    int r = next_row(static_cast<int>(row_), 1);
    if (r < 0) r = next_row(static_cast<int>(row_), -1);
    row_ = static_cast<size_t>(r);
  }
  col_ = std::min(col_, rows_[row_].size - 1);
  want_col_ = col_;
  for (size_t r = 0; r < rows_.size(); ++r) {
    const size_t vis = std::max<size_t>(1, rows_[r].visible);
    const size_t max_left = rows_[r].size > vis ? rows_[r].size - vis : 0;
    left_[r] = std::min(left_[r], max_left);
  }
  reveal();
}

void GridCursor::reveal() {
  if (row_ >= rows_.size()) return;
  const size_t vis = std::max<size_t>(1, rows_[row_].visible);
  if (col_ < left_[row_]) {
    left_[row_] = col_;
  } else if (col_ >= left_[row_] + vis) {
    left_[row_] = col_ - vis + 1;
  }
}

void GridCursor::go_to_row(size_t row) {
  row_ = row;
  col_ = std::min(want_col_, rows_[row_].size - 1);
  reveal();
}

bool GridCursor::move(Key key) {
  if (empty()) return false;
  const size_t old_row = row_;
  const size_t old_col = col_;
  switch (key) {
    case Key::Up:
    case Key::Down: {
      const int r = next_row(static_cast<int>(row_), key == Key::Up ? -1 : 1);
      if (r < 0) return false;
      go_to_row(static_cast<size_t>(r));
      break;
    }
    case Key::PageUp: {
      const int r = next_row(-1, 1);
      if (r < 0) return false;
      go_to_row(static_cast<size_t>(r));
      break;
    }
    case Key::PageDown: {
      const int r = next_row(static_cast<int>(rows_.size()), -1);
      if (r < 0) return false;
      go_to_row(static_cast<size_t>(r));
      break;
    }
    case Key::Left:
      if (col_ == 0) return false;
      --col_;
      want_col_ = col_;
      break;
    case Key::Right:
      if (col_ + 1 >= rows_[row_].size) return false;
      ++col_;
      want_col_ = col_;
      break;
    case Key::Home:
      col_ = want_col_ = 0;
      break;
    case Key::End:
      col_ = want_col_ = rows_[row_].size - 1;
      break;
    default:
      return false;
  }
  reveal();
  return row_ != old_row || col_ != old_col;
}

bool GridCursor::select(size_t row, size_t col) {
  if (row >= rows_.size() || col >= rows_[row].size) return false;
  row_ = row;
  col_ = want_col_ = col;
  reveal();
  return true;
}

std::optional<std::pair<size_t, size_t>> find_position(const std::vector<RowKeys>& rows, const std::string& row_key,
                                                        const std::string& item_key, size_t old_col) {
  for (size_t r = 0; r < rows.size(); ++r) {
    if (rows[r].key != row_key) continue;
    const auto& items = rows[r].items;
    if (items.empty()) return std::nullopt;
    const auto it = std::find(items.begin(), items.end(), item_key);
    if (it != items.end()) return std::make_pair(r, static_cast<size_t>(it - items.begin()));
    return std::make_pair(r, std::min(old_col, items.size() - 1));
  }
  return std::nullopt;
}

}  // namespace zerofin::ui
