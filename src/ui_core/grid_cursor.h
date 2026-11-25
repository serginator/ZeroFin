#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "ui_core/keys.h"

namespace zerofin::ui {

struct GridRow {
  size_t size = 0;     // cards in the row
  size_t visible = 1;  // cards shown at once
};

// Row and column of the selected card in rows of different lengths (carousels, poster grids).
// Up and Down keep a wanted column. Left and Right do not wrap. Each row keeps its own window,
// so a row shows the same cards when the cursor comes back to it.
class GridCursor {
 public:
  // Keeps the position when it still exists, else clamps it.
  void set_rows(std::vector<GridRow> rows);

  bool empty() const;
  size_t row() const { return row_; }
  size_t col() const { return col_; }
  // First visible column of `row`.
  size_t left(size_t row) const { return row < left_.size() ? left_[row] : 0; }

  // Up Down Left Right Home End PageUp PageDown. Returns true when the position changed.
  bool move(Key key);
  bool select(size_t row, size_t col);

 private:
  int next_row(int from, int direction) const;
  void go_to_row(size_t row);
  void reveal();

  std::vector<GridRow> rows_;
  std::vector<size_t> left_;
  size_t row_ = 0;
  size_t col_ = 0;
  size_t want_col_ = 0;
};

struct RowKeys {
  std::string key;
  std::vector<std::string> items;
};

// Where to put the cursor after a reload. The same row and item wins. Else the same row at
// min(old_col, size - 1). Else nullopt.
std::optional<std::pair<size_t, size_t>> find_position(const std::vector<RowKeys>& rows, const std::string& row_key,
                                                        const std::string& item_key, size_t old_col);

}  // namespace zerofin::ui
